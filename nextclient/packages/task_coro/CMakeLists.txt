cmake_minimum_required(VERSION 3.21)
project(TaskCoro)

find_package(concurrencpp CONFIG REQUIRED)

add_library(${PROJECT_NAME} STATIC
        src/TaskCoro.cpp
        src/io/net.cpp
)
add_library(TaskCoro::TaskCoro ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
        include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        data_types::data_types
        SafeResult::SafeResult
        concurrencpp::concurrencpp
)
