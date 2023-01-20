#include "avdecoder.hpp"

#include <iostream> // For debug message
#include <sys/stat.h>
#include <unistd.h>
#include <thread>
#include <chrono>

/**
 * @brief 解码异常类
 *
 */
class avdecoder_exception : public std::exception
{
public:
    avdecoder_exception()
    {
        this->info = "";
    }
    avdecoder_exception(const char *description)
    {
        this->info = description;
    }
    void set_info(const char *description)
    {
        this->info = description;
    }
    const char *what() const noexcept override
    {
        return this->info.c_str();
    }

private:
    std::string info;
};

/**
 * @brief Construct a new avdecoder::avdecoder object
 *
 * @param filename 文件名
 */
avdecoder::avdecoder(const char *filename)
{
    this->input_ctx             = NULL;
    this->video                 = NULL;
    this->audio                 = NULL;
    this->video_stream_index    = -1;
    this->audio_stream_index    = -1;
    this->video_decoder_ctx     = NULL;
    this->audio_decoder_ctx     = NULL;
    this->video_decoder         = NULL;
    this->audio_decoder         = NULL;
    this->hw_device_ctx         = NULL;
    this->video_hw_pix_fmt      = AV_PIX_FMT_NONE;
    this->filepath              = filename;
    // this->open(std::string(filename));
}

/**
 * @brief Construct a new avdecoder::avdecoder object
 *
 * @param filename 文件名
 */
avdecoder::avdecoder(std::string filename)
{
    this->input_ctx             = NULL;
    this->video                 = NULL;
    this->audio                 = NULL;
    this->video_stream_index    = -1;
    this->audio_stream_index    = -1;
    this->video_decoder_ctx     = NULL;
    this->audio_decoder_ctx     = NULL;
    this->video_decoder         = NULL;
    this->audio_decoder         = NULL;
    this->hw_device_ctx         = NULL;
    this->video_hw_pix_fmt      = AV_PIX_FMT_NONE;
    this->filepath              = filename;
    // this->open(filename);
}

/**
 * @brief Destroy the avdecoder::avdecoder object
 *
 */
avdecoder::~avdecoder()
{
    if (this->input_ctx != NULL)
    {
        avformat_close_input(&this->input_ctx);
        av_free(this->input_ctx);
    }
    if (this->video_decoder_ctx != NULL)
    {
        avcodec_close(this->video_decoder_ctx);
        avcodec_free_context(&this->video_decoder_ctx);
    }
    if (this->audio_decoder_ctx != NULL)
    {
        avcodec_close(this->audio_decoder_ctx);
        avcodec_free_context(&this->audio_decoder_ctx);
    }
}

/**
 * @brief 打开文件
 *
 */
