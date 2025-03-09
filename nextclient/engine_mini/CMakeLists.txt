cmake_minimum_required(VERSION 3.21)
project(engine_mini)
set(OUTPUT_NAME "next_engine_mini")

find_package(OpenSSL REQUIRED)
find_package(concurrencpp CONFIG REQUIRED)
find_package(optic REQUIRED)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 23)

# common options
set(BUILD_SHARED_LIBS OFF)

#openal options
#set(ALSOFT_UTILS OFF)
#set(ALSOFT_EXAMPLES OFF)
#set(ALSOFT_INSTALL OFF)
#set(ALSOFT_INSTALL_CONFIG OFF)
#set(ALSOFT_INSTALL_HRTF_DATA OFF)
#set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF)
#set(ALSOFT_INSTALL_EXAMPLES OFF)
#set(ALSOFT_INSTALL_UTILS OFF)
#set(ALSOFT_UPDATE_BUILD_VERSION OFF)
#set(FORCE_STATIC_VCRT ON)

#alure options
set(ALURE_BUILD_SHARED OFF)
set(ALURE_BUILD_STATIC ON)
set(ALURE_ENABLE_MINIMP3 OFF)
set(ALURE_ENABLE_WAVE OFF)
set(ALURE_ENABLE_OPUS OFF)
set(ALURE_ENABLE_VORBIS OFF)
set(ALURE_ENABLE_FLAC OFF)
set(ALURE_INSTALL OFF)
set(ALURE_BUILD_EXAMPLES OFF)


# explicit dependencies
# TODO maybe get from vcpkg
set(OPENAL_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/dep/OpenAL-SDK/libs/Win32/OpenAL32.lib)
set(OPENAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dep/OpenAL-SDK/include)
set(SNDFILE_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/dep/libsndfile/lib/sndfile.lib)
set(SNDFILE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dep/libsndfile/include)

add_subdirectory(dep/bzip2)
add_subdirectory(dep/alure)
set_property(TARGET alure2_s PROPERTY CXX_STANDARD ${CMAKE_CXX_STANDARD})
target_compile_options(alure2_s PUBLIC "/Zc:__cplusplus")

file(GLOB_RECURSE ENGINE_MINI_SOURCES
        LIST_DIRECTORIES FALSE
        "src/*.cpp"
        "src/*.hpp"
        "src/*.h")

add_library(${PROJECT_NAME} SHARED
        ${ENGINE_MINI_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        dep/optic/include
        dep/bzip2
        src/audio/include
        include
        src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        nitro_api::nitro_api
        NextClient::gameui
        NextClient::filesystem_proxy
        data_encoding::data_encoding
        data_types::data_types
        TaskCoro::TaskCoro
        SafeResult::SafeResult
        easyloggingpp::easyloggingpp
        cpr::cpr
        optic::optic
        OpenSSL::SSL
        OpenSSL::Crypto
        concurrencpp::concurrencpp
        alure2_s
        bzip2
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /GR- # disable rtti to reduce the library size
            /WX  # treat all warnings as error
            /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )

    if (CMAKE_BUILD_TYPE MATCHES Release)
        # TODO set back the /GL flag when zlib and curl are included in the build
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
    endif()
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        LOG_TAG="game"
        USE_OPTICK=$<BOOL:${USE_PROFILER}>
        _HAS_STATIC_RTTI=0 #disable rtti things in stl
        BREADCRUMBS_TAG="[${PROJECT_NAME}]"
        #easyloggingpp options
        ELPP_WINSOCK2
        ELPP_NO_DEFAULT_LOG_FILE
        ELPP_DEFAULT_LOGGER="engine_mini"
)

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
        PREFIX ""
        OUTPUT_NAME ${OUTPUT_NAME}
        SUFFIX ".dll"
        PDB_NAME ${OUTPUT_NAME}
)

#-----------------------------------------------------------------
# Export library headers
#-----------------------------------------------------------------
add_library(engine_mini_export INTERFACE)
add_library(NextClient::engine_mini ALIAS engine_mini_export)

target_include_directories(engine_mini_export INTERFACE
        include
)
