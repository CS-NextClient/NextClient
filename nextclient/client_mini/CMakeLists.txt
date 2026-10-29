cmake_minimum_required(VERSION 3.21)
project(client_mini)
set(OUTPUT_NAME "client_mini")

find_package(CMakeRC REQUIRED)

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
        src/multi_button_binds.cpp
        src/multi_button_binds.h
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
        src/say_text_line.h
        src/say_text_wrap.cpp
        src/say_text_wrap.h
        src/chat_wrap.cpp
        src/chat_wrap.h
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
        src/hud/HudRadarAdvanced.cpp
        src/hud/HudRadarAdvanced.h
        src/hud/radar/integration/RadarConfig.cpp
        src/hud/radar/integration/RadarConfig.h
        src/hud/radar/render/RadarView.cpp
        src/hud/radar/integration/RadarScoreboard.h
        src/hud/radar/integration/RadarScoreboard.cpp
        src/hud/radar/render/RadarView.h
        src/hud/radar/RadarFrame.h
        src/hud/radar/RadarFrame.cpp
        src/hud/radar/render/text/RadarLabel.cpp
        src/hud/radar/render/text/RadarLabel.h
        src/hud/radar/render/text/RadarText.cpp
        src/hud/radar/render/text/RadarText.h
        src/hud/radar/render/RadarPalette.h
        src/hud/radar/policy/RadarPolicy.cpp
        src/hud/radar/policy/RadarPolicy.h
        src/hud/radar/policy/ObjectiveSource.cpp
        src/hud/radar/policy/ObjectiveSource.h
        src/hud/radar/markers/RadarLayerMarker.cpp
        src/hud/radar/markers/RadarLayerMarker.h
        src/hud/radar/markers/ServerMarkerMotion.cpp
        src/hud/radar/markers/ServerMarker.cpp
        src/hud/radar/markers/ServerMarker.h
        src/hud/radar/map/Overview.cpp
        src/hud/radar/map/Overview.h
        src/hud/radar/map/layers/MapLayerOverrides.cpp
        src/hud/radar/map/layers/MapLayerOverrides.h
        src/hud/radar/map/MapGeometry.cpp
        src/hud/radar/map/MapReachability.cpp
        src/hud/radar/map/MapGeometry.h
        src/hud/radar/map/layers/RadarMapLayers.cpp
        src/hud/radar/map/layers/RadarLayerSelection.cpp
        src/hud/radar/map/layers/RadarMapLayers.h
        src/hud/radar/map/layers/RadarLayerAnalysisJob.cpp
        src/hud/radar/map/layers/RadarLayerAnalysisJob.h
        src/hud/radar/integration/LayerAnalysis.cpp
        src/hud/radar/integration/LayerAnalysis.h
        src/hud/radar/map/MapBspGeometry.cpp
        src/hud/radar/map/MapBspGeometry.h
        src/hud/radar/map/MapRender.cpp
        src/hud/radar/map/layers/MapLayerCache.cpp
        src/hud/radar/map/MapLayerRenderer.cpp
        src/hud/radar/map/MapRenderState.cpp
        src/hud/radar/map/MapImageTone.cpp
        src/hud/radar/map/MapImageTone.h
        src/hud/radar/map/layers/RadarLayerContext.cpp
        src/hud/radar/map/MapRender.h
        src/hud/radar/map/MapObjectives.cpp
        src/hud/radar/map/MapObjectives.h
        src/hud/radar/policy/Visibility.cpp
        src/hud/radar/policy/Visibility.h
        src/hud/radar/policy/SmokeRegistry.cpp
        src/hud/radar/policy/SmokeRegistry.h
        src/hud/radar/markers/RadarBroadcast.cpp
        src/hud/radar/markers/RadarBroadcast.h
        src/hud/radar/markers/MarkerIdentity.cpp
        src/hud/radar/markers/MarkerIdentity.h
        src/hud/radar/markers/MarkerFeed.cpp
        src/hud/radar/markers/MarkerFeed.h
        src/hud/radar/render/text/HudTextBuffer.cpp
        src/hud/radar/render/text/HudTextZone.cpp
        src/hud/radar/render/text/HudTextZone.h
        src/hud/radar/integration/RadarHooks.cpp
        src/hud/radar/integration/RadarHooks.h
        src/hud/radar/render/RadarDraw.cpp
        src/hud/radar/render/RadarDraw.h
        src/hud/radar/render/RadarDrawEffects.cpp
        src/hud/radar/render/RadarDrawInternal.h
        src/hud/radar/integration/RadarLayersDump.cpp
        src/hud/radar/integration/RadarLayersDump.h
        src/hud/radar/integration/RadarDebug.cpp
        src/hud/radar/integration/Performance.cpp
        src/hud/radar/integration/RadarDebug.h
        src/hud/radar/render/RadarFade.cpp
        src/hud/radar/render/RadarFade.h
        src/hud/radar/integration/RadarUtil.h
        src/hud/radar/RadarTeam.h
        src/hud/radar/RadarLimits.h
        src/hud/radar/layers/BackdropLayer.h
        src/hud/radar/layers/BackdropLayer.cpp
        src/hud/radar/layers/PlayersLayer.h
        src/hud/radar/layers/PlayersLayer.cpp
        src/hud/radar/layers/players/PlayerTracks.h
        src/hud/radar/layers/players/PlayerTracks.cpp
        src/hud/radar/layers/players/PlayerMarkerFade.h
        src/hud/radar/layers/players/PlayerMarkerFade.cpp
        src/hud/radar/layers/players/PlayerMarkerHistory.h
        src/hud/radar/layers/players/PlayerMarkerHistory.cpp
        src/hud/radar/layers/players/PlayerObserver.h
        src/hud/radar/layers/players/PlayerObserver.cpp
        src/hud/radar/layers/players/PlayerLastSeen.h
        src/hud/radar/layers/players/PlayerLastSeen.cpp
        src/hud/radar/layers/players/DeathMarks.h
        src/hud/radar/layers/players/DeathMarks.cpp
        src/hud/radar/layers/ObjectivesLayer.h
        src/hud/radar/layers/ObjectivesLayer.cpp
        src/hud/radar/layers/objectives/ObjectiveEntities.h
        src/hud/radar/layers/objectives/ObjectiveEntities.cpp
        src/hud/radar/layers/objectives/BombMarker.h
        src/hud/radar/layers/objectives/BombMarker.cpp
        src/hud/radar/layers/objectives/HostageMarkers.h
        src/hud/radar/layers/objectives/HostageMarkers.cpp
        src/hud/HudBase.h
        src/hud/HudBaseHelper.h
        src/hud/GameHud.h
        src/hud/GameHud.cpp
        src/hud/HudAmmo.h
        src/hud/HudAmmo.cpp
)

