cmake_minimum_required(VERSION 3.21)
project(client_mini)
set(OUTPUT_NAME "client_mini")

find_package(optic REQUIRED)
find_package(taocpp-json CONFIG REQUIRED)

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
        src/hlsdk.h
        src/main.cpp
        src/main.h
        src/view.cpp
        src/view.h
        src/hud/HudCrosshair.cpp
        src/hud/HudCrosshair.h
        src/shared_util.cpp
        src/shared_util.h
        src/pm_shared/pm_shared.cpp
        src/viewmodel_overrides.h
        src/fov.cpp
        src/fov.h
        src/invert_mouse.cpp
        src/invert_mouse.h
        src/hud/hud_sprite/sprite_rect.h
        src/hud/hud_sprite/sprite_color.h
        src/hud/hud_sprite/hud_sprite_state.h
        src/hud/hud_sprite/hud_sprite.cpp
        src/hud/hud_sprite/hud_sprite.h
        src/hud/hud_sprite/hud_sprite_store.cpp
        src/hud/hud_sprite/hud_sprite_store.h
        src/hud/hud_sprite/hud_sprite_parser.h
        src/hud/hud_sprite/hud_sprite_parser.cpp
        src/studiorenderer.cpp
        src/studiorenderer.h
        src/utils.cpp
        src/utils.h
        src/hlsdk/sys_dll.h
        src/color_chat_in_console.cpp
        src/color_chat_in_console.h
        src/inspect.cpp
        src/inspect.h
        src/commands.cpp
        src/commands.h
        src/camera.cpp
        src/camera.h
        src/hud/HudDamageDirection.cpp
        src/hud/HudDamageDirection.h
        src/hud/HudDamageIcons.cpp
        src/hud/HudDamageIcons.h
        src/hud/HudDeathNotice.cpp
        src/hud/HudDeathNotice.h
        src/hud/HudHealth.cpp
        src/hud/HudHealth.h
        src/hud/HudRadar.cpp
        src/hud/HudRadar.h
        src/hud/HudBase.h
        src/hud/HudBaseHelper.h
        src/hud/GameHud.h
        src/hud/GameHud.cpp
        src/hud/HudAmmo.h
        src/hud/HudAmmo.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        taocpp::json
        nitro_api::nitro_api
        NextClient::gameui
        optic::optic
        view::view
        cvars::cvars
        crosshair::crosshair
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /GR- # disable rtti to reduce the library size
            /WX  # treat all warnings as error
            /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )

endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        NEXTCLIENT_WITH_PLUGINS=$<BOOL:${NEXTCLIENT_WITH_PLUGINS}>
        LOG_TAG="game"
        USE_OPTICK=$<BOOL:${USE_PROFILER}>
        _HAS_STATIC_RTTI=0 #disable rtti things in stl
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
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/cstrike/cl_dlls"
        OUTPUT_NAME ${OUTPUT_NAME}
        SUFFIX ".dll"
        PDB_NAME ${OUTPUT_NAME}
)

#-----------------------------------------------------------------
# Export library headers
#-----------------------------------------------------------------
add_library(client_mini_export INTERFACE)
add_library(NextClient::client_mini ALIAS client_mini_export)

target_include_directories(client_mini_export INTERFACE
        include
)

if (NEXTCLIENT_WITH_PLUGINS)
    target_sources(${PROJECT_NAME} PRIVATE src/plugin_bridge.cpp src/plugin_game_state.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE next_plugins)
endif ()

if (NEXTCLIENT_BUILD_TESTS AND NEXTCLIENT_WITH_PLUGINS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    add_executable(plugin-bridge-tests tests/plugin_bridge_tests.cpp src/plugin_bridge.cpp src/plugin_game_state.cpp
        src/color_chat_in_console.cpp)
    target_include_directories(plugin-bridge-tests PRIVATE src)
    target_compile_features(plugin-bridge-tests PRIVATE cxx_std_23)
    target_compile_definitions(plugin-bridge-tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
        PLUGIN_GAME_PATH="$<TARGET_FILE:plugin-game-api>" PLUGIN_MESSAGE_PATH="$<TARGET_FILE:plugin-game-api-message>")
    target_link_libraries(plugin-bridge-tests PRIVATE next_plugins taocpp::json nitro_api::nitro_api
        NextClient::gameui crosshair::crosshair GTest::gtest_main ws2_32)
    add_dependencies(plugin-bridge-tests plugin-game-api plugin-game-api-message)
    gtest_discover_tests(plugin-bridge-tests)
endif ()
