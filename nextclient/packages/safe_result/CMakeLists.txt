cmake_minimum_required(VERSION 3.21)
project(SafeResult)

# common options
set(BUILD_SHARED_LIBS OFF)

add_library(${PROJECT_NAME} INTERFACE)
add_library(SafeResult::SafeResult ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE
        include
)
