# Serial Video
这是我在[B站《我用Linux和STM32摆烂了你的AE作业》](https://www.bilibili.com/video/BV1mT41117WZ/?vd_source=d06614fa4579c8370fcab0f85e2d023b)中用到的PC机代码，不过在视频发布之后该仓库还会陆续有更新 ~~（为了不让自己忘记这些代码，然后这个仓库沦为Inactive Repository）~~。

## 如何编译该工程？
**注意：本工程目前只适配Linux，没有做对Windows的支持！**
该工程使用libav进行视频解码，opencv进行传输前的视频帧处理，fftw3提取音频峰值功率对应的频率，最后用文件IO访问串口并发送数据。所以您需要提前准备这些依赖项：libavdevices-dev、libavutil-dev、libavcodec-dev、libopencv-dev和libfftw3-dev。
在Debian GNU/Linux下，您可以通过执行如下命令
```bash
sudo apt install libavdevices-dev libavutil-dev libavcodec-dev libopencv-dev libfftw3-dev
```
完成依赖项的安装。
接下来您可以开始配置工程并编译了
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```
如果没有出错，那么您将在build目录下看到名为vons的可执行文件，接下来您可以执行主程序了。
