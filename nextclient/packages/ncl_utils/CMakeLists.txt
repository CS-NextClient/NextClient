cmake_minimum_required(VERSION 3.21)
project(ncl_utils)

find_package(taocpp-json CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
    src/backend_config_parser.cpp
)
add_library(ncl_utils::ncl_utils ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
        include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        taocpp::json
)
