cmake_minimum_required(VERSION 3.21)
project(stb)

add_library(${PROJECT_NAME} INTERFACE)
add_library(stb::stb ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(${PROJECT_NAME} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/impl.cpp
)
