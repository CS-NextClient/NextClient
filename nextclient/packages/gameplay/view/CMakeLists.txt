cmake_minimum_required(VERSION 3.21)
project(view)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
        src/view_bob.cpp
)
add_library(view::view ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include)

target_link_libraries(${PROJECT_NAME} PUBLIC ncl_math::ncl_math)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
