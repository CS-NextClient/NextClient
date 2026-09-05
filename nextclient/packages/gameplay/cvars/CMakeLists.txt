cmake_minimum_required(VERSION 3.21)
project(cvars)

add_library(${PROJECT_NAME} INTERFACE)
add_library(cvars::cvars ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE include)