cmrc_add_resource_library(
        ${PROJECT_NAME}_rc
        ALIAS ${PROJECT_NAME}::rc

        assets/shaders/radar.vert
        assets/shaders/radar_prim.vert
        assets/shaders/radar_mask.frag
        assets/shaders/radar_prim.frag
        assets/shaders/radar_backdrop.frag
)

target_include_directories(${PROJECT_NAME} PRIVATE
        include
        src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        nitro_api::nitro_api
        NextClient::gameui
        NextClient::engine_mini
        NextClient::profiling
        glad::glad
        data_types::data_types
        ncl_math::ncl_math
        ncl_gl::ncl_gl
        ncl_utils::ncl_utils
        ${PROJECT_NAME}::rc
        view::view
        cvars::cvars
        crosshair::crosshair
        key_binding::key_binding
        TaskCoro::TaskCoro
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # TaskCoro runtime and synchronization context state belong to each module.
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:--exclude-libs,$<TARGET_FILE_NAME:TaskCoro>")
endif()

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /GR- # disable rtti to reduce the library size
            /WX  # treat all warnings as error
            /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )

endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        LOG_TAG="game"
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

target_link_libraries(client_mini_export INTERFACE cvars::cvars goldsrc::abi)

target_include_directories(client_mini_export INTERFACE
        include
)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
