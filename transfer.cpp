#include "transfer.hpp"

#include <thread>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <fstream> //for std::ios_base::failure

/**
 * @brief Construct a new transfer::transfer object
 *
 * @param device 串口设备路径
 * @param baudrate 波特率
 * @param framerate 视频帧率
 * @param frame_size 视频帧大小
 * @param audio_size 音频帧大小
 */
transfer::transfer(const char *device, int baudrate, int framerate, int frame_size, int audio_size)
{
    this->device_path   = device;
    this->frame_size    = frame_size;
    this->audio_size    = audio_size;
    this->framerate     = framerate;
    switch (baudrate)
    {
    case 50:
        this->baudrate = B50;
        break;
    case 75:
        this->baudrate = B75;
        break;
    case 110:
        this->baudrate = B110;
        break;
    case 134:
        this->baudrate = B134;
        break;
    case 150:
        this->baudrate = B150;
        break;
    case 200:
        this->baudrate = B200;
        break;
    case 300:
        this->baudrate = B300;
        break;
    case 600:
        this->baudrate = B600;
        break;
    case 1200:
        this->baudrate = B1200;
        break;
    case 1800:
        this->baudrate = B1800;
        break;
    case 2400:
        this->baudrate = B2400;
        break;
    case 4800:
        this->baudrate = B4800;
        break;
    case 9600:
        this->baudrate = B9600;
        break;
    case 19200:
        this->baudrate = B19200;
        break;
    case 38400:
        this->baudrate = B38400;
        break;
    case 57600:
        this->baudrate = B57600;
        break;
    case 115200:
        this->baudrate = B115200;
        break;
    case 230400:
        this->baudrate = B230400;
        break;
    case 460800:
        this->baudrate = B460800;
        break;
    case 500000:
        this->baudrate = B500000;
        break;
    case 576000:
        this->baudrate = B576000;
        break;
    case 921600:
        this->baudrate = B921600;
        break;
    case 1000000:
        this->baudrate = B1000000;
        break;
    case 1152000:
        this->baudrate = B1152000;
        break;
    case 1500000:
        this->baudrate = B1500000;
        break;
    case 2000000:
        this->baudrate = B2000000;
        break;
    case 2500000:
        this->baudrate = B2500000;
        break;
    case 3000000:
        this->baudrate = B3000000;
        break;
    case 3500000:
        this->baudrate = B3500000;
        break;
    case 4000000:
        this->baudrate = B4000000;
        break;
    default:
        this->baudrate = B115200;
    }
}

/**
 * @brief 启动传输
 *
 * @param video 视频帧队列
 * @param audio 音频帧队列
 */
void transfer::start(std::queue<uint8_t> &video, std::queue<uint8_t> &audio)
{
    struct termios serial_cfg;

    int fd = open(device_path.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        std::ios_base::failure ex("Unable to open serial port!");
        throw ex;
    }

    if (tcgetattr(fd, &serial_cfg) == -1)
    {
        std::ios_base::failure ex("Unable to get serial configuration!");
        close(fd);
        throw ex;
    }

    cfmakeraw(&serial_cfg);
    cfsetspeed(&serial_cfg, this->baudrate);
    serial_cfg.c_cflag |= CLOCAL;  // 本地模式，忽略控制线
    serial_cfg.c_cflag &= ~CREAD;  // 禁用读取
    serial_cfg.c_cflag &= ~CSTOPB; // 一位停止位
    serial_cfg.c_cflag &= ~CSIZE;  // 清除数据位设置
    serial_cfg.c_cflag |= CS8;     // 8位数据
    serial_cfg.c_cflag &= ~PARENB; // 无校验

    tcflush(fd, TCIOFLUSH);
    serial_cfg.c_cc[VTIME] = 0;
    serial_cfg.c_cc[VMIN] = 0; // 非阻塞读取（其实无所谓了，反正不读取）
    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &serial_cfg) == -1)
    {
        std::ios_base::failure ex("Unable to apply serial configuration");
        close(fd);
        throw ex;
    }
    char *buffer = new char[this->frame_size + this->audio_size];
    while (!video.empty() || !audio.empty())
    {
        auto wakeup_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000 / this->framerate);
        if (video.size() < this->frame_size || audio.size() < this->audio_size) // 剩余数据已不足以组成一个数据包
        {                                                                       // 丢弃所有数据
            while (!video.empty())
            {
                video.pop();
            }
            while (!audio.empty())
            {
                audio.pop();
            }
            break;
        }
        for (int i = 0; i < this->frame_size; i++) // 读取一帧视频到缓冲区
        {
            buffer[i] = video.front();
            video.pop();
        }
        for (int i = 0; i < this->audio_size; i++) // 读取一帧音频到缓冲区
        {
            buffer[this->frame_size + i] = audio.front();
            audio.pop();
        }
        write(fd, buffer, this->frame_size + this->audio_size); // 向串口写入
        std::this_thread::sleep_until(wakeup_time);             // 一小段休眠，以保证帧率准确
    }
    delete[] buffer;
    close(fd);
}