void avdecoder::open()
{
    if (access(this->filepath.c_str(), R_OK))
    {
        avdecoder_exception ex("Not enough permission to read file!"); // 路径不可读
        throw ex;
    }
    struct stat statbuf;
    stat(this->filepath.c_str(), &statbuf);
    if (statbuf.st_mode & S_IFDIR)
    {
        avdecoder_exception ex("Path is a directory!"); // 给定路径为文件夹
        throw ex;
    }

    if (avformat_open_input(&this->input_ctx, this->filepath.c_str(), NULL, NULL) < 0)
    {
        avdecoder_exception ex("Unable to open stream!"); // 打开输入流失败
        throw ex;
    }

    if (avformat_find_stream_info(this->input_ctx, NULL) < 0) // 获取媒体信息
    {
        avdecoder_exception ex("Unable to determine stream format!"); // 获取失败
        throw ex;
    }

    this->video_stream_index = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &this->video_decoder, 0);
    this->audio_stream_index = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &this->audio_decoder, 0); // 获取音视频流
    if (this->video_stream_index >= 0)
        this->video = this->input_ctx->streams[this->video_stream_index]; // 获取视频流
    if (this->audio_stream_index >= 0)
        this->audio = this->input_ctx->streams[this->audio_stream_index]; // 获取音频流

    /*-----------视频解码器初始化部分----------*/
    /*尝试硬件解码*/
    if (this->video != NULL)
    {
        int hw_codec_configured = 0, hw_device = -1;

#ifdef ENABLE_HWACCEL
        AVHWDeviceType type = av_hwdevice_find_type_by_name(DEFAULT_AV_DECODER);
        if (type == AV_HWDEVICE_TYPE_NONE) // 默认解码器不受支持
        {
            type = av_hwdevice_iterate_types(type); // 更换解码器（反正不让它是NONE就行）
            std::cerr << "Warning: Default decoder unavailable" << std::endl;
        }
        while (!hw_codec_configured)
        {
            if (type == AV_HWDEVICE_TYPE_NONE)
            {
                break; // 完成遍历，没有合适硬件解码器
            }
            for (int i = 0;; i++)
            {
                const AVCodecHWConfig *config = avcodec_get_hw_config(this->video_decoder, i); // 遍历该硬件的所有解码器
                if (config == NULL)
                {
                    break; // 该硬件上没有合适配置
                }
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type)
                {
                    this->video_hw_pix_fmt = config->pix_fmt;
                    hw_codec_configured = 1;
                    break; // 成功找到并配置解码器，停止遍历
                }
            }
            if (!hw_codec_configured)
                type = av_hwdevice_iterate_types(type); // 遍历解码硬件
        }
#endif

        if (!hw_codec_configured)
        {
            this->video_decoder = avcodec_find_decoder(this->video->codecpar->codec_id); // 硬件解码器配置失败，使用软件解码
            std::cerr << "Unable to find a hardware video decoder or disabled hardware acceleration! Fallback to software decoder then." << std::endl;
        }

        if (this->video_decoder == NULL)
        {
            avdecoder_exception ex("Unable to find any video decoder!"); // 软硬解码器都没能匹配得上
            throw ex;
        }

        if ((this->video_decoder_ctx = avcodec_alloc_context3(this->video_decoder)) == NULL)
        {
            avdecoder_exception ex("Unable to allocate video decoder context!"); // 解码器上下文分配失败
            throw ex;
        }
        if (avcodec_parameters_to_context(this->video_decoder_ctx, this->video->codecpar) < 0)
        {
            avdecoder_exception ex("Unable to apply video parameters!"); // 解码器参数设置失败
            throw ex;
        }
        this->video_decoder_ctx->flags2         |= AV_CODEC_FLAG2_FAST; // 允许非规范加速
        this->video_decoder_ctx->opaque         = this;
        this->video_decoder_ctx->pkt_timebase   = this->video->time_base; // 设置时间
        this->video_decoder_ctx->thread_count   = DEFAULT_THREAD_NUM;     // 解码线程数

#ifdef ENABLE_HWACCEL
        if (hw_codec_configured)
        {
            this->video_decoder_ctx->get_format = avdecoder::get_hw_format;
            if (av_hwdevice_ctx_create(&this->hw_device_ctx, type, NULL, NULL, 0) < 0) // 创建硬件解码器上下文
            {
                avdecoder_exception ex("Unable to create specified HW device!");
                throw ex;
            }
            this->video_decoder_ctx->hw_device_ctx = av_buffer_ref(this->hw_device_ctx);
        }
