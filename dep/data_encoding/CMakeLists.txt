cmake_minimum_required(VERSION 3.21)
project(data_encoding VERSION 1.2.4)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

add_library(data_encoding STATIC
        aes.cpp
        base64.cpp
        md5.cpp
)
add_library(data_encoding::data_encoding ALIAS data_encoding)

target_include_directories(data_encoding PUBLIC include)
