#ifndef __FFT_HPP__
#define __FFT_HPP__

#include <ccomplex>
#include <fftw3.h>
#include <queue>
#include <mutex>
#include <atomic>

#define FFT_QUEUE_LENGTH_MAX 10240 // 队列长度最大10KiB

/**
 * @brief 音频快速傅立叶变换，取功率最大的频率
 * 
 */
class fft
{
public:
    fft(int input_samplerate, int output_samplerate, double threshold);
    void calculate(std::queue<uint16_t> &input, std::queue<uint8_t> &output);
    void streamed_calculate(std::queue<uint16_t> &input, std::mutex &input_lock, std::queue<uint8_t> &output, std::mutex &output_lock, std::atomic<int> &abort_flag, std::atomic<int> &process_done);

private:
    int input_samplerate, output_samplerate;
    double threshold;
};

#endif