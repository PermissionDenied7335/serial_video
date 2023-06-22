cmake_minimum_required(VERSION 2.8.12)
project(serial_video)
option(DEBUG "Toggle debug complie mode" OFF)

if(DEBUG)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} " -g")
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS}, "-g")
endif()

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-O2")
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS}, "-O2")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

add_subdirectory(src)

add_executable(sv ${PROJECT_SOURCE_DIR}/serial_video/main.cpp)
target_include_directories(sv PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(sv PRIVATE avdecoder gray2bw fft transfer)

