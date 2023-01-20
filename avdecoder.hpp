#ifndef __AVDECODER_HPP__
#define __AVDECODER_HPP__

// #define ENABLE_HWACCEL //先不启用硬解，有bug，比多线程软解还慢
#define DEFAULT_AV_DECODER "vaapi"                 // 默认vaapi解码器，可按需修改
#define DEFAULT_THREAD_NUM 4                       // 软解默认4线程
#define VIDEO_QUEUE_LENGTH_MAX (1024 * 1024 * 100) // 视频队列长度最大100MiB
#define AUDIO_QUEUE_LENGTH_MAX (1024 * 10)         // 音频队列长度最大20KiB（10240 * sizeof(uint16_t)）

#include <string>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <queue>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
};

/**
 * @brief 视频解码类（将视频解码为灰度视频帧和单声道16bit音频）
 *
 */
class avdecoder
{
public:
    avdecoder(const char *filename);
    avdecoder(std::string filename);
    void open();
    ~avdecoder();
    double get_video_framerate(void);
    double get_audio_samplerate(void);
    int get_video_width(void);
    int get_video_height(void);

    void decode(std::queue<uint8_t> &video_frame, std::queue<uint16_t> &audio_pcm);
    void streamed_decode(std::queue<uint8_t> &video_frame, std::mutex &video_lock, std::queue<uint16_t> &audio_pcm, std::mutex &audio_lock, std::atomic<int> &abort_flag);

private:
    std::string filepath;
    AVFormatContext *input_ctx;
    AVStream *video, *audio;
    int video_stream_index, audio_stream_index;
    AVPixelFormat video_hw_pix_fmt;
    AVCodecContext *video_decoder_ctx, *audio_decoder_ctx;
    AVBufferRef *hw_device_ctx;
    const AVCodec *video_decoder, *audio_decoder;
    static AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);
};

#endif