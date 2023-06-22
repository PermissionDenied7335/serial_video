cmake_minimum_required(VERSION 2.8.12)

add_executable(sv main.cpp)
target_include_directories(sv PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(sv PRIVATE avdecoder gray2bw fft transfer)

