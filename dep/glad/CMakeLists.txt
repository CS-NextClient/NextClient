cmake_minimum_required(VERSION 3.21)
project(glad)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

add_library(glad STATIC
        src/glad.c
)
add_library(glad::glad ALIAS glad)

target_include_directories(glad PUBLIC include)
