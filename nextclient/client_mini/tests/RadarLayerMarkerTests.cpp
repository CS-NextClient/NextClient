#include <gtest/gtest.h>

#include "hud/radar/markers/RadarLayerMarker.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/layers/players/PlayerLastSeen.h"
#include "RadarMapLayerFixtures.h"

namespace
{
    RadarMapLayers TwoFloors()
    {
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0, {{0, 0}, {256, 256}});
        fixture.AddFloor(256, {{0, 0}, {256, 256}});

        RadarMapLayers map;
        EXPECT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

        std::string error;
        EXPECT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;

        return map;
    }
} // namespace

TEST(RadarLayerMarker, OtherFloorUsesPointFourOpacity)
{
    RadarMapLayers map = TwoFloors();
    RadarLayerView view = RadarLayerContext_ComputeView(map, 0, {64, 64, 1}, true);
    RadarLayerStyle style = RadarLayerMarker_ComputeStyle(view, ncl_math::Vector3{64, 64, 257});

    EXPECT_FLOAT_EQ(style.opacity, 0.4f);
    EXPECT_EQ(style.vertical, 1);

    view = RadarLayerContext_ComputeView(map, 1, view.origin, view.enabled);
    style = RadarLayerMarker_ComputeStyle(view, ncl_math::Vector3{64, 64, 1});

    EXPECT_FLOAT_EQ(style.opacity, 0.4f);
    EXPECT_EQ(style.vertical, -1);
}

TEST(RadarLayerMarker, VolumeMissAfterOtherFloorUsesItsOwnFallback)
{
    RadarMapLayers map = TwoFloors();
    RadarLayerView view = RadarLayerContext_ComputeView(map, 1, {64.0f, 64.0f, 257.0f}, true);
    ncl_math::Aabb3 volumes[] = {
        {{32.0f, 32.0f, -1.0f}, {96.0f, 96.0f, 1.0f}},
        {{1024.0f, 1024.0f, 255.0f}, {1056.0f, 1056.0f, 257.0f}},
    };
    RadarLayerStyle style = RadarLayerMarker_ComputeVolumeStyle(view, volumes);

    EXPECT_FLOAT_EQ(style.opacity, 1.0f);
    EXPECT_EQ(style.vertical, 0);

    volumes[1] = {{1024.0f, 1024.0f, -1.0f}, {1056.0f, 1056.0f, 1.0f}};
    style = RadarLayerMarker_ComputeVolumeStyle(view, volumes);

    EXPECT_FLOAT_EQ(style.opacity, 0.4f);
    EXPECT_EQ(style.vertical, -1);
}

TEST(RadarLayerMarker, SameLayerRiseHasNoArrow)
{
    RadarMapLayers map = TwoFloors();
    RadarLayerView view = RadarLayerContext_ComputeView(map, 0, {64, 64, 1}, true);
    RadarLayerStyle style = RadarLayerMarker_ComputeStyle(view, ncl_math::Vector3{64, 64, 100});

    EXPECT_FLOAT_EQ(style.opacity, 1.0f);
    EXPECT_EQ(style.vertical, 0);
}

TEST(RadarLayerMarker, UnknownHeightHasNoArrow)
{
    RadarMapLayers map = TwoFloors();
    RadarLayerView view = RadarLayerContext_ComputeView(map, 0, {}, true);
    EXPECT_EQ(RadarLayerMarker_ComputeStyle(view, std::nullopt).vertical, 0);

    view.enabled = false;
    RadarLayerStyle style = RadarLayerMarker_ComputeStyle(view, ncl_math::Vector3{64, 64, 257});
    EXPECT_EQ(style.vertical, 0);
    EXPECT_FLOAT_EQ(style.opacity, 1.0f);
}

TEST(RadarLayerMarker, RotatingRadarKeepsVerticalIndicator)
{
    RadarBox box{0, 0, 200, 200, 640, 480};
    RadarViewParams params{};
    params.ov_zoom = 1;
    params.map_scale = 1;
    params.rotate = true;
    params.view_yaw = 73;
    RadarTransform transform = RadarView_ComputeTransform(box, params);
    RadarShape shape = RadarView_ComputeShape(box, true);
    RadarLayerPlacement placement = RadarLayerMarker_Place(shape, transform.WorldToScreen({10, 20}), 5, {0.4f, 1});

    EXPECT_FLOAT_EQ(placement.indicator.x, placement.marker.x);
    EXPECT_LT(placement.indicator.y, placement.marker.y);
}

