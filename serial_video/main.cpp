#include <cstdlib>
#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <getopt.h>
#include <unistd.h>

#include "serial_video/avdecoder.hpp"
#include "serial_video/gray2bw.hpp"
#include "serial_video/fft.hpp"
#include "serial_video/transfer.hpp"

std::queue<uint8_t> av_video, gray_video, fft_audio;
std::queue<uint16_t> av_audio;
std::mutex av_video_lock, av_audio_lock, gray_video_lock, fft_audio_lock;
std::atomic<int> decode_done = 0, gray_done = 0, fft_done = 0;

const struct option longopts[]
{
    {"help", no_argument, NULL, 'h'},
    {"input-media", required_argument, NULL, 'i'},
    {"output-device", required_argument, NULL, 'o'},
    {"baudrate", required_argument, NULL, 'b'},
	{"audio-fft-threshold", required_argument, NULL, 'a'}
};

void usage(const char *progname)
{
    std::cout << "Usage: " << progname << " [OPTION]..." << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "\t-h, --help\t\t\t\t\tdisplay this help" << std::endl;
    std::cout << "\t-i, --input-media=path/to/your/media/file\tyour input media" << std::endl;
    std::cout << "\t-o, --output-device=path/to/serial/port\t\tyour serial port to transmit video" << std::endl;
    std::cout << "\t-b, --baudrate=BAUDRATE\t\t\t\tbaud rate in bps (e.g. 115200 2000000)" << std::endl;
	std::cout << "\t-a, --audio-fft-threshold\t\t\tthe lowest power in fft power spectrum for playback" << std::endl;
}

int main(int argc, char **argv)
{
    int optc, baudrate = -1, parse_failed = 0, audio_threshold = -1;
    const char *progname = basename(argv[0]);
    char *input_media = NULL, *output_device = NULL, *baudrate_str = NULL, *audio_threshold_str = NULL;
    while ((optc = getopt_long(argc, argv, "hi:o:b:a:", longopts, NULL)) != -1) //获取命令行参数
    {
        switch(optc)
        {
            case 'h': //帮助
                usage(progname);
                exit(EXIT_SUCCESS);
                break; //理论上不可能执行到这里，但还是加上保险
            case 'i': //输入
                input_media = optarg;
                break;
            case 'o': //输出
                output_device = optarg;
                break;
            case 'b': //波特率
                baudrate_str = optarg;
                baudrate = atoi(baudrate_str);
                break;
			case 'a': //音频功率谱阈值
				audio_threshold_str = optarg;
				audio_threshold = atoi(audio_threshold_str);
				break;
            default:
                parse_failed = 1;
        }
    }
    if (parse_failed || optind < argc || baudrate <= 0 || input_media == NULL || output_device == NULL)
    {
        if (optind < argc) //有未被解析出来的参数，属于无效参数
            std::cerr << "Invalid argument: " << argv[optind] << std::endl;
        if (input_media == NULL)
            std::cerr << "Input media not given" << std::endl;
        if (output_device == NULL)
            std::cerr << "Output device not given" << std::endl;
        if (baudrate <= 0)
            std::cerr << "Invalid baudrate" << std::endl;
		if (audio_threshold < 0)
			std::cerr << "Invalid audio threshold" << std::endl;
        std::cerr << "Try " << progname << " --help for more information." << std::endl;
        exit(EXIT_FAILURE);
    }

    if (access(input_media, R_OK))
    {
        std::cerr << "Cannot open " << input_media << std::endl;
        exit(EXIT_FAILURE);
    }
    if(access(output_device, R_OK|W_OK))
    {
        std::cerr << "Cannot open " << output_device << std::endl;
        exit(EXIT_FAILURE);
    }

    try
    {
        avdecoder av(input_media);
        av.open();
        gray2bw gray(av.get_video_width(), av.get_video_height(), 128, 64);
        fft freq(av.get_audio_samplerate(), av.get_video_framerate(), audio_threshold); //现在可以在命令行测试这个阈值
        transfer trans(output_device, baudrate, av.get_video_framerate(), 1024, 1);
        std::thread dec_t(&avdecoder::streamed_decode, &av, std::ref(av_video), std::ref(av_video_lock), std::ref(av_audio), std::ref(av_audio_lock), std::ref(decode_done));
        std::thread gray_t(&gray2bw::streamed_convert, &gray, std::ref(av_video), std::ref(av_video_lock), std::ref(gray_video), std::ref(gray_video_lock), std::ref(decode_done), std::ref(gray_done));
        std::thread freq_t(&fft::streamed_calculate, &freq, std::ref(av_audio), std::ref(av_audio_lock), std::ref(fft_audio), std::ref(fft_audio_lock), std::ref(decode_done), std::ref(fft_done));
        std::thread trans_t(&transfer::streamed_start, &trans, std::ref(gray_video), std::ref(gray_video_lock), std::ref(fft_audio), std::ref(fft_audio_lock), std::ref(gray_done), std::ref(fft_done));
        dec_t.join();
        gray_t.join();
        freq_t.join();
        trans_t.join();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