#endif

        if (avcodec_open2(this->video_decoder_ctx, this->video_decoder, NULL) < 0) // 打开解码器
        {
            avdecoder_exception ex("Unable to open video decoder!");
            throw ex;
        }
    }

    /*----------音频解码器初始化部分----------*/
    if (this->audio != NULL)
    {
        this->audio_decoder = avcodec_find_decoder(this->audio->codecpar->codec_id); // 音频全部软件解码

        if (this->audio_decoder == NULL)
        {
            avdecoder_exception ex("Unable to find any audio decoder!"); // 软硬解码器都没能匹配得上
            throw ex;
        }

        if ((this->audio_decoder_ctx = avcodec_alloc_context3(this->audio_decoder)) == NULL)
        {
            avdecoder_exception ex("Unable to allocate audio decoder context!"); // 解码器上下文分配失败
            throw ex;
        }
        if (avcodec_parameters_to_context(this->audio_decoder_ctx, this->audio->codecpar) < 0)
        {
            avdecoder_exception ex("Unable to apply audio parameters!"); // 解码器参数设置失败
            throw ex;
        }
        this->audio_decoder_ctx->flags2         |= AV_CODEC_FLAG2_FAST; // 允许非规范加速
        this->audio_decoder_ctx->opaque         = this;
        this->audio_decoder_ctx->pkt_timebase   = this->audio->time_base;            // 设置时间
        if (avcodec_open2(this->audio_decoder_ctx, this->audio_decoder, NULL) < 0) // 打开解码器
        {
            avdecoder_exception ex("Unable to open audio decoder!");
            throw ex;
        }
    }
}

/**
 * @brief 解码
 *
 * @param video_frame 用于保存视频帧的队列
 * @param audio_pcm 用于保存音频帧的队列
 */
