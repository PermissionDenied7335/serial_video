#ifndef __TRANSFER_HPP__
#define __TRANSFER_HPP__

#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <termios.h>

/**
 * @brief 音视频交错传输类
 * 
 */
class transfer
{
public:
    transfer(const char *device, int baudrate, int framerate, int frame_size, int audio_size);
    void start(std::queue<uint8_t> &video, std::queue<uint8_t> &audio);
    void streamed_start(std::queue<uint8_t> &video, std::mutex &video_lock, std::queue<uint8_t> &audio, std::mutex &audio_lock, std::atomic<int> &video_abort_flag, std::atomic<int> &audio_abort_flag);
private:
    std::string device_path;
    int frame_size, audio_size, framerate;
    speed_t baudrate;
};

#endif