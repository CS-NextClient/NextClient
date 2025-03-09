cmake_minimum_required(VERSION 3.20)
project(easyloggingpp)

add_library(${PROJECT_NAME} INTERFACE)
add_library(easyloggingpp::easyloggingpp ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_sources(${PROJECT_NAME} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/easylogging++.cc
)