TEST(RadarLayerMarker, CompositeMarkerFitsRadarShape)
{
    for (bool round : {false, true})
    {
        RadarShape shape{{100, 100}, 100, round};

        for (int vertical : {-1, 1})
        {
            RadarLayerPlacement placement = RadarLayerMarker_Place(shape, {400, -400}, 6, {0.4f, vertical});
            EXPECT_TRUE(placement.offscreen);

            for (ncl_math::Vector2 point : {placement.marker, placement.indicator})
            {
                EXPECT_TRUE(shape.Contains(point + ncl_math::Vector2{6, 0}));
                EXPECT_TRUE(shape.Contains(point - ncl_math::Vector2{6, 0}));
                EXPECT_TRUE(shape.Contains(point + ncl_math::Vector2{0, 6}));
                EXPECT_TRUE(shape.Contains(point - ncl_math::Vector2{0, 6}));
            }
        }

        EXPECT_FALSE(RadarLayerMarker_Place(shape, {199, 100}, 6, {0.4f, 1}).offscreen);
    }
}

TEST(RadarLayerMarker, HistoryHeightSurvivesFloorSwitch)
{
    RadarMapLayers map = TwoFloors();
    PlayerLastSeen memory;
    PlayerLastSeen::Observation observations[] = {{1, {64, 64, 257}, RadarTeam::Terrorist}};
    memory.Update(observations, 10);
    memory.Update({}, 11);

    ASSERT_EQ(memory.get_marks().size(), 1u);

    RadarLayerView view = RadarLayerContext_ComputeView(map, 0, {}, true);
    EXPECT_EQ(RadarLayerMarker_ComputeStyle(view, memory.get_marks()[0].pos).vertical, 1);

    view = RadarLayerContext_ComputeView(map, 1, view.origin, view.enabled);

    EXPECT_EQ(RadarLayerMarker_ComputeStyle(view, memory.get_marks()[0].pos).vertical, 0);
    EXPECT_FLOAT_EQ(memory.get_marks()[0].pos.z, 257);
}

TEST(RadarLayerMarker, SmallVolumeIntersectionKeepsActiveUpperLayerStyle)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});
    fixture.AddFloor(64.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -1.0f, 1.0f}, {"upper", 63.0f, 65.0f}}, {}}, error)) << error;
    ASSERT_EQ(map.get_layers().size(), 2u);

    int upper = map.Locate({0.009f, 0.009f, 64.0f}, RadarLayerQuery::Below).layer;
    ASSERT_EQ(upper, 1);

    RadarLayerView view = RadarLayerContext_ComputeView(map, upper, {0.009f, 0.009f, 64.0f}, true);
    ncl_math::Aabb3 volumes[] = {{{0.0f, 0.0f, -1.0f}, {0.018f, 0.018f, 65.0f}}};
    RadarLayerStyle style = RadarLayerMarker_ComputeVolumeStyle(view, volumes);

    EXPECT_FLOAT_EQ(style.opacity, 1.0f);
    EXPECT_EQ(style.vertical, 0);
}


