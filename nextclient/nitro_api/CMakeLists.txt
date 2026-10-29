cmake_minimum_required(VERSION 3.21)
project(nitro_api LANGUAGES CXX)
set(OUTPUT_NAME "nitro_api2")

if (MSVC)
    if (DEFINED NITRO_API_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY ${NITRO_API_MSVC_RUNTIME_LIBRARY})
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif ()

# explicit dependencies
find_package(OpenGL REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(xbyak CONFIG REQUIRED)

add_library(nitro_api SHARED
        dep/easyloggingpp/src/easylogging++.cc
        dep/easyloggingpp/src/easylogging++.h
        src/chain/HandlersChain.h
        src/modules/engine/enginemsg_utils.cpp
        src/modules/engine/enginemsg_utils.h
        src/hook/GenericHookJit.h
        src/hook/GenericHookStatic.h
        src/hook/GenericHookVaformatStatic.h
        src/NitroApi.cpp
        src/FrameLifecycle.cpp
        src/modules/sdl2/loaded_sdl_module.cpp
        src/NitroApi.h
        src/HookStorage.h
        src/modules/windows/WindowsModule.cpp
        src/modules/windows/WindowsModule.h
        src/ChainExtensions.cpp
        src/LibraryHookListener.h
        include/nitroapi/NitroFunction.h
        src/modules/client/ClientModule.cpp
        src/modules/client/ClientModule.h
        src/modules/engine/EngineModule.cpp
        src/modules/engine/EngineModule.h
        src/modules/engine/EngineAddressProvider8684Windows.cpp
        src/modules/engine/EngineAddressProvider8684Windows.h
        src/AddressProviderBase.h
        src/modules/ModuleBase.cpp
        src/modules/ModuleBase.h
        include/nitroapi/Concepts.h
        src/modules/client/ClientAddressProvider8684Windows.cpp
        src/modules/client/ClientAddressProvider8684Windows.h
        src/SearchConfig.cpp
        src/SearchConfig.h
        include/nitroapi/BuildVersion.h
        include/nitroapi/modules/client/ClientData.h
        src/modules/sdl2/SDL2Module.cpp
        src/modules/sdl2/SDL2Module.h
        include/nitroapi/modules/sdl2/SDL2Data.h
        src/modules/sdl2/SDL2AddressProvider.cpp
        src/modules/sdl2/SDL2AddressProvider.h
        include/nitroapi/modules/engine/IEngine.h
        src/profiler.h
        src/seh_platform.h
        src/AddressProviderBase.cpp
        src/modules/client/ClientAddressProvider8684Linux.cpp
        src/modules/client/ClientAddressProvider8684Linux.h
        src/modules/engine/EngineAddressProvider8684Linux.cpp
        src/modules/engine/EngineAddressProvider8684Linux.h
)

# Built with nitro_api, never linked into it: the layout test is a compile-time check.
add_dependencies(nitro_api goldsrc_abi_golden)

target_include_directories(nitro_api PRIVATE
        src
        ${OPENGL_INCLUDE_DIR}
        dep/easyloggingpp/src
        ${NEXTCLIENT_ROOT}/dep/nitro_utils/include
        ${NEXTCLIENT_ROOT}/dep/funchook/include
        include
)

target_link_libraries(nitro_api PRIVATE
        hl1_source_sdk::hl1_source_sdk
        goldsrc::abi
        OpenGL::GL
        magic_enum::magic_enum
        xbyak::xbyak
        funchook-static
        nitro_utils
)

if (WIN32)
    target_link_libraries(nitro_api PRIVATE
            psapi.lib
            ntdll.lib)
endif ()

if (MSVC)
    target_compile_options(nitro_api PRIVATE
            /bigobj # increases the max number of sections in obj files. Due to template methods, obj module files (EngineModule, ClientModule) get huge sizes
            /WX     # treat all warnings as error
            /EHsc   # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )

    if (CMAKE_BUILD_TYPE MATCHES Release)
        target_compile_options(nitro_api PRIVATE /Zi /O2)

        set_target_properties(nitro_api PROPERTIES
                LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF")

        # Set file name & location
        set_target_properties(nitro_api PROPERTIES
                COMPILE_PDB_NAME nitro_api
                COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR})
    endif ()
else ()
    if (CMAKE_BUILD_TYPE MATCHES Debug)
        target_compile_options(nitro_api PRIVATE -g -rdynamic)
    endif ()
endif ()

target_compile_definitions(nitro_api PRIVATE
        NITRO_API_EASYLOGGING
        #easyloggingpp options
        ELPP_WINSOCK2
        ELPP_THREAD_SAFE
        ELPP_DISABLE_VERBOSE_LOGS
        ELPP_DISABLE_TRACE_LOGS
        ELPP_DISABLE_DEBUG_LOGS
        ELPP_DISABLE_DEFAULT_CRASH_HANDLING
        ELPP_FRESH_LOG_FILE
        ELPP_DEFAULT_LOGGER="nitro_api"
        ELPP_DEFAULT_PERFORMANCE_LOGGER="nitro_api"
        ELPP_DEFAULT_LOG_FILE="nitro_api.log"
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
            _WIN32
            WIN32
            _WIN32_WINNT=0x0600
            WINVER=0x0600
            NOMINMAX
            _UNICODE
            WIN32_LEAN_AND_MEAN
    )
endif()

target_link_libraries(nitro_api PRIVATE NextClient::profiling)

set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${OUTPUT_NAME}
        SUFFIX ".dll"
        PDB_NAME ${OUTPUT_NAME}
)

#-----------------------------------------------------------------
# Export library
#-----------------------------------------------------------------
add_library(nitro_api_export STATIC
        src/ChainExtensions.cpp
)
add_library(nitro_api::nitro_api ALIAS nitro_api_export)

target_include_directories(nitro_api_export PUBLIC
        include
)

target_link_libraries(nitro_api_export INTERFACE
        nitro_utils::nitro_utils
        hl1_source_sdk::hl1_source_sdk
        goldsrc::abi
)

target_compile_definitions(nitro_api_export INTERFACE
        NITRO_API_INCLUDED
)

add_executable(nitro-api-factory-probe EXCLUDE_FROM_ALL tests/ShippingFactoryProbe.cpp)
target_include_directories(nitro-api-factory-probe PRIVATE include)
target_link_libraries(nitro-api-factory-probe PRIVATE ${CMAKE_DL_LIBS})
if (WIN32)
    target_compile_definitions(nitro-api-factory-probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif ()

set(factory_runtime_files "$<TARGET_FILE:nitro_api>")
if (WIN32)
    list(APPEND factory_runtime_files "${NEXTCLIENT_ROOT}/test_support/runtime/tier0.dll")
else ()
    list(APPEND factory_runtime_files "$<TARGET_FILE:tier0>" "$<TARGET_FILE:vstdlib>")
endif ()
if (USE_PROFILER)
    list(APPEND factory_runtime_files "$<TARGET_FILE:Tracy::TracyClient>")
endif ()
set(factory_probe_manifest "${CMAKE_CURRENT_BINARY_DIR}/factory-probe-$<CONFIG>.cmake")
string(CONCAT factory_probe_content
        "set(PROBE [==[$<TARGET_FILE:nitro-api-factory-probe>]==])\n"
        "set(PROVIDER [==[$<TARGET_FILE:nitro_api>]==])\n"
        "set(PREPARED_DIRECTORY [==[${CMAKE_CURRENT_BINARY_DIR}/factory-probe/$<CONFIG>]==])\n"
        "set(RUNTIME_FILES [==[${factory_runtime_files}]==])\n")
file(GENERATE OUTPUT "${factory_probe_manifest}" CONTENT "${factory_probe_content}")
add_custom_target(verify-nitro-api-factory
        COMMAND ${CMAKE_COMMAND} "-DPROBE_MANIFEST=${factory_probe_manifest}"
                -P "${NEXTCLIENT_ROOT}/cmake/RunNitroApiFactoryProbe.cmake"
        DEPENDS nitro-api-factory-probe nitro_api
        VERBATIM)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
    add_test(NAME nitro-api-shipping-factory
            COMMAND ${CMAKE_COMMAND} "-DPROBE_MANIFEST=${factory_probe_manifest}"
                    -P "${NEXTCLIENT_ROOT}/cmake/RunNitroApiFactoryProbe.cmake")
    add_test(NAME nitro-api-shipping-factory-missing-provider
            COMMAND ${CMAKE_COMMAND} "-DPROBE=$<TARGET_FILE:nitro-api-factory-probe>"
                    "-DPROVIDER=${CMAKE_CURRENT_BINARY_DIR}/missing-provider-$<CONFIG>.dll" -DEXPECTED_PHASE=load
                    -P "${NEXTCLIENT_ROOT}/cmake/tests/NitroApiFactoryProbeFailure.cmake")
    set_tests_properties(nitro-api-shipping-factory nitro-api-shipping-factory-missing-provider
            PROPERTIES LABELS shipping-factory)
endif ()
