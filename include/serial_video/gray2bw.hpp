#ifndef __GRAY2BW_HPP__
#define __GRAY2BW_HPP__

#include <opencv2/opencv.hpp>
#include <cstdint>
#include <mutex>
#include <queue>
#include <atomic>
#define BW_QUEUE_LENGTH_MAX (1024 * 100) // 队列长度最大100KiB

/**
 * @brief 灰度转抖动后的二值图像
 *
 */
class gray2bw
{
public:
    gray2bw(int in_width, int in_height, int out_width, int out_height);
    void convert(std::queue<uint8_t> &in_stream, std::queue<uint8_t> &out_stream);
    void streamed_convert(std::queue<uint8_t> &in_stream, std::mutex &in_lock, std::queue<uint8_t> &out_stream, std::mutex &out_lock, std::atomic<int> &abort_flag, std::atomic<int> &process_done);

private:
    cv::Mat in_frame, out_frame;
    int m_in_width, m_in_height, m_out_width, m_out_height;
};

#endif