TEST(RadarLayerMarker, DistantStacksUsePhysicalHeightForArrow)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(2048.0f, {{2048.0f, 0.0f}, {2560.0f, 512.0f}});
    fixture.AddFloor(2304.0f, {{2048.0f, 0.0f}, {2560.0f, 512.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);

    RadarLayerView view = RadarLayerContext_ComputeView(map, 1, {64.0f, 64.0f, 257.0f}, true);
    RadarLayerStyle style = RadarLayerMarker_ComputeStyle(view, ncl_math::Vector3{2112.0f, 64.0f, 2049.0f});
    EXPECT_FLOAT_EQ(style.opacity, 0.4f);
    EXPECT_EQ(style.vertical, 1);

    ncl_math::Aabb3 higher[] = {{{2080.0f, 32.0f, 2048.0f}, {2144.0f, 96.0f, 2112.0f}}};
    EXPECT_EQ(RadarLayerMarker_ComputeVolumeStyle(view, higher).vertical, 1);

    view = RadarLayerContext_ComputeView(map, 0, {2112.0f, 64.0f, 2049.0f}, true);
    style = RadarLayerMarker_ComputeStyle(view, ncl_math::Vector3{64.0f, 64.0f, 257.0f});

    EXPECT_FLOAT_EQ(style.opacity, 0.4f);
    EXPECT_EQ(style.vertical, -1);

    ncl_math::Aabb3 lower[] = {{{32.0f, 32.0f, 256.0f}, {96.0f, 96.0f, 320.0f}}};
    EXPECT_EQ(RadarLayerMarker_ComputeVolumeStyle(view, lower).vertical, -1);
}

TEST(RadarSiteLayerCache, RefreshesVolumesAndExplicitlyResetMapAtSameAddress)
{
    RadarMapLayers map = TwoFloors();
    RadarSiteLayerCache cache;
    ncl_math::Aabb3 volume{{32.0f, 32.0f, 255.0f}, {96.0f, 96.0f, 257.0f}};
    cache.Prepare(map, {&volume, 1}, 0);
    RadarLayerView view = RadarLayerContext_ComputeView(map, 0, {64.0f, 64.0f, 1.0f}, true);
    EXPECT_EQ(cache.ComputeStyle(view).vertical, 1);
    volume = {{32.0f, 32.0f, -1.0f}, {96.0f, 96.0f, 1.0f}};
    cache.Prepare(map, {&volume, 1}, 0);
    EXPECT_FLOAT_EQ(cache.ComputeStyle(view).opacity, 1.0f);
    volume = {{32.0f, 32.0f, 255.0f}, {96.0f, 96.0f, 257.0f}};
    cache.Prepare(map, {&volume, 1}, 0);
    RadarMapLayerFixture fixture;
    fixture.AddFloor(512.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    cache.Reset();
    cache.Prepare(map, {&volume, 1}, 0);
    view = RadarLayerContext_ComputeView(map, 0, {64.0f, 64.0f, 513.0f}, true);
    EXPECT_FLOAT_EQ(cache.ComputeStyle(view).opacity, 1.0f);
    EXPECT_EQ(cache.ComputeStyle(view).vertical, 0);
}

TEST(RadarSiteLayerCache, RecomputesMissTieForActiveLayerAndKeepsEachFallback)
{
    RadarMapLayers map = TwoFloors();
    RadarSiteLayerCache cache;
    ncl_math::Aabb3 volumes[] = {
        {{1024.0f, 1024.0f, 126.0f}, {1056.0f, 1056.0f, 130.0f}},
        {{32.0f, 32.0f, -1.0f}, {96.0f, 96.0f, 1.0f}},
    };
    cache.Prepare(map, volumes, 0);
    EXPECT_FLOAT_EQ(cache.ComputeStyle(RadarLayerContext_ComputeView(map, 0, {64.0f, 64.0f, 1.0f}, true)).opacity, 1.0f);
    cache.Prepare(map, volumes, 1);
    EXPECT_FLOAT_EQ(cache.ComputeStyle(RadarLayerContext_ComputeView(map, 1, {64.0f, 64.0f, 257.0f}, true)).opacity, 1.0f);
    volumes[0] = {{1024.0f, 1024.0f, -1.0f}, {1056.0f, 1056.0f, 1.0f}};
    cache.Prepare(map, volumes, 1);
    EXPECT_EQ(cache.ComputeStyle(RadarLayerContext_ComputeView(map, 1, {64.0f, 64.0f, 257.0f}, true)).vertical, -1);
}

TEST(RadarSiteLayerCache, StylesUseCurrentPreparedSupportHeight)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {256.0f, 0.0f, 256.0f}, 256.0f);
    fixture.AddFloor(128.0f, {{512.0f, 0.0f}, {768.0f, 256.0f}});
    RadarMapLayers map;
    std::string warning;
    ASSERT_EQ(
        map.Build(
            std::move(fixture.geometry),
            {{{"ramp", -1.0f, 300.0f}, {"site", 400.0f, 500.0f}}, {{"site", {{512.0f, 0.0f, 127.0f}, {768.0f, 256.0f, 129.0f}}}}},
            warning
        ),
        RadarMapStatus::Ready
    );
    ASSERT_TRUE(warning.empty()) << warning;
    RadarSiteLayerCache cache;
    ncl_math::Aabb3 volume{{544.0f, 32.0f, 127.0f}, {608.0f, 96.0f, 129.0f}};
    int active = map.Locate({32.0f, 32.0f, 33.0f}, RadarLayerQuery::Below).layer;
    cache.Prepare(map, {&volume, 1}, active);
    EXPECT_EQ(cache.ComputeStyle(RadarLayerContext_ComputeView(map, active, {32.0f, 32.0f, 33.0f}, true)).vertical, 1);
    EXPECT_EQ(cache.ComputeStyle(RadarLayerContext_ComputeView(map, active, {128.0f, 32.0f, 128.0f}, true)).vertical, 0);
    EXPECT_EQ(cache.ComputeStyle(RadarLayerContext_ComputeView(map, active, {224.0f, 32.0f, 225.0f}, true)).vertical, -1);
}

