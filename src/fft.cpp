#include "serial_video/fft.hpp"
#include <thread>
#include <chrono>


/**
 * @brief 傅里叶变换数据处理上下文
 * 
 */
class fft_calc_ctx
{
public:

    /**
     * @brief Construct a new fft calc ctx object
     * 
     * @param array_length 计算时的数据长度
     */
    fft_calc_ctx(uint32_t array_length)
    {
        this->array_length  = array_length;
        this->input_array   = (double *)fftw_malloc(array_length * sizeof(double));                                     // 实输入数据
        this->output_array  = (fftw_complex *)fftw_malloc(array_length * sizeof(fftw_complex));                         // 复输出数据
        this->plan          = fftw_plan_dft_r2c_1d(array_length, this->input_array, this->output_array, FFTW_MEASURE);  // 创建傅立叶变换计划
    }

    /**
     * @brief Destroy the fft_calc_ctx object
     * 
     */
    ~fft_calc_ctx()
    {
        if (this->plan != NULL)
            fftw_destroy_plan(this->plan);

        if (this->input_array != NULL)
            fftw_free(input_array);

        if (this->output_array != NULL)
            fftw_free(output_array);
    }

    /**
     * @brief 加载输入数据
     * 
     * @param input PCM输入队列
     */
    void feed_input_array(std::queue<uint16_t> &input)
    {
        for (int i = 0; i < this->array_length; i++)
        {
            input_array[i] = input.front();
            input.pop();
        }
    }

    /**
     * @brief 执行傅里叶变换
     * 
     */
    void calc(void)
    {
        fftw_execute(this->plan);
    }

    /**
     * @brief 输出f0（实际频率除以16后的数据）（未来将改为输出音阶）
     * 
     * @param output 输出队列
     * @param input_samplerate 输入源的采样率，用于计算实际频率
     * @param threshold 功率阈值，超过该阈值才输出（未来将改为置信度）
     */
    void get_f0(std::queue<uint8_t> &output, uint32_t input_samplerate, double threshold)
    {
        int maxp = 0;
        double maxn = 0;
        for (int i = 1; i < this->array_length; i++) // 从1开始，去除直流分量
        {
            double current = sqrt(this->output_array[i][0] * this->output_array[i][0] + this->output_array[i][1] * this->output_array[i][1]); // 计算功率
            if (current > maxn && current > threshold)
            {
                maxn = current;
                maxp = i;
            }
        }
        int freq = 0;
        if (maxp > 0)
            freq = (maxp - 1) * input_samplerate / this->array_length; // 计算频率
        freq >>= 4;                                              // 除以16以匹配uint8_t的输出格式
        if (freq > 255)                                          // 剔除超过范围的结果
            output.push(0);
        else
            output.push(freq);
    }

private:
    uint32_t array_length;
    double *input_array = NULL;
    fftw_complex *output_array = NULL;
    fftw_plan plan = NULL;
};

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
    uint32_t length = input_samplerate / output_samplerate; // 缓冲区长度
    fft_calc_ctx ctx(length); // 创建计算上下文

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
        ctx.feed_input_array(input);
        ctx.calc(); // 执行变换
        ctx.get_f0(output, input_samplerate, this->threshold);
    }
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
    uint32_t length = input_samplerate / output_samplerate; // 缓冲区长度
    fft_calc_ctx ctx(length); // 创建计算上下文

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
                process_done = 1; // 运行完成
                return;
            }
            if (input.size() >= length)
                break;
            input_lock.unlock();                                       // 输入解锁
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ctx.feed_input_array(input);
        input_lock.unlock(); // 输入解锁

        ctx.calc(); // 执行变换

        while (1)
        {
            output_lock.lock();
            if (output.size() < FFT_QUEUE_LENGTH_MAX) // 如果有足够空间输出
                break;
            output_lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ctx.get_f0(output, this->input_samplerate, this->threshold);
        output_lock.unlock(); // 输出解锁
    }
    process_done = 1;
}