/**
 * @brief 用于多线程的启动流式传输
 *
 * @param video 视频帧队列
 * @param vlock 视频队列锁
 * @param audio 音频帧队列
 * @param alock 音频队列锁
 * @param abort_flag 结束标志，置1后结束
 */
void transfer::streamed_start(std::queue<uint8_t> &video, std::mutex &vlock, std::queue<uint8_t> &audio, std::mutex &alock, std::atomic<int> &video_abort_flag, std::atomic<int> &audio_abort_flag)
{
    struct termios serial_cfg;

    int fd = open(device_path.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        std::ios_base::failure ex("Unable to open serial port!");
        throw ex;
    }

    if (tcgetattr(fd, &serial_cfg) == -1)
    {
        std::ios_base::failure ex("Unable to get serial configuration!");
        close(fd);
        throw ex;
    }

    cfmakeraw(&serial_cfg);
    cfsetspeed(&serial_cfg, this->baudrate);
    serial_cfg.c_cflag |= CLOCAL;  // 本地模式，忽略控制线
    serial_cfg.c_cflag &= ~CREAD;  // 禁用读取
    serial_cfg.c_cflag &= ~CSTOPB; // 一位停止位
    serial_cfg.c_cflag &= ~CSIZE;  // 清除数据位设置
    serial_cfg.c_cflag |= CS8;     // 8位数据
    serial_cfg.c_cflag &= ~PARENB; // 无校验

    tcflush(fd, TCIOFLUSH);
    serial_cfg.c_cc[VTIME] = 0;
    serial_cfg.c_cc[VMIN] = 0; // 非阻塞读取（其实无所谓了，反正不读取）
    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &serial_cfg) == -1)
    {
        std::ios_base::failure ex("Unable to apply serial configuration");
        close(fd);
        throw ex;
    }
    char *buffer = new char[this->frame_size + this->audio_size];
    while (1)
    {
        auto wakeup_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000 / this->framerate);
        size_t vsize, asize;
        vlock.lock();
        vsize = video.size();
        vlock.unlock();
        alock.lock();
        asize = audio.size();
        alock.unlock();
        if ((video_abort_flag > 0 && vsize < this->frame_size) || (audio_abort_flag > 0 && asize < this->audio_size)) // 已终止，且剩余数据已不足以组成一个数据包
        {
            while (video_abort_flag == 0)
            {
                vlock.lock();
                while (!video.empty())
                {
                    video.pop(); //丢弃视频帧所有数据
                }
                vlock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            vlock.lock();
            while (!video.empty())
            {
                video.pop(); //丢弃视频帧所有数据
            }
            vlock.unlock();

            while (audio_abort_flag == 0)
            {
                alock.lock();
                while (!audio.empty())
                {
                    audio.pop(); //丢弃音频帧所有数据
                }
                alock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            alock.lock();
            while (!audio.empty())
            {
                audio.pop(); //丢弃音频帧所有数据
            }
            alock.unlock();
            break;
        }
        while (vsize < this->frame_size || asize < this->audio_size) // 等待直至队列长度足够
        {
            vlock.lock();
            vsize = video.size();
            vlock.unlock();
            alock.lock();
            asize = audio.size();
            alock.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(100)); // 每0.1毫秒读取一次队列长度
        }
        vlock.lock(); // 视频加锁
        for (int i = 0; i < this->frame_size; i++)
        {
            buffer[i] = video.front(); // 读入缓冲区
            video.pop();
        }
        vlock.unlock(); // 视频解锁
        alock.lock();   // 音频加锁
        for (int i = 0; i < this->audio_size; i++)
        {
            buffer[this->frame_size + i] = audio.front(); // 读入缓冲区
            audio.pop();
        }
        alock.unlock();                                         // 音频解锁
        write(fd, buffer, this->frame_size + this->audio_size); // 写入串口
        std::this_thread::sleep_until(wakeup_time);             // 休眠以保证帧率准确
    }
    delete[] buffer;
    close(fd);
}