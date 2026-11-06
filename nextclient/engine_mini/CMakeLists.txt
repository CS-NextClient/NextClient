cmake_minimum_required(VERSION 3.21)
project(engine_mini)
set(OUTPUT_NAME "next_engine_mini")

find_package(concurrencpp CONFIG REQUIRED)
find_package(CMakeRC REQUIRED)
find_package(SDL2 REQUIRED)
find_package(imageinfo CONFIG REQUIRED)
find_package(EASTL CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)
find_package(maxminddb CONFIG REQUIRED)

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

if (NOT WIN32)
    # src/binding/jsapi is the CEF-based in-game browser's JS API - real CEF isn't
    # ported to Linux here (see the ncl-hl1-source-sdk fork), so it's excluded for now.
    list(FILTER ENGINE_MINI_SOURCES EXCLUDE REGEX "/binding/jsapi/")
    # The shared text backend uses GDI and the Windows surface calling convention.
    list(FILTER ENGINE_MINI_SOURCES EXCLUDE REGEX "/graphics/text/")
endif ()

add_library(${PROJECT_NAME} ${LIBRARY_BUILD_TYPE}
        ${ENGINE_MINI_SOURCES}
)

cmrc_add_resource_library(
        ${PROJECT_NAME}_rc
        ALIAS ${PROJECT_NAME}::rc

        assets/shaders/preview_pick.vert
        assets/shaders/preview_pick.frag
        assets/shaders/preview_outline.vert
        assets/shaders/preview_outline.frag
        assets/shaders/test_quad.vert
        assets/shaders/test_quad.frag
        assets/shaders/studio.vert
        assets/shaders/studio.frag
        assets/shaders/studio_legacy.frag
        assets/shaders/studio.json
        assets/shaders/material_coverage.glsl
        assets/shaders/material_effect_clock.glsl
        assets/shaders/material_glitch.glsl
        assets/shaders/material_glitch_surface.glsl
        assets/shaders/studio_transform.glsl
        assets/shaders/studio_fog.glsl
        assets/shaders/material_occlusion.glsl
        assets/shaders/studio_effect_mask.frag
        assets/shaders/studio_effect_seed.frag
        assets/shaders/studio_effect_distance.frag
        assets/shaders/studio_effect_composite.frag
        assets/shaders/weapon_finish.vert
        assets/shaders/weapon_finish.frag
        assets/shaders/weapon_finish.json
)

target_include_directories(${PROJECT_NAME} PRIVATE
        include
        src
        ${NEXTCLIENT_ROOT}/dep/nextclientapi_amxx/include
)

target_link_libraries(${PROJECT_NAME} ${TARGET_LIBRARIES_SCOPE}
        nitro_api::nitro_api
        funchook-static
        NextClient::gameui
        NextClient::filesystem_proxy
        data_encoding::data_encoding
        data_types::data_types
        TaskCoro::TaskCoro
        ncl_utils::ncl_utils
        hwid_collector::hwid_collector
        cvars::cvars
        ncl_math::ncl_math
        ncl_perf::ncl_perf
        easyloggingpp::easyloggingpp
        cpr::cpr
        NextClient::profiling
        concurrencpp::concurrencpp
        bzip2
        glad::glad
        ncl_gl::ncl_gl
        ncl_material::ncl_material
        ncl_studio::ncl_studio
        ${PROJECT_NAME}::rc
        SDL2::SDL2
        ${CMAKE_DL_LIBS}
        imageinfo::imageinfo
        ncl_image::ncl_image
        EASTL
        unordered_dense::unordered_dense
        maxminddb::maxminddb
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} ${TARGET_LIBRARIES_SCOPE}
            crypt32
            advapi32
            wbemuuid
            ole32
            oleaut32
    )
endif ()

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /GR- # disable rtti to reduce the library size
            /WX  # treat all warnings as error
            /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )

endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        LOG_TAG="game"
        _HAS_STATIC_RTTI=0 #disable rtti things in stl
        BREADCRUMBS_TAG="[${PROJECT_NAME}]"
)

# easyloggingpp options, public in a test build: the library's source is compiled into every target that links
# it, and each copy has to see the same options
target_compile_definitions(${PROJECT_NAME} ${TARGET_LIBRARIES_SCOPE}
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

target_link_libraries(engine_mini_export INTERFACE goldsrc::abi ncl_perf::ncl_perf cvars::cvars)

target_include_directories(engine_mini_export INTERFACE
        include
)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
