cmake_minimum_required(VERSION 3.21)
project(data_encoding)

add_library(data_encoding STATIC
        src/aes.cpp
        src/base64.cpp
        src/md5.cpp
)
add_library(data_encoding::data_encoding ALIAS data_encoding)

target_include_directories(data_encoding PUBLIC include)
