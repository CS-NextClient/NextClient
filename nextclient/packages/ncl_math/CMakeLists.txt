cmake_minimum_required(VERSION 3.21)
project(ncl_math)

add_library(${PROJECT_NAME} STATIC
    src/quaternion.cpp
    src/matrix.cpp
)
add_library(ncl_math::ncl_math ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17 PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PUBLIC
        include
)

target_compile_options(${PROJECT_NAME} PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/fp:precise>"
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-fast-math;-ffp-contract=off>"
)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
