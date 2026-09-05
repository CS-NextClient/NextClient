cmake_minimum_required(VERSION 3.21)
project(crosshair)

add_library(${PROJECT_NAME} INTERFACE)
add_library(crosshair::crosshair ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE include)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
