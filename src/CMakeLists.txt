cmake_minimum_required(VERSION 2.8.12)

add_library(avdecoder SHARED avdecoder.cpp)
target_include_directories(avdecoder PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_library(fft SHARED fft.cpp)
target_include_directories(fft PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_library(gray2bw SHARED gray2bw.cpp)
target_include_directories(gray2bw PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_library(transfer SHARED transfer.cpp)
target_include_directories(transfer PRIVATE ${PROJECT_SOURCE_DIR}/include)

find_package(libav REQUIRED)
if(libav_FOUND)

    target_link_libraries(avdecoder PRIVATE ${libav_LIBS})

endif()

find_package(fftw3 REQUIRED)
if(fftw3_FOUND)

    target_link_libraries(fft PRIVATE ${fftw3_LIBS})

endif()


# 这是代码编写时留下的漏洞，现在在cmake里补救一下，之后将解决这个问题
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)

    #target_link_libraries(gray2bw PRIVATE ${OpenCV_LIBS})
    target_link_libraries(gray2bw PUBLIC ${OpenCV_LIBS})
    #target_include_directories(gray2bw PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_include_directories(gray2bw PUBLIC ${OpenCV_INCLUDE_DIRS})

endif()
