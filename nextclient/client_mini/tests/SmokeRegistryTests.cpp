#include <gtest/gtest.h>

#include "hud/radar/policy/SmokeRegistry.h"
#include "hud/radar/markers/RadarLayerMarker.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "RadarMapLayerFixtures.h"

TEST(SmokeRegistry, SmokeUsesItsOwnHeight)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});
    fixture.AddFloor(256, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;

    SmokeRegistry smoke;
    smoke.OnSmoke({64, 64, 257}, true, 10);
    SmokeRegistry::Disc discs[1];

    ASSERT_EQ(smoke.Collect(discs, 11), 1);

    ncl_math::Vector3 center = discs[0].center;
    EXPECT_FLOAT_EQ(center.z, 257);

    RadarLayerStyle style = RadarLayerMarker_ComputeStyle(RadarLayerContext_ComputeView(map, 0, {64, 64, 1}, true), center);
    EXPECT_FLOAT_EQ(style.opacity, 0.4f);
    EXPECT_EQ(style.vertical, 1);
}
