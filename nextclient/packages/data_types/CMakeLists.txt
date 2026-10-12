cmake_minimum_required(VERSION 3.21)
project(data_types)

set(CMAKE_CXX_STANDARD 23)

add_library(data_types STATIC
        src/ByteBuffer.cpp
        src/MessageReader.cpp
        src/MessageWriter.cpp
)
add_library(data_types::data_types ALIAS data_types)

target_include_directories(data_types PUBLIC include)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
