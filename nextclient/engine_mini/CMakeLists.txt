cmake_minimum_required(VERSION 3.21)
project(engine_mini)
set(OUTPUT_NAME "next_engine_mini")

find_package(concurrencpp CONFIG REQUIRED)
find_package(optic REQUIRED)
find_package(SDL2 REQUIRED)
find_package(imageinfo CONFIG REQUIRED)
find_package(EASTL CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 23)

if (NEXTCLIENT_BUILD_TESTS)
    set(LIBRARY_BUILD_TYPE STATIC)
    set(TARGET_LIBRARIES_SCOPE PUBLIC)
else ()
    set(LIBRARY_BUILD_TYPE SHARED)
    set(TARGET_LIBRARIES_SCOPE PRIVATE)
endif ()

# common options
set(BUILD_SHARED_LIBS OFF)

file(GLOB_RECURSE ENGINE_MINI_SOURCES
        LIST_DIRECTORIES FALSE
        "src/*.cpp"
        "src/*.hpp"
        "src/*.h")

add_library(${PROJECT_NAME} ${LIBRARY_BUILD_TYPE}
        ${ENGINE_MINI_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        include
        src
)

target_link_libraries(${PROJECT_NAME} ${TARGET_LIBRARIES_SCOPE}
        nitro_api::nitro_api
        NextClient::gameui
        NextClient::filesystem_proxy
        data_encoding::data_encoding
        data_types::data_types
        TaskCoro::TaskCoro
        ncl_utils::ncl_utils
        easyloggingpp::easyloggingpp
        cpr::cpr
        optic::optic
        concurrencpp::concurrencpp
        bzip2
        glad::glad
        SDL2::SDL2
        imageinfo::imageinfo
        stb::stb
        EASTL
        unordered_dense::unordered_dense
        crypt32
        advapi32
        wbemuuid
        ole32
        oleaut32
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /GR- # disable rtti to reduce the library size
            /WX  # treat all warnings as error
            /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )

endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        LOG_TAG="game"
        USE_OPTICK=$<BOOL:${USE_PROFILER}>
        _HAS_STATIC_RTTI=0 #disable rtti things in stl
        BREADCRUMBS_TAG="[${PROJECT_NAME}]"
        #easyloggingpp options
        ELPP_THREAD_SAFE
        ELPP_WINSOCK2
        ELPP_DISABLE_DEFAULT_CRASH_HANDLING
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

if (NOT NEXTCLIENT_BUILD_TESTS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            PREFIX ""
            OUTPUT_NAME ${OUTPUT_NAME}
            SUFFIX ".dll"
            PDB_NAME ${OUTPUT_NAME}
    )
endif ()

#-----------------------------------------------------------------
# Export library headers
#-----------------------------------------------------------------
add_library(engine_mini_export INTERFACE)
add_library(NextClient::engine_mini ALIAS engine_mini_export)

target_include_directories(engine_mini_export INTERFACE
        include
)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
