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

# The studio file layout comes from the SDK's studio.h, but nothing here links against
# the SDK, so only its include paths are pulled in and only for the implementation.
target_include_directories(${PROJECT_NAME} PRIVATE
        $<TARGET_PROPERTY:hl1_source_sdk::hl1_source_sdk,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        ncl_math::ncl_math
        ncl_utils::ncl_utils
)