TEST(RadarLayerContextPreparedView, DisabledMissingAndSlopedSupport)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {256.0f, 0.0f, 256.0f}, 256.0f);
    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    RadarLayerView view = RadarLayerContext_ComputeView(map, 0, {64.0f, 32.0f, 64.0f}, true);
    ASSERT_TRUE(view.active_support_z.has_value());
    EXPECT_FLOAT_EQ(*view.active_support_z, 64.0f);
    EXPECT_FALSE(RadarLayerContext_ComputeView(map, 0, {}, false).active_support_z.has_value());
    EXPECT_FALSE(RadarLayerContext_ComputeView(map, 10, {}, true).active_support_z.has_value());
}

TEST(RadarLayerMarker, PlayerIndicatorKeepsASmallGapForBothHeightDirections)
{
    RadarShape shape{{100.0f, 100.0f}, 100.0f, true};
    for (int dot : {2, 5, 12})
    {
        int extent = (int)std::ceil(dot * 2.4f);
        for (int vertical : {-1, 1})
        {
            RadarLayerPlacement placement = RadarLayerMarker_Place(shape, shape.center, extent, {0.4f, vertical}, dot);
            float gap = (placement.indicator - placement.marker).Length() - extent - dot * 0.5f;
            EXPECT_NEAR(gap, 1.0f, 0.001f);
            EXPECT_FLOAT_EQ(placement.indicator.x, placement.marker.x);
            EXPECT_LT((placement.indicator.y - placement.marker.y) * vertical, 0.0f);
        }
    }
}

TEST(RadarLayerMarker, PlayerAndSmallIndicatorFitAtEveryRadarEdge)
{
    for (bool round : {false, true})
    {
        RadarShape shape{{200.0f, 200.0f}, 200.0f, round};
        for (int dot : {2, 5, 12, 32})
        {
            int extent = (int)std::ceil(dot * 2.4f);
            for (int vertical : {-1, 1})
            {
                for (ncl_math::Vector2 raw : {ncl_math::Vector2{-500.0f, -500.0f}, {-500.0f, 800.0f}, {800.0f, -500.0f}, {800.0f, 800.0f}})
                {
                    RadarLayerPlacement placement = RadarLayerMarker_Place(shape, raw, extent, {0.4f, vertical}, dot);
                    EXPECT_TRUE(placement.offscreen);
                    for (int step = 0; step < 32; step++)
                    {
                        ncl_math::Vector2 direction = ncl_math::Vector2{1.0f, 0.0f}.Rotated(step * ncl_math::kPi / 16.0f);
                        EXPECT_TRUE(shape.Contains(placement.marker + direction * (float)extent));
                        EXPECT_TRUE(shape.Contains(placement.indicator + direction * (float)dot));
                    }
                }
            }
        }
    }
}

TEST(RadarLayerMarker, DefaultIndicatorKeepsASmallGapForBothHeightDirections)
{
    RadarShape shape{{100.0f, 100.0f}, 100.0f, true};
    for (int size : {3, 9, 24})
    {
        for (int vertical : {-1, 1})
        {
            RadarLayerPlacement placement = RadarLayerMarker_Place(shape, shape.center, size, {0.4f, vertical});
            float gap = (placement.indicator - placement.marker).Length() - size - size * 0.5f;
            EXPECT_NEAR(gap, 1.0f, 0.001f);
        }
    }
}

TEST(RadarLayerMarker, IndicatorOffsetUsesImageHeightWhileClampingKeepsTheFullExtent)
{
    for (bool round : {false, true})
    {
        RadarShape shape{{200.0f, 200.0f}, 200.0f, round};
        for (int vertical : {-1, 1})
        {
            for (ncl_math::Vector2 raw : {shape.center, ncl_math::Vector2{-500, -500}, {800, 800}})
            {
                int extent = 24;
                int indicator_size = 3;
                float half_height = 4.0f;
                RadarLayerPlacement placement = RadarLayerMarker_Place(shape, raw, extent, {0.4f, vertical}, indicator_size, half_height);
                float gap = (placement.indicator - placement.marker).Length() - half_height - indicator_size * 0.5f;
                EXPECT_NEAR(gap, 1.0f, 0.001f);
                for (int step = 0; step < 32; step++)
                {
                    ncl_math::Vector2 direction = ncl_math::Vector2{1.0f, 0.0f}.Rotated(step * ncl_math::kPi / 16.0f);
                    EXPECT_TRUE(shape.Contains(placement.marker + direction * (float)extent));
                    EXPECT_TRUE(shape.Contains(placement.indicator + direction * (float)indicator_size));
                }
            }
        }
    }
}

