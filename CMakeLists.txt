cmake_minimum_required(VERSION 3.22)
project(ex4)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ex4
        MemoryConstants.h
        PhysicalMemory.cpp
        PhysicalMemory.h
        test2_write_one_page_twice_and_read.cpp
        VirtualMemory.cpp
        VirtualMemory.h)
