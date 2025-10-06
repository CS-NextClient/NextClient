cmake_minimum_required(VERSION 3.21)
project(TaskCoro)

set(CMAKE_CXX_STANDARD 23)

find_package(concurrencpp CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

add_library(${PROJECT_NAME} STATIC
        src/TaskCoro.cpp
        src/SynchronizationContext.cpp
        src/io/net.cpp
        src/impl/TaskCoroImpl.cpp
        src/impl/SynchronizationContextImpl.cpp
        src/impl/SynchronizationContextManual.cpp
)
add_library(TaskCoro::TaskCoro ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
        include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        data_types::data_types
        concurrencpp::concurrencpp
        magic_enum::magic_enum
)
