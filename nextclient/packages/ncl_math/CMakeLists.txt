cmake_minimum_required(VERSION 3.21)
project(ncl_math)

add_library(${PROJECT_NAME} INTERFACE)
add_library(ncl_math::ncl_math ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE
        include
)
