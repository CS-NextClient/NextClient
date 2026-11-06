cmake_minimum_required(VERSION 3.21)
project(ncl_image)

set(CMAKE_CXX_STANDARD 23)

add_library(ncl_image STATIC src/Decode.cpp)
add_library(ncl_image::ncl_image ALIAS ncl_image)
target_include_directories(ncl_image PUBLIC include)
target_link_libraries(ncl_image PUBLIC ncl_utils::ncl_utils PRIVATE stb::stb)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
