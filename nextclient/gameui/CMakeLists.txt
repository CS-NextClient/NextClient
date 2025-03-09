cmake_minimum_required(VERSION 3.21)
project(gameui VERSION 1.2.4)
set(OUTPUT_NAME "GameUI")

find_package(concurrencpp CONFIG REQUIRED)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 23)

# common options
set(BUILD_SHARED_LIBS OFF)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

if (UNIX)
    add_compile_options(-m32 -march=pentium4 -Wno-narrowing)
    add_link_options(-m32 -static-libstdc++ -static-libgcc)
    set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config/library_config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/config/library_config.h)

file(GLOB_RECURSE GAMEUI_SOURCES
        LIST_DIRECTORIES FALSE
        "src/*.cpp"
        "src/*.hpp"
        "src/*.h"
)

add_library(${PROJECT_NAME} SHARED
        ${GAMEUI_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        src/GameUi
        src/GameUi/Controls
        src/GameUi/CreateMultiPlayerGameDialog
        src/GameUi/OptionsDialog
        src/config
        src/utils
        src
        include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        hl1_source_sdk::hl1_source_sdk
        NextClient::engine_mini
        nitro_utils::nitro_utils
        data_encoding::data_encoding
        concurrencpp::concurrencpp
        Ws2_32.lib
)

target_compile_options(${PROJECT_NAME} PRIVATE
        /WX  # treat all warnings as error
        /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
)

if (MSVC AND CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /Zi # enables PDB generation
            /O2 # enables maximize speed optimizations
    )

    target_link_options(${PROJECT_NAME} PRIVATE
            /LTCG            # enables whole program optimization on link-time
            /INCREMENTAL:NO  # /INCREMENTAL is not compatible with /LTCG
            /DEBUG           # enables PDB generation
            /OPT:REF         # eliminates functions and data that are never referenced
            /OPT:ICF         # enables COMDAT folding
    )
endif ()

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            _WIN32
            WIN32
            _WIN32_WINNT=0x0600
            WINVER=0x0600
            NOMINMAX
            _UNICODE
            WIN32_LEAN_AND_MEAN
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX "cstrike/cl_dlls/"
        OUTPUT_NAME ${OUTPUT_NAME}
        SUFFIX ".dll"
        PDB_NAME ${OUTPUT_NAME}
)

#-----------------------------------------------------------------
# Export library headers
#-----------------------------------------------------------------
add_library(gameui_export INTERFACE)
add_library(NextClient::gameui ALIAS gameui_export)

target_include_directories(gameui_export INTERFACE
        include
)
