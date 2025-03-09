cmake_minimum_required(VERSION 3.21)
project(data_types)

add_library(data_types STATIC
        src/ByteBuffer.cpp
)
add_library(data_types::data_types ALIAS data_types)

target_include_directories(data_types PUBLIC include)
