cmake_minimum_required(VERSION 2.8.12)

find_package(PkgConfig REQUIRED)
if(PkgConfig_FOUND)
    
    pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET libavformat libavcodec libavdevice libavutil libswscale)
    set(libav_LIBS PkgConfig::LIBAV)
    set(libav_FOUND TRUE)

endif()
