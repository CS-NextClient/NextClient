cmake_minimum_required(VERSION 3.21)
project(launcher VERSION 2.4.0)


# base configuration
set(NEXT_CLIENT_BUILD_VERSION_MAJOR 2)
set(NEXT_CLIENT_BUILD_VERSION_MINOR 4)
set(NEXT_CLIENT_BUILD_VERSION_PATCH 0)
set(NEXT_CLIENT_BUILD_VERSION_PRERELEASE "beta.6")

set(NEXT_CLIENT_BUILD_VERSION_PRERELEASE_APPEND "")
if (NEXT_CLIENT_BUILD_VERSION_PRERELEASE)
    set(NEXT_CLIENT_BUILD_VERSION_PRERELEASE_APPEND "-${NEXT_CLIENT_BUILD_VERSION_PRERELEASE}")
endif()
set(NEXT_CLIENT_BUILD_VERSION "${NEXT_CLIENT_BUILD_VERSION_MAJOR}.${NEXT_CLIENT_BUILD_VERSION_MINOR}.${NEXT_CLIENT_BUILD_VERSION_PATCH}${NEXT_CLIENT_BUILD_VERSION_PRERELEASE_APPEND}")
set(NEXT_LAUNCHER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_SYSTEM_VERSION 6.0)
set(CMAKE_CXX_STANDARD 23)

set(BUILD_SHARED_LIBS OFF)

if (VCPKG_TARGET_TRIPLET STREQUAL "x86-windows-static")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

# options
option(NEXT_LAUNCHER_ENABLE_GAMEANALYTICS "Enable GameAnalytics support" OFF)
set(NEXT_LAUNCHER_GAMEANALYTICS_GAME_KEY "" CACHE STRING "GameAnalytics game key")
set(NEXT_LAUNCHER_GAMEANALYTICS_SECRET_KEY "" CACHE STRING "GameAnalytics secret key")

option(NEXT_LAUNCHER_ENABLE_SENTRY "Enable Sentry support" OFF)
set(NEXT_LAUNCHER_SENTRY_PROJECT_NAME "" CACHE STRING "Sentry project name")
set(NEXT_LAUNCHER_SENTRY_UPLOAD_URL "" CACHE STRING "Sentry upload url (DSN)")
set(NEXT_LAUNCHER_SENTRY_ENV "" CACHE STRING "Sentry environment")

option(NEXT_LAUNCHER_ENABLE_UPDATER "Enable auto updater support" OFF)
set(NEXT_LAUNCHER_UPDATER_SERVICE_URL "" CACHE STRING "Auto updater service url")
set(NEXT_LAUNCHER_UPDATER_CIPHER_KEY "" CACHE STRING "Auto updater AES cipher key")

# cpr configuration
set(CPR_ENABLE_SSL ON)
set(CPR_USE_SYSTEM_CURL ON)

# gameanalytics configuration
if (NEXT_LAUNCHER_ENABLE_GAMEANALYTICS)
    if (VCPKG_TARGET_TRIPLET STREQUAL "x86-windows-static")
        set(PLATFORM "win32-vc141-mt-static")
    else ()
        set(PLATFORM "win32-vc141-static")
    endif ()
    set(NO_SQLITE_SRC "NO")

    add_subdirectory(dep/gs-sdk/build/cmake/gameanalytics)
    target_compile_options(GameAnalytics PRIVATE
            /Zc:strictStrings-
    )
    add_subdirectory(dep/gs-sdk/build/cmake/sqlite)
endif ()

# launcher subprojects
add_subdirectory(src)

if (LAUNCHER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
