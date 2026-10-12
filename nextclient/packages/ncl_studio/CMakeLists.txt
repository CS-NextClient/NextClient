cmake_minimum_required(VERSION 3.21)
project(ncl_studio)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
    src/StudioMeshBuilder.cpp
    src/StudioTextures.cpp
    src/StudioValidate.cpp
)
add_library(ncl_studio::ncl_studio ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
        include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        goldsrc::abi
        ncl_math::ncl_math
        ncl_utils::ncl_utils
)