void avdecoder::decode(std::queue<uint8_t> &video_frame, std::queue<uint16_t> &audio_pcm)
{
    avdecoder_exception ex;                                    // 异常信息
    AVChannelLayout audio_out_layout = AV_CHANNEL_LAYOUT_MONO; // 单声道输出
    AVSampleFormat audio_out_sample_fmt = AV_SAMPLE_FMT_S16;   // 16bit
    AVPacket *pkt = av_packet_alloc();                         // 分配数据包
    SwrContext *audio_swr_ctx = swr_alloc();                   // 音频重采样上下文
    // 像素格式转换器上下文，转换为8位灰度
    SwsContext *video_sws_ctx = sws_getContext(this->video_decoder_ctx->width, this->video_decoder_ctx->height, this->video_decoder_ctx->pix_fmt, this->video_decoder_ctx->width, this->video_decoder_ctx->height, AV_PIX_FMT_GRAY8, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    // 分配帧
    AVFrame *frame      = av_frame_alloc();
    AVFrame *sw_frame   = av_frame_alloc();
    AVFrame *gray_frame = av_frame_alloc();
    AVFrame *pcm        = av_frame_alloc();
    uint16_t *audio_buffer  = (uint16_t *)av_malloc(this->audio_decoder_ctx->sample_rate);                                                                          // 分配音频缓冲区
    uint8_t *video_buffer   = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_GRAY8, this->video_decoder_ctx->width, this->video_decoder_ctx->height, 1)); // 分配视频缓冲区

    if (pkt == NULL)
    {
        ex.set_info("Unable to allocate packet!");
        goto fail;
    }
    if (audio_swr_ctx == NULL)
    {
        ex.set_info("Unable to allocate audio resampler!");
        goto fail;
    }
    if (video_sws_ctx == NULL)
    {
        ex.set_info("Unable to allocate video scaler!");
        goto fail;
    }
    if (audio_buffer == NULL)
    {
        ex.set_info("Unable to allocate audio buffer!");
        goto fail;
    }
    if (video_buffer == NULL)
    {
        ex.set_info("Unable to allocate video buffer!");
        goto fail;
    }
    if (frame == NULL)
    {
        ex.set_info("Unable to allocate HW frame!");
        goto fail;
    }
    if (sw_frame == NULL)
    {
        ex.set_info("Unable to allocate SW frame!");
        goto fail;
    }
    if (gray_frame == NULL)
    {
        ex.set_info("Unable to allocate gray frame!");
        goto fail;
    }
    if (pcm == NULL)
    {
        ex.set_info("Unable to allocate audio frame!");
        goto fail;
    }
    if (av_image_fill_arrays(gray_frame->data, gray_frame->linesize, video_buffer, AV_PIX_FMT_GRAY8, this->video_decoder_ctx->width, this->video_decoder_ctx->height, 1) < 0) // 向灰度帧应用自己分配的缓冲区
    {
        ex.set_info("Unable to fill image array!");
        goto fail;
    }

    // 重采样为16位整数单声道PCM（采样率不变）
    if (swr_alloc_set_opts2(&audio_swr_ctx, &audio_out_layout, audio_out_sample_fmt, this->audio_decoder_ctx->sample_rate, &this->audio_decoder_ctx->ch_layout, this->audio_decoder_ctx->sample_fmt, this->audio_decoder_ctx->sample_rate, 0, NULL) < 0)
    {
        ex.set_info("Unable to setup audio resampler!");
        goto fail;
    }
    if (swr_init(audio_swr_ctx) < 0) // 初始化重采样上下文
    {
        ex.set_info("Unable to initalize audio resampler!");
        goto fail;
    }

    while (av_read_frame(this->input_ctx, pkt) >= 0) // 读出数据包
    {
        if (this->video_decoder_ctx != NULL && pkt->stream_index == this->video_stream_index) // 如果配置过视频解码器且该数据包属于视频流
        {
            if (avcodec_send_packet(this->video_decoder_ctx, pkt) < 0) // 发送数据包到视频解码器
            {
                ex.set_info("Unable to send packet to video decoder!");
                goto fail;
            }
            while (1)
            {
                int video_rec_ret = avcodec_receive_frame(this->video_decoder_ctx, frame); // 接收帧

                if (video_rec_ret == AVERROR(EAGAIN) || video_rec_ret == AVERROR_EOF)
                {
                    break;
                }
                else if (video_rec_ret < 0)
                {
                    ex.set_info("Unable to receive frame from video decoder!");
                    goto fail;
                }
                if (frame->format == this->video_hw_pix_fmt) // 确实是硬件帧
                {
                    if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) // 从硬件接收帧数据
                    {
                        ex.set_info("Unable to receive data from HW frame!");
                        goto fail;
                    }
                    if (sws_scale(video_sws_ctx, sw_frame->data, sw_frame->linesize, 0, this->video_decoder_ctx->height, gray_frame->data, gray_frame->linesize) < 0) // 转换格式
                    {
                        ex.set_info("Unable to convert pix format!");
                        goto fail;
                    }
                }
                else // 本来就是软件帧
                {
                    if (sws_scale(video_sws_ctx, frame->data, frame->linesize, 0, this->video_decoder_ctx->height, gray_frame->data, gray_frame->linesize) < 0) // 转换格式
                    {
                        ex.set_info("Unable to convert pix format!");
                        goto fail;
                    }
                }
                for (int i = 0; i < this->video_decoder_ctx->width * this->video_decoder_ctx->height; i++)
                {
                    video_frame.push(gray_frame->data[0][i]);
                }
            }
        }
        else if (this->audio_decoder_ctx != NULL && pkt->stream_index == this->audio_stream_index) // 如果配置过音频解码器且该数据包属于音频流
        {
            if (avcodec_send_packet(this->audio_decoder_ctx, pkt) < 0) // 向音频解码器发送数据包
            {
                ex.set_info("Unable to send packet to audio decoder!");
                goto fail;
            }
            while (1)
            {
                int audio_rec_ret = avcodec_receive_frame(this->audio_decoder_ctx, pcm); // 从音频解码器接收帧
                if (audio_rec_ret == AVERROR(EAGAIN) || audio_rec_ret == AVERROR_EOF)
                {
                    break;
                }
                else if (audio_rec_ret < 0)
                {
                    ex.set_info("Unable to receive pcm from audio decoder!");
                    goto fail;
                }

                swr_convert(audio_swr_ctx, (uint8_t **)&audio_buffer, this->audio_decoder_ctx->sample_rate, (const uint8_t **)pcm->data, pcm->nb_samples); // 重采样
                for (int i = 0; i < pcm->nb_samples; i++)
                {
                    audio_pcm.push(audio_buffer[i]); // 导出音频
                }
            }
        }
        av_packet_unref(pkt);
    }

    if (pkt)
        av_packet_free(&pkt);
    if (audio_swr_ctx)
        swr_free(&audio_swr_ctx);
    if (video_sws_ctx)
        sws_freeContext(video_sws_ctx);
    if (frame)
        av_frame_free(&frame);
    if (sw_frame)
        av_frame_free(&sw_frame);
    if (gray_frame)
        av_frame_free(&gray_frame);
    if (pcm)
        av_frame_free(&pcm);
    if (audio_buffer)
        av_free(audio_buffer);
    if (video_buffer)
        av_free(video_buffer);
    return;

