set(TARGET_NAME "client_mini-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        RadarLayersDumpTests.cpp
        ../src/hud/radar/integration/RadarLayersDump.cpp
        RadarBspFixture.cpp
        RadarBspFileTests.cpp
        RadarMapLayerFixtures.cpp
        MapLayerCacheTests.cpp
        MapLayerRenderTests.cpp
        ../src/hud/radar/map/MapLayerRenderer.cpp
        ../src/hud/radar/map/MapRenderState.cpp
        ../src/hud/radar/map/MapImageTone.cpp
        RadarLayerContextTests.cpp
        RadarLayerLifecycleTests.cpp
        RadarLayerAnalysisJobTests.cpp
        ../src/hud/radar/map/layers/RadarLayerAnalysisJob.cpp
        ../src/hud/radar/map/layers/MapLayerCache.cpp
        ../src/hud/radar/map/layers/RadarLayerContext.cpp
        RadarLayerMarkerTests.cpp
        RadarMapObjectivesTests.cpp
        SmokeRegistryTests.cpp
        DeathMarksTests.cpp
        ServerMarkerTests.cpp
        ../src/hud/radar/markers/ServerMarkerMotion.cpp
        ../src/hud/radar/markers/RadarLayerMarker.cpp
        ../src/hud/radar/policy/SmokeRegistry.cpp
        MapLayerOverridesTests.cpp
        ../src/hud/radar/map/layers/MapLayerOverrides.cpp
        RadarMapLayersTests.cpp
        RadarLayerSelectionTests.cpp
        ../src/hud/radar/map/layers/RadarLayerSelection.cpp
        ../src/hud/radar/map/layers/RadarMapLayers.cpp
        MapGeometryTests.cpp
        MapReachabilityTests.cpp
        MapBspGeometryTests.cpp
        ../src/hud/radar/map/MapGeometry.cpp
        ../src/hud/radar/map/MapReachability.cpp
        ../src/hud/radar/map/MapBspGeometry.cpp
        MaterialDocTests.cpp
        MaterialResolveTests.cpp
        StudioMeshBuilderTests.cpp
        StudioValidateTests.cpp
        StudioTexturesTests.cpp
        MaterialPresetTests.cpp
        RadarUtilTests.cpp
        RadarViewTests.cpp
        RadarFrameTimeTests.cpp
        ../src/hud/radar/RadarFrame.cpp
        RadarScoreboardTests.cpp
        NclMathTests.cpp
        RadarPolylineTests.cpp
        PlayerTracksTests.cpp
        PlayerMarkerFadeTests.cpp
        PlayerMarkerHistoryTests.cpp
        ../src/hud/radar/layers/players/PlayerMarkerHistory.cpp
        PlayerObserverTests.cpp
        ../src/hud/radar/layers/players/PlayerObserver.cpp
        ../src/hud/radar/layers/players/PlayerMarkerFade.cpp
        PlayerLastSeenTests.cpp
        RadarFadeTests.cpp
        RadarPolicyTests.cpp
        RadarBroadcastTests.cpp
        MarkerIdentityTests.cpp
        MarkerFeedTests.cpp
        ObjectiveSourceTests.cpp
        HudTextZoneTests.cpp
        HudTextBufferTests.cpp
        ../src/hud/radar/render/RadarView.cpp
        ../src/hud/radar/integration/RadarScoreboard.cpp
        ../src/hud/radar/layers/players/PlayerTracks.cpp
        ../src/hud/radar/layers/players/PlayerLastSeen.cpp
        ../src/hud/radar/layers/players/DeathMarks.cpp
        ../src/hud/radar/render/RadarFade.cpp
        ../src/hud/radar/policy/RadarPolicy.cpp
        ../src/hud/radar/markers/RadarBroadcast.cpp
        ../src/hud/radar/markers/MarkerIdentity.cpp
        ../src/hud/radar/markers/MarkerFeed.cpp
        ../src/hud/radar/policy/ObjectiveSource.cpp
        ../src/hud/radar/render/text/HudTextZone.cpp
        ../src/hud/radar/render/text/HudTextBuffer.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        ncl_math::ncl_math
        ncl_material::ncl_material
        ncl_studio::ncl_studio
        goldsrc::abi
        glad::glad
        TaskCoro::TaskCoro
)

# Only the engine-free radar pieces are tested (RadarUtil.h, the RadarView math, the
# PlayerTracks ring buffers, the RadarFade curves, the RadarPolicy decisions, the
# MarkerFeed two-feed resolution and the ObjectiveSource feed choice), so the
# engine-coupled DLL is not linked. RadarFade takes screenfade_t from goldsrc/view.h of
# goldsrc::abi (headers only, no engine/vgui/steam link).
target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

if (WIN32)
    target_compile_definitions(${TARGET_NAME} PRIVATE NOMINMAX)
endif()

gtest_discover_tests(${TARGET_NAME})

file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/radar-test-properties.cmake" CONTENT [=[
list(FIND client_mini-tests_TESTS RadarLayerScheduledJobTest.DestructionJoinsWithoutPumpingFrames radar_job_test)
if(NOT radar_job_test EQUAL -1)
    set_tests_properties(RadarLayerScheduledJobTest.DestructionJoinsWithoutPumpingFrames PROPERTIES TIMEOUT 10)
endif()
]=])
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${CMAKE_CURRENT_BINARY_DIR}/radar-test-properties.cmake")

add_executable(radar-gl-tests
        MapLayerRenderGlTests.cpp
        ../src/hud/radar/render/RadarView.cpp
        ../src/hud/radar/markers/RadarLayerMarker.cpp
        RadarMapLayerFixtures.cpp
        ../src/hud/radar/map/MapLayerRenderer.cpp
        ../src/hud/radar/map/MapRenderState.cpp
        ../src/hud/radar/map/MapImageTone.cpp
        ../src/hud/radar/map/MapGeometry.cpp
        ../src/hud/radar/map/MapReachability.cpp
        ../src/hud/radar/map/layers/RadarMapLayers.cpp
        ../src/hud/radar/map/layers/MapLayerCache.cpp
        ../src/hud/radar/map/layers/MapLayerOverrides.cpp
        RadarShaderTests.cpp
        RadarDrawGlTests.cpp
        RadarGlContext.cpp
        ../src/hud/radar/render/RadarDraw.cpp
        ../src/hud/radar/render/RadarDrawEffects.cpp
)

target_link_libraries(radar-gl-tests PRIVATE
        GTest::gtest_main
        glad::glad
        goldsrc::abi
        ncl_math::ncl_math
        ncl_gl::ncl_gl
        client_mini::rc
        nitro_api::nitro_api
        NextClient::gameui
        TaskCoro::TaskCoro
)
target_include_directories(radar-gl-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_compile_definitions(radar-gl-tests PRIVATE
        RADAR_SHADER_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders"
)

if (WIN32)
    target_compile_definitions(radar-gl-tests PRIVATE NOMINMAX)
    target_link_libraries(radar-gl-tests PRIVATE opengl32 gdi32 user32)
else()
    target_link_libraries(radar-gl-tests PRIVATE ${CMAKE_DL_LIBS})
endif()

if (UNIX)
    gtest_discover_tests(radar-gl-tests PROPERTIES LABELS gl ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
else()
    gtest_discover_tests(radar-gl-tests PROPERTIES LABELS gl)
endif()