TEST(RadarLayerMarker, PlayerPlacementKeepsCallerInsetsAndNormalizesFacing)
{
    for (bool round : {false, true})
    {
        RadarShape shape{{200, 200}, 200, round};

        for (int size : {2, 5, 12, 32})
        {
            for (int extent : {size * 2, (int)std::ceil(size * 2.4f)})
            {
                for (int vertical : {-1, 0, 1})
                {
                    for (bool directed : {false, true})
                    {
                        ncl_math::Vector2 direction = directed ? ncl_math::Vector2{0, 10} : ncl_math::Vector2{};
                        RadarPlayerPlacement result = RadarLayerMarker_PlacePlayer(
                            shape,
                            {399, 200},
                            direction,
                            size,
                            extent,
                            {0.4f, vertical}
                        );

                        float half_height = directed && vertical == -1 ? size * 2.0f : (float)size;
                        float offset = vertical == 0 ? 0.0f : half_height + size * 0.5f + 1.0f;
                        int inset = (int)std::ceil(std::max((float)extent, offset + size));

                        EXPECT_FALSE(result.placement.offscreen);
                        EXPECT_EQ(result.out_dir, (ncl_math::Vector2{}));

                        EXPECT_EQ(result.placement.marker, (ncl_math::Vector2{400.0f - inset, 200}));
                        EXPECT_FLOAT_EQ(result.placement.indicator.x, result.placement.marker.x);

                        EXPECT_FLOAT_EQ(result.placement.indicator.y, 200 - vertical * offset);
                    }
                }
            }
        }
    }
}

TEST(RadarLayerMarker, PlayerPlacementUsesRadialArrowHeightAtEveryCorner)
{
    const float diagonal = std::sqrt(0.5f);

    for (bool round : {false, true})
    {
        RadarShape shape{{200, 200}, 200, round};

        for (int size : {2, 5, 12, 32})
        {
            int extent = (int)std::ceil(size * 2.4f);

            for (int vertical : {-1, 1})
            {
                for (int x : {-1, 1})
                {
                    for (int y : {-1, 1})
                    {
                        ncl_math::Vector2 raw = shape.center + ncl_math::Vector2{700.0f * x, 700.0f * y};
                        RadarPlayerPlacement result = RadarLayerMarker_PlacePlayer(
                            shape,
                            raw,
                            {0, 10},
                            size,
                            extent,
                            {0.4f, vertical}
                        );

                        float half_height = size * diagonal * (vertical * y > 0 ? 2.0f : 1.7f);
                        float offset = half_height + size * 0.5f + 1.0f;
                        int inset = (int)std::ceil(std::max((float)extent, offset + size));
                        float limit = (200 - inset) * (round ? diagonal : 1.0f);

                        EXPECT_TRUE(result.placement.offscreen);
                        EXPECT_NEAR(result.out_dir.x, x * diagonal, 0.0001f);

                        EXPECT_NEAR(result.out_dir.y, y * diagonal, 0.0001f);
                        EXPECT_NEAR(result.placement.marker.x, 200 + x * limit, 0.0001f);

                        EXPECT_NEAR(result.placement.marker.y, 200 + y * limit, 0.0001f);

                        float gap = std::abs(result.placement.indicator.y - result.placement.marker.y) - half_height - size * 0.5f;

                        EXPECT_NEAR(gap, 1.0f, 0.0001f);
                    }
                }
            }
        }
    }
}

TEST(RadarLayerMarker, PlayerPlacementKeepsQuestionMarkOverrides)
{
    for (bool round : {false, true})
    {
        RadarShape shape{{200, 200}, 200, round};

        for (int vertical : {-1, 1})
        {
            for (ncl_math::Vector2 raw : {shape.center, ncl_math::Vector2{900, 200}})
            {
                RadarPlayerPlacement result = RadarLayerMarker_PlacePlayer(
                    shape,
                    raw,
                    {},
                    5,
                    24,
                    {0.4f, vertical},
                    3,
                    4.0f
                );

                EXPECT_EQ(result.placement.offscreen, raw.x == 900);
                EXPECT_EQ(result.out_dir, (raw.x == 900 ? ncl_math::Vector2{1, 0} : ncl_math::Vector2{}));

                EXPECT_EQ(result.placement.marker, (raw.x == 900 ? ncl_math::Vector2{376, 200} : shape.center));
                EXPECT_FLOAT_EQ(result.placement.indicator.y, 200 - vertical * 6.5f);
            }
        }
    }
}
