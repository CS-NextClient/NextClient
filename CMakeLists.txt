set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
set(VCPKG_TARGET_TRIPLET "x86-windows-static" CACHE STRING "Vcpkg triplet")

cmake_minimum_required(VERSION 3.21)
project(NextClient VERSION 2.4.0)

set(NEXTCLIENT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# TODO do fetch curl and zlib via vcpkg, make a custom port if necessary
set(CMAKE_MODULE_PATH
        ${NEXTCLIENT_ROOT}/cmake
        ${CMAKE_MODULE_PATH}
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out/pdb/${CMAKE_BUILD_TYPE})

# CMAKE_MSVC_RUNTIME_LIBRARY and BUILD_SHARED_LIBS only affect external dependencies.
# NextClient subprojects are always built as shared libraries with mt runtime.
if (VCPKG_TARGET_TRIPLET STREQUAL "x86-windows-static")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()
set(BUILD_SHARED_LIBS OFF)

# NitroApi options
set(NITRO_API_MSVC_RUNTIME_LIBRARY ${CMAKE_MSVC_RUNTIME_LIBRARY})

# cpr options
set(CPR_USE_SYSTEM_CURL ON)
set(CPR_USE_SYSTEM_GTEST OFF)
set(CPR_ENABLE_CURL_HTTP_ONLY OFF)
set(CPR_ENABLE_SSL ON)

# external dependencies
add_subdirectory(dep/data_encoding)
add_subdirectory(dep/cpr)

# NextClient sub-projects
add_subdirectory(dep/NclNitroApi)
add_subdirectory(nextclient/launcher)
add_subdirectory(nextclient/engine_mini)
add_subdirectory(nextclient/filesystem_proxy)
add_subdirectory(nextclient/client_mini)
add_subdirectory(nextclient/gameui)
add_subdirectory(nextclient/steam_api_proxy)

# custom build targets
add_custom_target(BUILD_ALL ALL)
add_dependencies(BUILD_ALL
        next_launcher
        engine_mini
        filesystem_proxy
        client_mini
        gameui
        steam_api_proxy
        nitro_api
)

add_custom_target(INSTALL_ALL
        COMMENT "Installing libraries and assets..."
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:next_launcher> $ENV{NEXTCLIENT_INSTALL_DIR}/cs.exe
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:engine_mini> $ENV{NEXTCLIENT_INSTALL_DIR}/next_engine_mini.dll
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:filesystem_proxy> $ENV{NEXTCLIENT_INSTALL_DIR}/FileSystem_Proxy.dll
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:client_mini> $ENV{NEXTCLIENT_INSTALL_DIR}/cstrike/cl_dlls/client_mini.dll
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gameui> $ENV{NEXTCLIENT_INSTALL_DIR}/cstrike/cl_dlls/GameUI.dll
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:steam_api_proxy> $ENV{NEXTCLIENT_INSTALL_DIR}/steam_api.dll
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nitro_api> $ENV{NEXTCLIENT_INSTALL_DIR}/nitro_api2.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/nextclient/engine_mini/bin/OpenAL32.dll $ENV{NEXTCLIENT_INSTALL_DIR}/OpenAL32.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/nextclient/engine_mini/bin/sndfile.dll $ENV{NEXTCLIENT_INSTALL_DIR}/sndfile.dll
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets $ENV{NEXTCLIENT_INSTALL_DIR}
)

add_custom_target(BUILD_AND_INSTALL_ALL ALL)
add_dependencies(BUILD_AND_INSTALL_ALL
        BUILD_ALL
        INSTALL_ALL
)