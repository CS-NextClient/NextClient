set(TARGET_NAME "client_mini-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        RadarUtilTests.cpp
        RadarViewTests.cpp
        NclMathTests.cpp
        RadarMemoryTests.cpp
        RadarFadeTests.cpp
        RadarPolicyTests.cpp
        ../src/hud/radar/RadarView.cpp
        ../src/hud/radar/RadarMemory.cpp
        ../src/hud/radar/RadarFade.cpp
        ../src/hud/radar/RadarPolicy.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        ncl_math::ncl_math
)

# Only the engine-free radar pieces are tested (RadarUtil.h, the RadarView math, the
# RadarMemory ring buffers, the RadarFade curves and the RadarPolicy decisions), so the
# engine-coupled DLL is not linked. RadarFade needs the SDK's screenfade.h POD header
# only, pulled in via the SDK interface include paths (headers only, no engine/vgui/steam
# link).
target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/hud/radar
        $<TARGET_PROPERTY:hl1_source_sdk::hl1_source_sdk,INTERFACE_INCLUDE_DIRECTORIES>
)

gtest_discover_tests(${TARGET_NAME})
