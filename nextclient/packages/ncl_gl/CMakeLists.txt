cmake_minimum_required(VERSION 3.21)
project(ncl_gl)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
    src/capture.cpp
    src/loader.cpp
    src/program.cpp
)
add_library(ncl_gl::ncl_gl ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
        include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        glad::glad
)
