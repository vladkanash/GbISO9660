cmake_minimum_required(VERSION 3.3)
project(gbiso)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    gbiso.cpp
    gbiso.h
    types.h ISOWriter.cpp ISOWriter.h ISO_writer_example.cpp)

add_executable(gbiso ${SOURCE_FILES} ISOWriter.cpp ISOWriter.h ISO_writer_example.cpp)