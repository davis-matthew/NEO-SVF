cmake_minimum_required(VERSION 3.14)
project(TSATEST)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_STANDARD 14)

add_executable(TSATEST main.cpp Test2.cpp Test3.cpp Test2.h Test3.h)