fail:
    if (pkt)
        av_packet_free(&pkt);
    if (audio_swr_ctx)
        swr_free(&audio_swr_ctx);
    if (video_sws_ctx)
        sws_freeContext(video_sws_ctx);
    if (frame)
        av_frame_free(&frame);
    if (sw_frame)
        av_frame_free(&sw_frame);
    if (gray_frame)
        av_frame_free(&gray_frame);
    if (pcm)
        av_frame_free(&pcm);
    if (audio_buffer)
        av_free(audio_buffer);
    if (video_buffer)
        av_free(video_buffer);
    throw ex;
}

/**
 * @brief 用于多线程的流式解码
 *
 * @param video_frame 用于保存视频帧的队列
 * @param video_lock 视频帧队列的锁
 * @param audio_pcm 用于保存音频帧的队列
 * @param audio_lock 音频帧队列的锁
 * @param abort_flag 终止标志，终止后置1，也可由外界置1停止其运行
 */
void avdecoder::streamed_decode(std::queue<uint8_t> &video_frame, std::mutex &video_lock, std::queue<uint16_t> &audio_pcm, std::mutex &audio_lock, std::atomic<int> &abort_flag)
{
    avdecoder_exception ex;                                    // 异常信息
    AVChannelLayout audio_out_layout = AV_CHANNEL_LAYOUT_MONO; // 单声道输出
    AVSampleFormat audio_out_sample_fmt = AV_SAMPLE_FMT_S16;   // 16bit
    AVPacket *pkt = av_packet_alloc();                         // 分配数据包
    SwrContext *audio_swr_ctx = swr_alloc();                   // 音频重采样上下文
    // 像素格式转换器上下文，转换为8位灰度
    SwsContext *video_sws_ctx = sws_getContext(this->video_decoder_ctx->width, this->video_decoder_ctx->height, this->video_decoder_ctx->pix_fmt, this->video_decoder_ctx->width, this->video_decoder_ctx->height, AV_PIX_FMT_GRAY8, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    // 分配帧
    AVFrame *frame      = av_frame_alloc();
    AVFrame *sw_frame   = av_frame_alloc();
    AVFrame *gray_frame = av_frame_alloc();
    AVFrame *pcm        = av_frame_alloc();
    uint16_t *audio_buffer  = (uint16_t *)av_malloc(this->audio_decoder_ctx->sample_rate);                                                                          // 分配音频缓冲区
    uint8_t *video_buffer   = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_GRAY8, this->video_decoder_ctx->width, this->video_decoder_ctx->height, 1)); // 分配视频缓冲区

    if (pkt == NULL)
    {
        ex.set_info("Unable to allocate packet!");
        goto fail;
    }
    if (audio_swr_ctx == NULL)
    {
        ex.set_info("Unable to allocate audio resampler!");
        goto fail;
    }
    if (video_sws_ctx == NULL)
    {
        ex.set_info("Unable to allocate video scaler!");
        goto fail;
    }
    if (audio_buffer == NULL)
    {
        ex.set_info("Unable to allocate audio buffer!");
        goto fail;
    }
    if (video_buffer == NULL)
    {
        ex.set_info("Unable to allocate video buffer!");
        goto fail;
    }
    if (frame == NULL)
    {
        ex.set_info("Unable to allocate HW frame!");
        goto fail;
    }
    if (sw_frame == NULL)
    {
        ex.set_info("Unable to allocate SW frame!");
        goto fail;
    }
    if (gray_frame == NULL)
    {
        ex.set_info("Unable to allocate gray frame!");
        goto fail;
    }
    if (pcm == NULL)
    {
        ex.set_info("Unable to allocate audio frame!");
        goto fail;
    }
    if (av_image_fill_arrays(gray_frame->data, gray_frame->linesize, video_buffer, AV_PIX_FMT_GRAY8, this->video_decoder_ctx->width, this->video_decoder_ctx->height, 1) < 0) // 向灰度帧应用自己分配的缓冲区
    {
        ex.set_info("Unable to fill image array!");
        goto fail;
    }

    // 重采样为16位整数单声道PCM（采样率不变）
    if (swr_alloc_set_opts2(&audio_swr_ctx, &audio_out_layout, audio_out_sample_fmt, this->audio_decoder_ctx->sample_rate, &this->audio_decoder_ctx->ch_layout, this->audio_decoder_ctx->sample_fmt, this->audio_decoder_ctx->sample_rate, 0, NULL) < 0)
    {
        ex.set_info("Unable to setup audio resampler!");
        goto fail;
    }
    if (swr_init(audio_swr_ctx) < 0) // 初始化重采样上下文
    {
        ex.set_info("Unable to initalize audio resampler!");
        goto fail;
    }

    while (abort_flag == 0 && av_read_frame(this->input_ctx, pkt) >= 0) // 读出数据包
    {
        if (this->video_decoder_ctx != NULL && pkt->stream_index == this->video_stream_index) // 如果配置过视频解码器且该数据包属于视频流
        {
            if (avcodec_send_packet(this->video_decoder_ctx, pkt) < 0) // 发送数据包到视频解码器
            {
                ex.set_info("Unable to send packet to video decoder!");
                goto fail;
            }
            while (1)
            {
                int video_rec_ret = avcodec_receive_frame(this->video_decoder_ctx, frame); // 接收帧

                if (video_rec_ret == AVERROR(EAGAIN) || video_rec_ret == AVERROR_EOF)
                {
                    break;
                }
                else if (video_rec_ret < 0)
                {
                    ex.set_info("Unable to receive frame from video decoder!");
                    goto fail;
                }
                if (frame->format == this->video_hw_pix_fmt) // 确实是硬件帧
                {
                    if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) // 从硬件接收帧数据
                    {
                        ex.set_info("Unable to receive data from HW frame!");
                        goto fail;
                    }
                    if (sws_scale(video_sws_ctx, sw_frame->data, sw_frame->linesize, 0, this->video_decoder_ctx->height, gray_frame->data, gray_frame->linesize) < 0) // 转换格式
                    {
                        ex.set_info("Unable to convert pix format!");
                        goto fail;
                    }
                }
                else // 本来就是软件帧
                {
                    if (sws_scale(video_sws_ctx, frame->data, frame->linesize, 0, this->video_decoder_ctx->height, gray_frame->data, gray_frame->linesize) < 0) // 转换格式
                    {
                        ex.set_info("Unable to convert pix format!");
                        goto fail;
                    }
                }
                while (1)
                {
                    video_lock.lock();
                    int s = video_frame.size(); // 先加锁，再获取队列长度
                    video_lock.unlock();
                    if ((s * sizeof(uint8_t)) < VIDEO_QUEUE_LENGTH_MAX)
                        break; // 队列足够短，开始向队列写入
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
                video_lock.lock();
                for (int i = 0; i < this->video_decoder_ctx->width * this->video_decoder_ctx->height; i++)
                {
                    video_frame.push(gray_frame->data[0][i]); // 写入
                }
                video_lock.unlock();
            }
        }
        else if (this->audio_decoder_ctx != NULL && pkt->stream_index == this->audio_stream_index) // 如果配置过音频解码器且该数据包属于音频流
        {
            if (avcodec_send_packet(this->audio_decoder_ctx, pkt) < 0) // 向音频解码器发送数据包
            {
                ex.set_info("Unable to send packet to audio decoder!");
                goto fail;
            }
            while (1)
            {
                int audio_rec_ret = avcodec_receive_frame(this->audio_decoder_ctx, pcm); // 从音频解码器接收帧
                if (audio_rec_ret == AVERROR(EAGAIN) || audio_rec_ret == AVERROR_EOF)
                {
                    break;
                }
                else if (audio_rec_ret < 0)
                {
                    ex.set_info("Unable to receive pcm from audio decoder!");
                    goto fail;
                }

                swr_convert(audio_swr_ctx, (uint8_t **)&audio_buffer, this->audio_decoder_ctx->sample_rate, (const uint8_t **)pcm->data, pcm->nb_samples); // 重采样
                while (1)
                {
                    audio_lock.lock();
                    int s = audio_pcm.size(); // 先加锁，再获取队列长度
                    audio_lock.unlock();
                    if ((s * sizeof(uint16_t)) < AUDIO_QUEUE_LENGTH_MAX)
                        break; // 队列足够短，开始向队列写入
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                audio_lock.lock();
                for (int i = 0; i < pcm->nb_samples; i++)
                {
                    audio_pcm.push(audio_buffer[i]); // 导出音频
                }
                audio_lock.unlock();
            }
        }
        av_packet_unref(pkt);
    }

    if (pkt)
        av_packet_free(&pkt);
    if (audio_swr_ctx)
        swr_free(&audio_swr_ctx);
    if (video_sws_ctx)
        sws_freeContext(video_sws_ctx);
    if (frame)
        av_frame_free(&frame);
    if (sw_frame)
        av_frame_free(&sw_frame);
    if (gray_frame)
        av_frame_free(&gray_frame);
    if (pcm)
        av_frame_free(&pcm);
    if (audio_buffer)
        av_free(audio_buffer);
    if (video_buffer)
        av_free(video_buffer);
    abort_flag = 1;
    return;

fail:
    if (pkt)
        av_packet_free(&pkt);
    if (audio_swr_ctx)
        swr_free(&audio_swr_ctx);
    if (video_sws_ctx)
        sws_freeContext(video_sws_ctx);
    if (frame)
        av_frame_free(&frame);
    if (sw_frame)
        av_frame_free(&sw_frame);
    if (gray_frame)
        av_frame_free(&gray_frame);
    if (pcm)
        av_frame_free(&pcm);
    if (audio_buffer)
        av_free(audio_buffer);
    if (video_buffer)
        av_free(video_buffer);
    throw ex;
}

