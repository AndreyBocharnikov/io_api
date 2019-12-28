cmake_minimum_required(VERSION 3.14)
project(my_server)

SET(CMAKE_CXX_FLAGS -pthread)
set(CMAKE_CXX_STANDARD 14)

add_executable(my_server main.cpp server.cpp worker.cpp task.cpp)