cmake_minimum_required(VERSION 3.27)
project(MockFilesys)

set(CMAKE_CXX_STANDARD 17)

# define ON_LINUX to use the FakeDiskDriver
# Filesystem files are separately recompiled by kernel right now to test integration.
# Later, we probably want to compile the filesystem separately and link it with the kernel.
add_compile_definitions(ON_LINUX) 

#Replaced main.cpp with test class, change back later as needed
add_executable(MockFilesys main.cpp
        interface/FakeDiskDriver.h
        interface/FakeDiskDriver.cpp
        interface/BlockManager.h
        interface/BlockManager.cpp
        filesys/Block.h
        filesys/FileSystem.cpp
        filesys/FileSystem.h
        filesys/BitmapManager.cpp
        filesys/BitmapManager.h
        filesys/InodeTable.cpp
        filesys/InodeTable.h
        filesys/File.cpp
        filesys/File.h
        filesys/Directory.cpp
        filesys/Directory.h
        filesys/LogManager.cpp
        filesys/LogManager.h
)
