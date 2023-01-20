#include "fft.hpp"
#include <thread>
#include <chrono>

/**
 * @brief Construct a new fft::fft object
 *
 * @param input_samplerate 输入数据采样率（单位：Hz）
 * @param output_samplerate 每秒输出频率数
 * @param threshold 阈值，当频谱中最大功率超过该阈值时才输出
 */
fft::fft(int input_samplerate, int output_samplerate, double threshold)
{
    if (input_samplerate <= 0)
    {
        std::invalid_argument ex("input_samplerate below 0!");
        throw ex;
    }
    if (output_samplerate <= 0)
    {
        std::invalid_argument ex("output_samplerate below 0!");
    }
    if (output_samplerate > input_samplerate)
    {
        std::invalid_argument ex("output_samplerate greater than input_samplerate!");
        throw ex;
    }

    // 保存参数
    this->input_samplerate  = input_samplerate;
    this->output_samplerate = output_samplerate;
    this->threshold         = threshold;
}

/**
 * @brief 计算峰值功率对应的频率
 *
 * @param input 输入队列
 * @param output 输出队列
 */
void fft::calculate(std::queue<uint16_t> &input, std::queue<uint8_t> &output)
{
    int length = input_samplerate / output_samplerate;                                       // 缓冲区长度
    double *input_array = (double *)fftw_malloc(length * sizeof(double));                    // 实输入数据
    fftw_complex *output_array = (fftw_complex *)fftw_malloc(length * sizeof(fftw_complex)); // 复输出数据
    fftw_plan p = fftw_plan_dft_r2c_1d(length, input_array, output_array, FFTW_MEASURE);     // 创建傅立叶变换计划
    while (!input.empty())
    {
        if (input.size() < length) // 当队列长度小于缓冲区
        {
            while (!input.empty())
            {
                input.pop(); // 丢弃所有数据
            }
            break;
        }
        for (int i = 0; i < length; i++) // 加载数据
        {
            input_array[i] = input.front();
            input.pop();
        }
        fftw_execute(p); // 执行变换
        int maxp = 0;
        double maxn = 0;
        for (int i = 1; i < length; i++) // 从1开始，去除直流分量
        {
            double current = sqrt(output_array[i][0] * output_array[i][0] + output_array[i][1] * output_array[i][1]); // 计算功率
            if (current > maxn && current > this->threshold)
            {
                maxn = current;
                maxp = i;
            }
        }
        int freq = 0;
        if (maxp > 0)
            freq = (maxp - 1) * this->input_samplerate / length; // 计算频率
        freq >>= 4;                                              // 除以16以匹配uint8_t的输出格式
        if (freq > 255)                                          // 剔除超过范围的结果
            output.push(0);
        else
            output.push(freq);
    }
    // 清理
    fftw_destroy_plan(p);
    fftw_free(input_array);
    fftw_free(output_array);
}

/**
 * @brief 用于多线程的流式计算峰值功率对应频率
 *
 * @param input 输入队列
 * @param input_lock 输入锁
 * @param output 输出队列
 * @param output_lock 输出锁
 * @param abort_flag 终止标志
 * @param process_done 运行完成标志
 */
void fft::streamed_calculate(std::queue<uint16_t> &input, std::mutex &input_lock, std::queue<uint8_t> &output, std::mutex &output_lock, std::atomic<int> &abort_flag, std::atomic<int> &process_done)
{
    int length = input_samplerate / output_samplerate;                                       // 缓冲区长度
    double *input_array = (double *)fftw_malloc(length * sizeof(double));                    // 实输入数据
    fftw_complex *output_array = (fftw_complex *)fftw_malloc(length * sizeof(fftw_complex)); // 复输出数据
    fftw_plan p = fftw_plan_dft_r2c_1d(length, input_array, output_array, FFTW_MEASURE);     // 创建傅立叶变换计划
    while (1)
    {
        while (1)
        {
            input_lock.lock();  // 输入加锁
            if (abort_flag > 0 && input.size() < length) // 已终止且剩余数据不足以填满缓冲区
            {
                while (!input.empty())
                {
                    input.pop(); // 丢弃所有数据
                }
                input_lock.unlock(); // 输入解锁
                goto done;               // 退出处理循环
            }
            if (input.size() >= length)
                break;
            input_lock.unlock();                                       // 输入解锁
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        for (int i = 0; i < length; i++)
        {
            input_array[i] = input.front();
            input.pop();
        }
        input_lock.unlock(); // 输入解锁
        fftw_execute(p);     // 执行变换
        int maxp = 0;
        double maxn = 0;
        for (int i = 1; i < length; i++) // 去除直流分量
        {
            double current = sqrt(output_array[i][0] * output_array[i][0] + output_array[i][1] * output_array[i][1]); // 计算功率
            if (current > maxn && current > this->threshold)
            {
                maxn = current;
                maxp = i;
            }
        }
        int freq = 0;
        if (maxp > 0)
            freq = (maxp - 1) * this->input_samplerate / length; // 计算频率
        freq >>= 4;                                              // 除以16以匹配uint8_t的输出格式
        while (1)
        {
            output_lock.lock();
            if (output.size() < FFT_QUEUE_LENGTH_MAX) // 如果有足够空间输出
                break;
            output_lock.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        if (freq > 255) // 剔除超过范围的结果
            output.push(0);
        else
            output.push(freq);
        output_lock.unlock(); // 输出解锁
    }
    // 清理
    done:fftw_destroy_plan(p);
    fftw_free(input_array);
    fftw_free(output_array);
    process_done = 1;
}