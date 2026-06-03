cmake_minimum_required(VERSION 3.21)
project(hwid_collector)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
    src/hwid_collector.cpp
)
add_library(hwid_collector::hwid_collector ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
        include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        crypt32
        advapi32
        wbemuuid
        ole32
        oleaut32
)
