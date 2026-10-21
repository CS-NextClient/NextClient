cmake_minimum_required(VERSION 3.21)
project(key_binding)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
        src/key_binding.cpp
)
add_library(key_binding::key_binding ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
