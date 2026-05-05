cmake_minimum_required(VERSION 3.21)
project(NextClient VERSION 2.5.0)

set(NEXTCLIENT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# TODO do fetch curl and zlib via vcpkg, make a custom port if necessary
set(CMAKE_MODULE_PATH
        ${NEXTCLIENT_ROOT}/cmake
        ${CMAKE_MODULE_PATH}
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out/pdb)

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

# common compiler and linker options
if (MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/MP>)
    
    string(APPEND CMAKE_C_FLAGS_RELEASE
            " /Zi"  # enables PDB generation
            " /O2"  # enables maximize speed optimizations
            " /GL"  # enables whole program optimization
    )
    string(APPEND CMAKE_CXX_FLAGS_RELEASE
            " /Zi"  # enables PDB generation
            " /O2"  # enables maximize speed optimizations
            " /GL"  # enables whole program optimization
    )
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE
            " /LTCG"           # enables whole program optimization on link-time
            " /INCREMENTAL:NO" # /INCREMENTAL is not compatible with /LTCG
            " /DEBUG"          # enables PDB generation
            " /OPT:REF"       # eliminates functions and data that are never referenced
            " /OPT:ICF"       # enables COMDAT folding
    )
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE
            " /LTCG"           # enables whole program optimization on link-time
            " /INCREMENTAL:NO" # /INCREMENTAL is not compatible with /LTCG
            " /DEBUG"          # enables PDB generation
            " /OPT:REF"       # eliminates functions and data that are never referenced
            " /OPT:ICF"       # enables COMDAT folding
    )
endif ()

# external dependencies
add_subdirectory(dep/cpr)
add_subdirectory(dep/easyloggingpp)
add_subdirectory(dep/bzip2)
add_subdirectory(dep/glad)
add_subdirectory(dep/stb)

# NextClient sub-projects
add_subdirectory(dep/NclNitroApi)
add_subdirectory(nextclient/packages)
add_subdirectory(nextclient/launcher)
add_subdirectory(nextclient/engine_mini)
add_subdirectory(nextclient/filesystem_proxy)
add_subdirectory(nextclient/client_mini)
add_subdirectory(nextclient/gameui)
add_subdirectory(nextclient/steam_api_proxy)

# Install directory: cache variable with env fallback
if (DEFINED ENV{NEXTCLIENT_INSTALL_DIR} AND NOT NEXTCLIENT_INSTALL_DIR)
    set(NEXTCLIENT_INSTALL_DIR "$ENV{NEXTCLIENT_INSTALL_DIR}" CACHE PATH "NextClient install directory")
else ()
    set(NEXTCLIENT_INSTALL_DIR "" CACHE PATH "NextClient install directory")
endif ()

# custom build target
add_custom_target(BUILD_ALL ALL)
add_dependencies(BUILD_ALL
        next_launcher
        engine_mini
        filesystem_proxy
        client_mini
        gameui
        steam_api_proxy
        nitro_api
        vgui2
)

if (NEXTCLIENT_INSTALL_DIR)
    add_custom_command(TARGET BUILD_ALL POST_BUILD
            COMMENT "Installing libraries and assets to ${NEXTCLIENT_INSTALL_DIR}..."
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:next_launcher> ${NEXTCLIENT_INSTALL_DIR}/cstrike.exe
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:engine_mini> ${NEXTCLIENT_INSTALL_DIR}/next_engine_mini.dll
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:filesystem_proxy> ${NEXTCLIENT_INSTALL_DIR}/FileSystem_Proxy.dll
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:client_mini> ${NEXTCLIENT_INSTALL_DIR}/cstrike/cl_dlls/client_mini.dll
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gameui> ${NEXTCLIENT_INSTALL_DIR}/cstrike/cl_dlls/GameUI.dll
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:steam_api_proxy> ${NEXTCLIENT_INSTALL_DIR}/steam_api.dll
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nitro_api> ${NEXTCLIENT_INSTALL_DIR}/nitro_api2.dll
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vgui2> ${NEXTCLIENT_INSTALL_DIR}/vgui2.dll
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/dep/optic/bin/OptickCore.dll ${NEXTCLIENT_INSTALL_DIR}/OptickCore.dll
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${NEXTCLIENT_INSTALL_DIR}
    )
endif ()
