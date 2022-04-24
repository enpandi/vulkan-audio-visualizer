cmake_minimum_required(VERSION 3.10)
project(audio_visualizer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_STANDARD 20)

add_executable(audio_visualizer main.cpp)
