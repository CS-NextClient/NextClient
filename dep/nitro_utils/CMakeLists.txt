cmake_minimum_required(VERSION 3.17)
project(nitro_utils VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)

set(NITRO_UTILS_SOURCES
        src/config/FileConfigProvider.cpp
        src/high_resolution_counter.cpp
        src/MemoryTools.cpp
        src/MemoryToolsEx.cpp
        src/net_utils.cpp
        src/random_utils.cpp
        src/string_utils.cpp)

if (UNIX)
    set(NITRO_UTILS_SOURCES ${NITRO_UTILS_SOURCES}
            dep/proc_maps_parser/pmparser.cpp
            src/platform/platform_linux.cpp)
else()
    set(NITRO_UTILS_SOURCES ${NITRO_UTILS_SOURCES}
            src/platform/platform_win32.cpp)
endif()

add_library(nitro_utils STATIC ${NITRO_UTILS_SOURCES})
add_library(nitro_utils::nitro_utils ALIAS nitro_utils)

target_include_directories(nitro_utils PRIVATE
        dep/proc_maps_parser/include
        src
        PUBLIC
        include
)

if (MSVC AND CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_options(nitro_utils PRIVATE /Zi /O2)

    set_target_properties(nitro_utils PROPERTIES
            LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF")

    # Set file name & location
    set_target_properties(nitro_utils PROPERTIES
            COMPILE_PDB_NAME nitro_utils
            COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR})
endif()

if (WIN32)
    target_compile_definitions(nitro_utils PRIVATE
            _WINDOWS
            _WIN32
            WIN32
            _WIN32_WINNT=0x0600 #https://docs.microsoft.com/ru-ru/cpp/porting/modifying-winver-and-win32-winnt?view=msvc-160
            WINVER=0x0600
            WIN32_LEAN_AND_MEAN
    )

    target_link_libraries(nitro_utils
            psapi.lib)
endif ()
