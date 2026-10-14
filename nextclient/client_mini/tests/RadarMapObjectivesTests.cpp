#include <gtest/gtest.h>

#include "hud/radar/markers/RadarLayerMarker.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "RadarMapLayerFixtures.h"

TEST(RadarMapObjectives, ObjectiveSpansTwoFloors)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});
    fixture.AddFloor(256, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;

    ncl_math::Aabb3 volume{{32, 32, -1}, {96, 96, 257}};
    EXPECT_EQ(map.Intersect(volume), (std::vector<int>{0, 1}));

    RadarLayerView view = RadarLayerContext_ComputeView(map, 0, {}, true);
    EXPECT_FLOAT_EQ(RadarLayerMarker_ComputeVolumeStyle(view, {&volume, 1}).opacity, 1.0f);

    view = RadarLayerContext_ComputeView(map, 1, view.origin, view.enabled);

    EXPECT_EQ(RadarLayerMarker_ComputeVolumeStyle(view, {&volume, 1}).vertical, 0);
}

TEST(RadarMapObjectives, DisjointSupportDoesNotAssignVolume)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});
    fixture.AddFloor(256, {{512, 0}, {768, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.Intersect({{32, 32, -1}, {96, 96, 257}}), (std::vector<int>{0}));
}

TEST(RadarMapObjectives, NearestAssignedLayerUsesDeterministicTie)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});
    fixture.AddFloor(256, {{0, 0}, {256, 256}});
    fixture.AddFloor(512, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 384.0f}, {"top", 384.0f, 640.0f}}, {}}, error)) << error;

    for (bool lower_first : {true, false})
    {
        SCOPED_TRACE(lower_first);
        ncl_math::Aabb3 lower{{32, 32, -1}, {96, 96, 1}};
        ncl_math::Aabb3 upper{{32, 32, 511}, {96, 96, 513}};
        ncl_math::Aabb3 volumes[] = {lower_first ? lower : upper, lower_first ? upper : lower};
        RadarLayerStyle style = RadarLayerMarker_ComputeVolumeStyle(RadarLayerContext_ComputeView(map, 1, {}, true), volumes);
        EXPECT_FLOAT_EQ(style.opacity, 0.4f);
        EXPECT_EQ(style.vertical, -1);
    }
}
