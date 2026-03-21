cmake_minimum_required(VERSION 3.21)
project(bzip2)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

add_library(bzip2 STATIC
        blocksort.c
        bzlib.c
        bzlib.h
        bzlib_private.h
        compress.c
        crctable.c
        decompress.c
        huffman.c
        randtable.c
)
add_library(bzip2::bzip2 ALIAS bzip2)

target_include_directories(bzip2 PUBLIC .)