/**
 * @brief 获取当前视频流帧率
 *
 * @return double 视频流有效时返回帧率，无效则返回-1
 */
double avdecoder::get_video_framerate(void)
{
    if (this->video != NULL)
        return (double)this->video->avg_frame_rate.num / this->video->avg_frame_rate.den;
    return -1;
}

/**
 * @brief 获取当前音频流采样率
 *
 * @return double 音频流有效时返回采样率，无效则返回-1
 */
double avdecoder::get_audio_samplerate(void)
{
    if (this->audio != NULL)
        return (double)this->audio_decoder_ctx->sample_rate;
    return -1;
}

/**
 * @brief 获取当前视频帧宽度
 *
 * @return int 视频流有效时返回以像素为单位的宽度，无效则返回-1
 */
int avdecoder::get_video_width(void)
{
    if (this->video_decoder_ctx != NULL)
        return this->video_decoder_ctx->width;
    return -1;
}

/**
 * @brief 获取当前视频帧高度
 *
 * @return int 视频流有效时返回以像素为单位的高度，无效则返回-1
 */
int avdecoder::get_video_height(void)
{
    if (this->video_decoder_ctx != NULL)
        return this->video_decoder_ctx->height;
    return -1;
}

/**
 * @brief 获取像素格式（私有静态方法）
 *
 * @param ctx 解码器上下文
 * @param pix_fmts 全部像素格式列表
 * @return AVPixelFormat 与硬件解码器想匹配的像素格式
 */
AVPixelFormat avdecoder::get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const AVPixelFormat *p;
    avdecoder *pThis = (avdecoder *)ctx->opaque;
    for (p = pix_fmts; *p != -1; p++)
    {
        if (*p == pThis->video_hw_pix_fmt)
            return *p;
    }
    return AV_PIX_FMT_NONE;
}