#include "serial_video/gray2bw.hpp"
#include <thread>
#include <chrono>

/**
 * @brief Construct a new gray2bw::gray2bw object
 *
 * @param in_width 输入视频流的宽度
 * @param in_height 输入视频流的高度
 * @param out_width 输出视频流的宽度
 * @param out_height 输出视频流的高度
 */
gray2bw::gray2bw(int in_width, int in_height, int out_width, int out_height)
{
    if (in_width <= 0)
    {
        std::invalid_argument ex("in_width below 0!");
        throw ex;
    }
    if (in_height <= 0)
    {
        std::invalid_argument ex("in_height below 0!");
        throw ex;
    }
    if (out_width <= 0)
    {
        std::invalid_argument ex("out_width below 0!");
        throw ex;
    }
    if (out_height <= 0)
    {
        std::invalid_argument ex("out_height below 0!");
        throw ex;
    }

    // 保存参数
    this->m_in_width    = in_width;
    this->m_in_height   = in_height;
    this->m_out_width   = out_width;
    this->m_out_height  = out_height;
}

/**
 * @brief 将灰度视频流转换为列行式+抖动灰度的二值视频流
 *
 * @param in_stream 输入流
 * @param out_stream 输出流
 */
void gray2bw::convert(std::queue<uint8_t> &in_stream, std::queue<uint8_t> &out_stream)
{
    this->in_frame.create(cv::Size(this->m_in_width, this->m_in_height), CV_8UC1);    // 创建输入矩阵
    this->out_frame.create(cv::Size(this->m_out_width, this->m_out_height), CV_8UC1); // 创建空白输出矩阵
    while (!in_stream.empty())
    {
        if (in_stream.size() < this->m_in_width * this->m_in_height) // 输入队列不足一帧，但仍有数据
        {
            while (!in_stream.empty())
            {
                in_stream.pop(); // 全部丢弃
            }
            break;
        }
        for (int i = 0; i < this->m_in_width * this->m_in_height; i++)
        {
            this->in_frame.data[i] = in_stream.front(); // 输入矩阵
            in_stream.pop();
        }

        cv::Mat temp_frame;
        cv::resize(this->in_frame, temp_frame, cv::Size(this->m_out_width, this->m_out_height)); // 缩放至目标大小

        // 五档抖动
        for (int i = 0; i < this->m_out_height; i += 2)
        {
            for (int j = 0; j < this->m_out_width; j += 2)
            {
                uint8_t avg = (temp_frame.at<uint8_t>(i, j) + temp_frame.at<uint8_t>(i, j + 1) + temp_frame.at<uint8_t>(i + 1, j) + temp_frame.at<uint8_t>(i + 1, j + 1)) / 4;
                if (avg < 51)
                {
                    this->out_frame.at<uint8_t>(i, j)           = 0;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 0;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 0;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 0;
                }
                else if (avg < 102)
                {
                    this->out_frame.at<uint8_t>(i, j)           = 0;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 255;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 0;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 0;
                }
                else if (avg < 153)
                {
                    this->out_frame.at<uint8_t>(i, j)           = 0;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 255;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 255;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 0;
                }
                else if (avg < 204)
                {
                    this->out_frame.at<uint8_t>(i, j)           = 0;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 255;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 255;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 255;
                }
                else
                {
                    this->out_frame.at<uint8_t>(i, j)           = 255;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 255;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 255;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 255;
                }
            }
        }
        for (int page = 0; page < this->m_out_height; page += 8)
        {
            for (int col = 0; col < this->m_out_width; col++)
            {
                uint8_t data = 0;
                for (int i = 0; i < 8; i++)
                {
                    data >>= 1;
                    if (this->out_frame.data[col + (page + i) * this->m_out_width])
                        data |= 0x80;
                }
                out_stream.push(data);
            }
        }
    }
    in_frame.release();
    out_frame.release();
}

