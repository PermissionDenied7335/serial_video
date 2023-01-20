cmake_minimum_required(VERSION 2.8.12)
project(serial_video)
option(DEBUG "Toggle debug complie mode" OFF)
#aux_source_directory(${PROJECT_SOURCE_DIR} SRC_LIST)
#add_executable(main ${SRC_LIST})
add_executable(main main.cpp)
add_library(avdecoder SHARED avdecoder.cpp)
add_library(gray2bw SHARED gray2bw.cpp)
add_library(fft SHARED fft.cpp)
add_library(transfer SHARED transfer.cpp)
target_link_libraries(main avdecoder gray2bw fft transfer)

if(DEBUG)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} " -g")
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS}, "-g")
endif()

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-O2")
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS}, "-O2")

find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)

    target_include_directories(gray2bw PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_include_directories(main PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(gray2bw ${OpenCV_LIBS})

endif()

find_package(PkgConfig REQUIRED)
if(PkgConfig_FOUND)

    pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET libavformat libavcodec libavdevice libavutil libswscale)
    target_link_libraries(avdecoder PkgConfig::LIBAV)

    pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)
    target_link_libraries(fft PkgConfig::FFTW3)
endif()
