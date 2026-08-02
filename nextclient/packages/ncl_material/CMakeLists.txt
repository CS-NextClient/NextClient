cmake_minimum_required(VERSION 3.21)
project(ncl_material)

find_package(taocpp-json CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
    src/MaterialDoc.cpp
    src/MaterialParse.cpp
    src/MaterialResolve.cpp
    src/ProgramManifest.cpp
    src/MaterialPreset.cpp
)
add_library(ncl_material::ncl_material ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
        include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        ncl_utils::ncl_utils
        taocpp::json
)