/**
 * @brief 将灰度视频流转换为列行式+抖动灰度的二值视频流（用于多线程）
 *
 * @param in_stream 输入流
 * @param in_lock 输入流的锁
 * @param out_stream 输出流
 * @param out_lock 输出流的锁
 * @param abort_flag 终止标志
 * @param process_done 运行完成标志
 */
void gray2bw::streamed_convert(std::queue<uint8_t> &in_stream, std::mutex &in_lock, std::queue<uint8_t> &out_stream, std::mutex &out_lock, std::atomic<int> &abort_flag, std::atomic<int> &process_done)
{
    this->in_frame.create(cv::Size(this->m_in_width, this->m_in_height), CV_8UC1);    // 创建输入矩阵
    this->out_frame.create(cv::Size(this->m_out_width, this->m_out_height), CV_8UC1); // 创建空白输出矩阵
    while (1)
    {
        while (1)
        {
            in_lock.lock();                                                                // 输入加锁
            if (abort_flag > 0 && in_stream.size() < this->m_in_height * this->m_in_width) // 已终止且剩余输入不足一帧
            {
                while (!in_stream.empty())
                {
                    in_stream.pop(); // 丢弃全部输入
                }
                in_lock.unlock(); // 解锁退出
                goto done;
            }
            if (in_stream.size() >= this->m_in_width * this->m_in_height)
                break;
            in_lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        for (int i = 0; i < this->m_in_width * this->m_in_height; i++)
        {
            this->in_frame.data[i] = in_stream.front(); // 输入矩阵
            in_stream.pop();
        }
        in_lock.unlock();

        cv::Mat temp_frame;
        cv::resize(this->in_frame, temp_frame, cv::Size(this->m_out_width, this->m_out_height)); // 缩放至目标大小

        // 五档抖动
        for (int i = 0; i < this->m_out_height; i += 2)
        {
            for (int j = 0; j < this->m_out_width; j += 2)
            {
                uint8_t avg = (temp_frame.at<uint8_t>(i, j) + temp_frame.at<uint8_t>(i, j + 1) + temp_frame.at<uint8_t>(i + 1, j) + temp_frame.at<uint8_t>(i + 1, j + 1)) / 4;
                if (avg < 51)
                {
                    this->out_frame.at<uint8_t>(i, j)           = 0;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 0;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 0;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 0;
                }
                else if (avg < 102)
                {
                    this->out_frame.at<uint8_t>(i, j)           = 0;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 255;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 0;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 0;
                }
                else if (avg < 153)
                {
                    this->out_frame.at<uint8_t>(i, j)           = 0;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 255;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 255;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 0;
                }
                else if (avg < 204)
                {
                    this->out_frame.at<uint8_t>(i, j)           = 0;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 255;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 255;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 255;
                }
                else
                {
                    this->out_frame.at<uint8_t>(i, j)           = 255;
                    this->out_frame.at<uint8_t>(i + 1, j)       = 255;
                    this->out_frame.at<uint8_t>(i, j + 1)       = 255;
                    this->out_frame.at<uint8_t>(i + 1, j + 1)   = 255;
                }
            }
        }

        // 等队列长度够短再输出
        while (1)
        {
            out_lock.lock();
            int length = out_stream.size();
            if (length < BW_QUEUE_LENGTH_MAX)
                break;
            out_lock.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        // 重新取模为列行式
        for (int page = 0; page < this->m_out_height; page += 8)
        {
            for (int col = 0; col < this->m_out_width; col++)
            {
                uint8_t data = 0;
                for (int i = 0; i < 8; i++)
                {
                    data >>= 1;
                    if (this->out_frame.data[col + (page + i) * this->m_out_width])
                        data |= 0x80;
                }
                out_stream.push(data);
            }
        }
        out_lock.unlock(); // 输出解锁
    }
    done:this->in_frame.release();
    this->out_frame.release();
    process_done = 1;
}
