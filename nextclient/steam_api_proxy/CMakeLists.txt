cmake_minimum_required(VERSION 3.21)
project(steam_api_proxy)
set(OUTPUT_NAME "steam_api")

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()
set(CMAKE_CXX_STANDARD 23)

# common options
set(BUILD_SHARED_LIBS OFF)

if (UNIX)
    add_compile_options(-m32 -march=pentium4 -Wno-narrowing)
    add_link_options(-m32 -static-libstdc++ -static-libgcc)
    set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
endif ()

add_library(${PROJECT_NAME} SHARED
        src/api_impl/SteamAPI.cpp
        src/public/steam/isteamapps.h
        src/public/steam/isteambilling.h
        src/public/steam/isteamclient.h
        src/public/steam/isteamcontroller.h
        src/public/steam/isteamfriends.h
        src/public/steam/isteamgameserver.h
        src/public/steam/isteamgameserverstats.h
        src/public/steam/isteamhttp.h
        src/public/steam/isteammatchmaking.h
        src/public/steam/isteamnetworking.h
        src/public/steam/isteamremotestorage.h
        src/public/steam/isteamscreenshots.h
        src/public/steam/isteamunifiedmessages.h
        src/public/steam/isteamuser.h
        src/public/steam/isteamuserstats.h
        src/public/steam/isteamutils.h
        src/public/steam/matchmakingtypes.h
        src/public/steam/steam_api.h
        src/public/steam/steam_gameserver.h
        src/public/steam/steamclientpublic.h
        src/public/steam/steamhttpenums.h
        src/public/steam/steamtypes.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
        src
        src/public
        include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        nitro_api::nitro_api
        NextClient::client_mini
)

if (UNIX)
    target_link_options(${PROJECT_NAME} PRIVATE
            -Wl,--no-undefined
            -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/version.version)

    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/version.version)
endif ()

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /GR- # disable rtti to reduce the library size
            /WX  # treat all warnings as error
            /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )

    if (CMAKE_BUILD_TYPE MATCHES Release)
        target_compile_options(${PROJECT_NAME} PRIVATE
                /Zi # enables PDB generation
                /O2 # enables maximize speed optimizations
        )

        target_link_options(${PROJECT_NAME} PRIVATE
                /INCREMENTAL:NO  # /INCREMENTAL is not compatible with /LTCG
                /DEBUG           # enables PDB generation
                /OPT:REF         # eliminates functions and data that are never referenced
                /OPT:ICF         # enables COMDAT folding
        )
    endif()
else ()
    if (CMAKE_BUILD_TYPE MATCHES Debug)
        target_compile_options(${PROJECT_NAME} PRIVATE -g -rdynamic)
    endif ()
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        STEAM_API_EXPORTS
        SOURCEQUERY_STATIC
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            _WIN32
            WIN32
            _WIN32_WINNT=0x0600
            WINVER=0x0600
            NOMINMAX
            _UNICODE
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
add_library(steam_api_proxy_export INTERFACE)
add_library(NextClient::steam_api_proxy ALIAS steam_api_proxy_export)

target_include_directories(steam_api_proxy_export INTERFACE
        include
)
