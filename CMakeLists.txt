cmake_minimum_required(VERSION 3.27)
project(MockFilesys)

set(CMAKE_CXX_STANDARD 17)

#Replaced main.cpp with test class, change back later as needed
add_executable(MockFilesys LogFilesysTest.cpp
        FakeDiskDriver.h
        FakeDiskDriver.cpp
        BlockManager.h
        BlockManager.cpp
        Superblock.cpp
        Superblock.h
        LogRecord.cpp
        LogRecord.h
        Segment.cpp
        Segment.h
        LogManager.cpp
        LogManager.h
        )
