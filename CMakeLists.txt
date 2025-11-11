cmake_minimum_required(VERSION 3.25)
project(free_operate)

set(CMAKE_CXX_STANDARD 20)

add_executable(free_operate main.cpp tiny_thread_pool.h tiny_thread_pool.cpp)
