cmake_minimum_required(VERSION 3.27)
project(MockFilesys)

set(CMAKE_CXX_STANDARD 17)

add_executable(MockFilesys main.cpp
        FakeDiskDriver.h
        FakeDiskDriver.cpp
        BlockManager.h
        BlockManager.cpp)
