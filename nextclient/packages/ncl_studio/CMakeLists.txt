cmake_minimum_required(VERSION 3.21)
project(ncl_studio)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
    src/studio_mesh_builder.cpp
    src/StudioMetadata.cpp
    src/StudioReferencePose.cpp
    src/StudioTextures.cpp
    src/studio_validate.cpp
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
