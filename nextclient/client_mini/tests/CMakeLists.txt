set(TARGET_NAME "client_mini-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        RadarUtilTests.cpp
        RadarViewTests.cpp
        NclMathTests.cpp
        PlayerTracksTests.cpp
        RadarFadeTests.cpp
        RadarPolicyTests.cpp
        MarkerIdentityTests.cpp
        MarkerFeedTests.cpp
        ObjectiveSourceTests.cpp
        HudTextZoneTests.cpp
        HudTextBufferTests.cpp
        ../src/hud/radar/RadarView.cpp
        ../src/hud/radar/layers/players/PlayerTracks.cpp
        ../src/hud/radar/RadarFade.cpp
        ../src/hud/radar/RadarPolicy.cpp
        ../src/hud/radar/MarkerIdentity.cpp
        ../src/hud/radar/MarkerFeed.cpp
        ../src/hud/radar/ObjectiveSource.cpp
        ../src/hud/radar/HudTextZone.cpp
        ../src/hud/radar/HudTextBuffer.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        ncl_math::ncl_math
        goldsrc::abi
)

# Only the engine-free radar pieces are tested (RadarUtil.h, the RadarView math, the
# PlayerTracks ring buffers, the RadarFade curves, the RadarPolicy decisions, the
# MarkerFeed two-feed resolution and the ObjectiveSource feed choice), so the
# engine-coupled DLL is not linked. RadarFade takes screenfade_t from goldsrc/view.h of
# goldsrc::abi (headers only, no engine/vgui/steam link).
target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/hud/radar
)

gtest_discover_tests(${TARGET_NAME})
