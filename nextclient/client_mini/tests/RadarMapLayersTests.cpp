#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <taskcoro/CancellationToken.h>

#include "RadarMapLayerFixtures.h"
#include "hud/radar/map/layers/RadarMapLayers.h"

TEST(RadarMapLayers, StackedFloorsRemainSeparate)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 257.0f}, RadarLayerQuery::Below).layer, 1);
}

TEST(RadarMapLayers, ReusedVolumeOutputContainsUniqueLayersAndClearsMisses)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -256.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(256.0f, {{-256.0f, -256.0f}, {256.0f, 256.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);

    std::vector<int> layers{99};
    map.Intersect({{-64.0f, -64.0f, -1.0f}, {64.0f, 64.0f, 257.0f}}, layers);

    EXPECT_EQ(layers, (std::vector<int>{0, 1}));

    map.Intersect({{-64.0f, -64.0f, 256.0f}, {64.0f, 64.0f, 256.0f}}, layers);

    EXPECT_EQ(layers, (std::vector<int>{1}));

    map.Intersect({{256.0f, -64.0f, -1.0f}, {288.0f, 64.0f, 257.0f}}, layers);

    EXPECT_TRUE(layers.empty());

    map.Intersect({{-64.0f, -64.0f, -1.0f}, {64.0f, 64.0f, 1.0f}}, layers);

    EXPECT_EQ(layers, (std::vector<int>{0}));

    map.Intersect({{1024.0f, 1024.0f, -1.0f}, {2048.0f, 2048.0f, 257.0f}}, layers);

    EXPECT_TRUE(layers.empty());

    map.Reset();
    layers.push_back(99);
    map.Intersect({{-64.0f, -64.0f, -1.0f}, {64.0f, 64.0f, 257.0f}}, layers);

    EXPECT_TRUE(layers.empty());
}

TEST(RadarMapLayers, SubdivisionPreservesAssignment)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            fixture.AddFloor(256.0f, {{(float)x * 32.0f, (float)y * 32.0f}, {(float)(x + 1) * 32.0f, (float)(y + 1) * 32.0f}});
        }
    }

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(map.get_layers()[0].area, 262144.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[1].area, 262144.0f);
    EXPECT_EQ(map.Locate({17.0f, 113.0f, 257.0f}, RadarLayerQuery::Below).layer, 1);
}

TEST(RadarMapLayers, EdgeContactIsNotVerticalOverlap)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(16.0f, {{256.0f, 0.0f}, {512.0f, 256.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_EQ(map.Locate({300.0f, 64.0f, 17.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarMapLayers, SmallRiseJoinsRoom)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(16.0f, {{256.0f, 0.0f}, {320.0f, 64.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);

    RadarLayerMatch match = map.Locate({288.0f, 32.0f, 17.0f}, RadarLayerQuery::Below);
    EXPECT_EQ(match.layer, 0);
    EXPECT_FLOAT_EQ(match.support.z, 16.0f);
}

TEST(RadarMapLayers, StairChainDoesNotCollapseFloors)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, 0.0f}, {0.0f, 256.0f}});
    fixture.AddFloor(256.0f, {{512.0f, 0.0f}, {768.0f, 256.0f}});
    fixture.AddStairs(0.0f, 256.0f, 16, {{0.0f, 0.0f}, {512.0f, 64.0f}});
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_EQ(map.Locate({64.0f, 32.0f, 49.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({448.0f, 32.0f, 241.0f}, RadarLayerQuery::Below).layer, 1);
    ASSERT_EQ(map.get_analysis().transitions.size(), 1u);
    EXPECT_FLOAT_EQ(map.get_analysis().transitions[0].height, 128.0f);
}

namespace
{
    float ProjectedArea(const RadarMapSurface& surface)
    {
        float area = 0.0f;

        for (size_t i = 0; i < surface.vertices.size(); i++)
        {
            const ncl_math::Vector3& a = surface.vertices[i].pos;
            const ncl_math::Vector3& b = surface.vertices[(i + 1) % surface.vertices.size()].pos;
            area += a.x * b.y - b.x * a.y;
        }

        return std::abs(area * 0.5f);
    }

    float WallArea(const RadarMapSurface& surface)
    {
        float area = 0.0f;

        for (size_t i = 1; i + 1 < surface.vertices.size(); i++)
        {
            area +=
                (surface.vertices[i].pos - surface.vertices[0].pos).Cross(surface.vertices[i + 1].pos - surface.vertices[0].pos).Length() *
                0.5f;
        }

        return area;
    }
} // namespace

namespace
{
    int AddWall(RadarMapLayerFixture& fixture, float minimum_x, float maximum_x, float y, float minimum_z, float maximum_z)
    {
        RadarMapSurface wall{};
        wall.surface_id = (int)fixture.geometry.surfaces.size();
        wall.normal = {0.0f, -1.0f, 0.0f};
        wall.vertices = {
            {{minimum_x, y, minimum_z}, {0.0f, 0.0f}, {0.0f, 0.0f}},
            {{maximum_x, y, minimum_z}, {1.0f, 0.0f}, {1.0f, 0.0f}},
            {{maximum_x, y, maximum_z}, {1.0f, 1.0f}, {1.0f, 1.0f}},
            {{minimum_x, y, maximum_z}, {0.0f, 1.0f}, {0.0f, 1.0f}},
        };
        int source = wall.surface_id;
        fixture.geometry.bounds.Include({minimum_x, y});
        fixture.geometry.bounds.Include({maximum_x, y});
        fixture.geometry.surfaces.push_back(std::move(wall));

        return source;
    }

    float DetailArea(const RadarMapLayers& map, int layer, int source)
    {
        float area = 0.0f;

        for (const RadarMapSurface& surface : map.get_surfaces(layer))
        {
            if (surface.surface_id == source)
            {
                area += WallArea(surface);
            }
        }

        return area;
    }
} // namespace

TEST(RadarMapLayers, NegativeCoordinatesUseFloorDivision)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-35.0f, -35.0f}, {-31.0f, -31.0f}});
    fixture.AddFloor(256.0f, {{-35.0f, -35.0f}, {-31.0f, -31.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.Locate({-34.0f, -34.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({-31.5f, -31.5f, 257.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_FLOAT_EQ(map.Locate({-31.5f, -31.5f, 257.0f}, RadarLayerQuery::Below).support.z, 256.0f);
}

TEST(RadarMapLayers, SmallAreasRemainOneLayer)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {32.0f, 32.0f}});
    fixture.AddFloor(16.0f, {{256.0f, 0.0f}, {288.0f, 32.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_EQ(map.Locate({16.0f, 16.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({272.0f, 16.0f, 17.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarMapLayers, EmptyGeometryFailsOnce)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {32.0f, 32.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.Build({}), RadarMapStatus::Failed);
    EXPECT_EQ(map.get_analysis().status, RadarMapStatus::Failed);
    EXPECT_TRUE(map.get_layers().empty());
    EXPECT_EQ(map.Locate({16.0f, 16.0f, 1.0f}, RadarLayerQuery::Nearest).layer, -1);

    map.Reset();

    EXPECT_EQ(map.get_analysis().status, RadarMapStatus::Waiting);
    EXPECT_TRUE(map.get_analysis().transitions.empty());
}

TEST(RadarMapLayers, PermutingInputKeepsIdsAndAreas)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(16.0f, {{256.0f, 0.0f}, {320.0f, 64.0f}});
    fixture.AddOverlapRoom(0, 1);
    RadarMapGeometry permuted = fixture.geometry;
    std::reverse(permuted.surfaces.begin(), permuted.surfaces.end());
    RadarMapLayers first;
    RadarMapLayers second;

    ASSERT_EQ(first.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(second.Build(std::move(permuted)), RadarMapStatus::Ready);
    ASSERT_EQ(first.get_layers().size(), 2u);
    ASSERT_EQ(second.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(first.get_layers()[0].area, 200704.0f);
    EXPECT_FLOAT_EQ(second.get_layers()[0].area, 200704.0f);
    EXPECT_EQ(first.Locate({64.0f, 64.0f, 257.0f}, RadarLayerQuery::Below).layer, 1);
    EXPECT_EQ(second.Locate({64.0f, 64.0f, 257.0f}, RadarLayerQuery::Below).layer, 1);
    EXPECT_EQ(second.Locate({288.0f, 32.0f, 17.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarMapLayers, StairMidpointDoesNotDuplicatePositiveArea)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, 0.0f}, {0.0f, 256.0f}});
    fixture.AddFloor(256.0f, {{512.0f, 0.0f}, {768.0f, 256.0f}});
    fixture.AddStairs(0.0f, 256.0f, 16, {{0.0f, 0.0f}, {512.0f, 64.0f}});
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);

    float total = 0.0f;

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        total += layer.area;
    }

    EXPECT_FLOAT_EQ(total, 688128.0f);
    EXPECT_EQ(map.Locate({240.0f, 32.0f, 129.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarMapLayers, PositivePolygonOverlapBlocksMerging)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(16.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 17.0f}, RadarLayerQuery::Below).layer, 1);
}

TEST(RadarMapLayers, OverlappingBoundsWithoutPolygonOverlapCanMerge)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(16.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    RadarMapSurface& lower = fixture.geometry.surfaces[0];
    lower.vertices = {{{0.0f, 0.0f, 0.0f}}, {{256.0f, 0.0f, 0.0f}}, {{0.0f, 256.0f, 0.0f}}};
    RadarMapSurface& upper = fixture.geometry.surfaces[1];
    upper.vertices = {{{256.0f, 256.0f, 16.0f}}, {{0.0f, 256.0f, 16.0f}}, {{256.0f, 0.0f, 16.0f}}};

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_EQ(map.Locate({240.0f, 240.0f, 17.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarMapLayers, TerrainHeightSpanDoesNotSplitLayers)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(48.0f, {{128.0f, 0.0f}, {256.0f, 128.0f}});
    fixture.AddFloor(96.0f, {{256.0f, 0.0f}, {384.0f, 128.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_EQ(map.Locate({192.0f, 64.0f, 49.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({320.0f, 64.0f, 97.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarMapLayers, CollisionBackedRampKeepsGeometricSupportHeight)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {256.0f, 0.0f, 128.0f}, 64.0f);
    fixture.AddSupportHull(0);

    EXPECT_FALSE(MapGeometry_CheckClearance(fixture.geometry, {64.0f, 0.0f, 32.0f}));

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);

    RadarLayerMatch match = map.Locate({64.0f, 0.0f, 33.0f}, RadarLayerQuery::Below);
    EXPECT_EQ(match.layer, 0);
    EXPECT_FLOAT_EQ(match.support.z, 32.0f);
}

TEST(RadarMapLayers, RampPartitionsAtEndpointMidpoint)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(map.get_layers()[0].area, 344064.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[1].area, 344064.0f);
    EXPECT_EQ(map.Locate({192.0f, 0.0f, 97.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({320.0f, 0.0f, 161.0f}, RadarLayerQuery::Below).layer, 1);
    EXPECT_FLOAT_EQ(map.get_analysis().transitions[0].height, 128.0f);
}

TEST(RadarMapLayers, SkyAndBlockedSupportsAreExcludedButTiledFloorRemains)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.surfaces.back().tiled = true;
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.surfaces.back().sky = true;
    fixture.AddFloor(512.0f, {{256.0f, 0.0f}, {384.0f, 128.0f}});
    fixture.AddSolidBox({{240.0f, -16.0f, 532.0f}, {400.0f, 144.0f, 536.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 257.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({320.0f, 64.0f, 513.0f}, RadarLayerQuery::Below).layer, -1);
    EXPECT_TRUE(std::none_of(map.get_surfaces(0).begin(), map.get_surfaces(0).end(), [](const RadarMapSurface& surface) {
        return surface.sky;
    }));

    EXPECT_TRUE(std::any_of(map.get_surfaces(0).begin(), map.get_surfaces(0).end(), [](const RadarMapSurface& surface) {
        return surface.surface_id == 0 && surface.tiled;
    }));
}

TEST(RadarMapLayers, VisibleDetailsKeepWallsAndSlopesButExcludeCeilings)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.surfaces.back().tiled = true;
    fixture.AddFloor(128.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.surfaces.back().normal = {0.0f, 0.0f, -1.0f};
    int ceiling = fixture.geometry.surfaces.back().surface_id;
    int wall = AddWall(fixture, 16.0f, 112.0f, 96.0f, 0.0f, 128.0f);
    RadarMapSurface slope{};
    slope.surface_id = (int)fixture.geometry.surfaces.size();
    slope.normal = {-0.8f, 0.0f, 0.6f};
    slope.vertices = {
        {{16.0f, 16.0f, 20.0f}, {}, {}},
        {{64.0f, 16.0f, 84.0f}, {}, {}},
        {{64.0f, 64.0f, 84.0f}, {}, {}},
        {{16.0f, 64.0f, 20.0f}, {}, {}},
    };
    int upward_detail = slope.surface_id;
    fixture.geometry.surfaces.push_back(std::move(slope));

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, ceiling), 0.0f);
    EXPECT_GT(DetailArea(map, 0, wall), 0.0f);
    EXPECT_GT(DetailArea(map, 0, upward_detail), 0.0f);
    EXPECT_TRUE(std::none_of(map.get_surfaces(0).begin(), map.get_surfaces(0).end(), [](const RadarMapSurface& surface) {
        return surface.sky || surface.normal.z < 0.0f;
    }));

    EXPECT_TRUE(std::any_of(map.get_surfaces(0).begin(), map.get_surfaces(0).end(), [](const RadarMapSurface& surface) {
        return surface.surface_id == 0 && surface.tiled;
    }));
}

TEST(RadarMapLayers, RenderExcludesDownwardBevelsAndKeepsWallsAndUpwardSlopes)
{
    for (size_t layer_count : {1u, 2u})
    {
        SCOPED_TRACE(layer_count);
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

        if (layer_count == 2)
        {
            fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        }

        fixture.AddRamp({64.0f, 128.0f, 64.0f}, {80.0f, 128.0f, 96.0f}, 64.0f);
        int upward = fixture.geometry.surfaces.back().surface_id;
        RadarMapSurface bevel = fixture.geometry.surfaces.back();
        bevel.surface_id = (int)fixture.geometry.surfaces.size();
        bevel.normal = -bevel.normal;
        std::reverse(bevel.vertices.begin(), bevel.vertices.end());
        int downward = bevel.surface_id;
        ASSERT_GT(bevel.normal.z, -0.5f);
        ASSERT_LT(bevel.normal.z, 0.0f);
        fixture.geometry.surfaces.push_back(std::move(bevel));
        int wall = AddWall(fixture, 32.0f, 160.0f, 192.0f, 0.0f, 128.0f);

        RadarMapLayers map;
        ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        ASSERT_EQ(map.get_layers().size(), layer_count);

        float upward_area = 0.0f;
        float wall_area = 0.0f;

        for (const RadarLayerInfo& layer : map.get_layers())
        {
            EXPECT_FLOAT_EQ(DetailArea(map, layer.id, downward), 0.0f);
            upward_area += DetailArea(map, layer.id, upward);
            wall_area += DetailArea(map, layer.id, wall);
        }

        EXPECT_GT(upward_area, 0.0f);
        EXPECT_GT(wall_area, 0.0f);
    }
}

TEST(RadarMapLayers, NearestUsesGeometryAndPreferredOnlyForTies)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {32.0f, 32.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {32.0f, 32.0f}});
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.Locate({16.0f, 16.0f, 128.0f}, RadarLayerQuery::Nearest).layer, 0);
    EXPECT_EQ(map.Locate({16.0f, 16.0f, 128.0f}, RadarLayerQuery::Nearest, 1).layer, 1);
    EXPECT_EQ(map.Locate({16.0f, 16.0f, 1.0f}, RadarLayerQuery::Nearest, 1).layer, 0);

    RadarLayerMatch outside = map.Locate({40.0f, 16.0f, 1.0f}, RadarLayerQuery::Nearest);
    EXPECT_FALSE(outside.contained);
    EXPECT_EQ(outside.support, (ncl_math::Vector3{32.0f, 16.0f, 0.0f}));
    EXPECT_EQ(map.Locate({40.0f, 16.0f, 1.0f}, RadarLayerQuery::Below).layer, -1);
}

TEST(RadarMapLayers, DetailsUseLocalSupportVolumeWithoutDuplicates)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    RadarMapSurface wall{};
    wall.surface_id = 2;
    wall.normal = {1.0f, 0.0f, 0.0f};
    wall.vertices = {{{64.0f, 0.0f, 0.0f}}, {{64.0f, 128.0f, 0.0f}}, {{64.0f, 128.0f, 256.0f}}, {{64.0f, 0.0f, 256.0f}}};
    fixture.geometry.surfaces.push_back(std::move(wall));

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;
    ASSERT_EQ(map.get_layers().size(), 2u);

    for (int layer = 0; layer < 2; layer++)
    {
        float area = 0.0f;

        for (const RadarMapSurface& surface : map.get_surfaces(layer))
        {
            if (surface.surface_id != 2)
            {
                continue;
            }

            area += WallArea(surface);

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                EXPECT_GE(vertex.pos.z, layer == 0 ? 0.0f : 256.0f);
                EXPECT_LE(vertex.pos.z, 256.0f);
            }
        }

        EXPECT_FLOAT_EQ(area, layer == 0 ? 32768.0f : 0.0f);
    }

    EXPECT_TRUE(map.get_surfaces(-1).empty());
    EXPECT_TRUE(map.get_surfaces(2).empty());
    EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{128.0f, 128.0f}));
}

TEST(RadarMapLayers, TransitionKeepsOverlapEndpointHeights)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddFloor(16.0f, {{-256.0f, 128.0f}, {-192.0f, 192.0f}});
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    ASSERT_EQ(map.get_analysis().transitions.size(), 1u);
    EXPECT_NEAR(map.get_layers()[0].height, 0.19753087f, 0.000001f);
    EXPECT_FLOAT_EQ(map.get_analysis().transitions[0].height, 128.0f);
    EXPECT_EQ(map.Locate({255.5f, 0.0f, 129.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({257.5f, 0.0f, 130.0f}, RadarLayerQuery::Below).layer, 1);
}

TEST(RadarMapLayers, NarrowFootprintKeepsEmptyCellPixelsUnassigned)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{4.0f, 0.0f}, {8.0f, 32.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.Locate({6.0f, 16.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({16.0f, 16.0f, 1.0f}, RadarLayerQuery::Below).layer, -1);
    EXPECT_FLOAT_EQ(map.get_layers()[0].area, 128.0f);
}

TEST(RadarMapLayers, RampFragmentsPreserveSourceFlagsAndUvs)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.geometry.surfaces.back().tiled = true;
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    for (int layer = 0; layer < 2; layer++)
    {
        float area = 0.0f;
        bool boundary = false;

        for (const RadarMapSurface& surface : map.get_surfaces(layer))
        {
            if (surface.surface_id != 2)
            {
                continue;
            }

            EXPECT_TRUE(surface.tiled);
            EXPECT_FALSE(surface.sky);

            area += ProjectedArea(surface);

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                EXPECT_EQ(vertex.uv, vertex.lightmap_uv);

                if (vertex.pos.z == 128.0f)
                {
                    boundary = true;

                    EXPECT_FLOAT_EQ(vertex.uv.x, 0.5f);
                }
            }
        }

        EXPECT_TRUE(boundary);
        EXPECT_FLOAT_EQ(area, 16384.0f);
    }
}

TEST(RadarMapLayers, DistantNarrowPolygonRetainsItsArea)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{30000.0f, 30000.0f}, {30004.0f, 30004.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(map.get_layers()[0].area, 16.0f);
    EXPECT_EQ(map.Locate({30002.0f, 30002.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarMapLayers, ConflictingAdjacentAreaRemainsIndependent)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(80.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(40.0f, {{96.0f, 0.0f}, {512.0f, 512.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 3u);

    RadarLayerMatch lower = map.Locate({112.0f, 16.0f, 1.0f}, RadarLayerQuery::Below);
    RadarLayerMatch middle = map.Locate({112.0f, 16.0f, 41.0f}, RadarLayerQuery::Below);
    RadarLayerMatch upper = map.Locate({112.0f, 16.0f, 81.0f}, RadarLayerQuery::Below);

    EXPECT_NE(lower.layer, middle.layer);
    EXPECT_NE(middle.layer, upper.layer);
    EXPECT_NE(lower.layer, upper.layer);
    EXPECT_TRUE(map.get_analysis().transitions.empty());
}

TEST(RadarMapLayers, PartialUpperFootprintConservesDetailArea)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {8.0f, 32.0f}});
    int wall = AddWall(fixture, 0.0f, 32.0f, 16.0f, 0.0f, 256.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall) + DetailArea(map, 1, wall), 8192.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall), 8192.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 1, wall), 0.0f);
}

TEST(RadarMapLayers, UpperLayerExcludesDetailBelowItsSupport)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    int wall = AddWall(fixture, 0.0f, 32.0f, 16.0f, 136.0f, 240.0f);
    RadarMapLayers map;

    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall), 3328.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 1, wall), 0.0f);
}

TEST(RadarMapLayers, PartialUpperFootprintCapsOnlyItsOwnRegion)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {8.0f, 32.0f}});
    int wall = AddWall(fixture, 0.0f, 32.0f, 16.0f, 0.0f, 512.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall), 14336.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 1, wall), 2048.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall) + DetailArea(map, 1, wall), 16384.0f);
}

TEST(RadarMapLayers, SlopedSupportPlanesBoundDetails)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 16.0f, 0.0f}, {128.0f, 16.0f, 64.0f}, 32.0f);
    fixture.AddRamp({0.0f, 16.0f, 256.0f}, {128.0f, 16.0f, 320.0f}, 32.0f);
    int wall = AddWall(fixture, 0.0f, 32.0f, 16.0f, -32.0f, 384.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall), 8192.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 1, wall), 3840.0f);

    for (int layer = 0; layer < 2; layer++)
    {
        for (const RadarMapSurface& surface : map.get_surfaces(layer))
        {
            if (surface.surface_id != wall)
            {
                continue;
            }

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                EXPECT_GE(vertex.pos.z + 0.00001f, vertex.pos.x * 0.5f + (layer == 0 ? 0.0f : 256.0f));

                if (layer == 0)
                {
                    EXPECT_LE(vertex.pos.z, vertex.pos.x * 0.5f + 256.00001f);
                }

                EXPECT_EQ(vertex.uv, vertex.lightmap_uv);
            }
        }
    }
}

TEST(RadarMapLayers, PartialSlopedUpperFootprintUsesItsPlane)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddRamp({0.0f, 16.0f, 256.0f}, {8.0f, 16.0f, 260.0f}, 32.0f);
    int wall = AddWall(fixture, 0.0f, 32.0f, 16.0f, 0.0f, 512.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall), 14352.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 1, wall), 2032.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall) + DetailArea(map, 1, wall), 16384.0f);
}

TEST(RadarMapLayers, TransitionComponentKeepsInternallyConflictingSupportsSeparate)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 256.0f);
    fixture.AddRamp({0.0f, 0.0f, 64.0f}, {512.0f, 0.0f, 320.0f}, 256.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 2u);
    EXPECT_NE(
        map.Locate({16.0f, 24.0f, 9.0f}, RadarLayerQuery::Below).layer, map.Locate({16.0f, 24.0f, 73.0f}, RadarLayerQuery::Below).layer
    );

    EXPECT_TRUE(map.get_analysis().transitions.empty());
}

namespace
{
    int LayerNamed(const RadarMapLayers& map, const std::string& name)
    {
        for (const RadarLayerInfo& layer : map.get_layers())
        {
            if (layer.name == name)
            {
                return layer.id;
            }
        }

        return -1;
    }
} // namespace

TEST(RadarMapLayers, CorrectionsPreserveClippedAreaFlagsAndUvs)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {128.0f, 0.0f, 64.0f}, 32.0f);
    fixture.geometry.surfaces.back().tiled = true;

    RadarLayerOverrides overrides{
        {{"base", -512.0f, 512.0f}, {"region", 512.0f, 1024.0f}},
        {{"region", {{20.0f, -8.0f, 16.0f}, {100.0f, 8.0f, 48.0f}}}},
    };
    RadarMapLayers map;
    std::string error;

    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;

    int base = LayerNamed(map, "base");
    int region = LayerNamed(map, "region");
    ASSERT_GE(base, 0);
    ASSERT_GE(region, 0);
    EXPECT_FLOAT_EQ(map.get_layers()[base].area, 3072.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[region].area, 1024.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[base].height, 32.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[region].height, 32.0f);

    float total = 0.0f;

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        for (const RadarMapSurface& surface : map.get_surfaces(layer.id))
        {
            total += ProjectedArea(surface);

            EXPECT_TRUE(surface.tiled);
            EXPECT_FALSE(surface.sky);
            EXPECT_EQ(surface.surface_id, 0);

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                EXPECT_NEAR(vertex.uv.x, vertex.pos.x / 128.0f, 0.00001f);
                EXPECT_NEAR(vertex.uv.y, (vertex.pos.y + 16.0f) / 32.0f, 0.00001f);
                EXPECT_EQ(vertex.uv, vertex.lightmap_uv);
            }
        }
    }

    EXPECT_FLOAT_EQ(total, 4096.0f);
}

TEST(RadarMapLayers, CorrectionsRetainUncoveredAutomaticAssignment)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(-128.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});

    RadarMapLayers map;
    std::string error;

    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_TRUE(map.ApplyOverrides({{{"manual", -16.0f, 16.0f}}, {}}, error));

    int automatic = LayerNamed(map, "Layer 1");
    ASSERT_GE(automatic, 0);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, -127.0f}, RadarLayerQuery::Below).layer, automatic);
    EXPECT_FLOAT_EQ(map.get_layers()[automatic].area, 16384.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[automatic].height, -128.0f);
}

TEST(RadarMapLayers, ManualMergeRecordsVerticalConflict)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});

    RadarMapLayers map;
    std::string error;

    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_TRUE(map.ApplyOverrides({{{"merged", -512.0f, 512.0f}}, {}}, error));
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(map.get_layers()[0].height, 128.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[0].area, 32768.0f);
    EXPECT_FALSE(map.get_analysis().manual_conflicts.empty());
}

TEST(RadarMapLayers, CorrectedTransitionsKeepUncoveredAutomaticBoundary)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    std::string error;

    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", 0.0f, 64.0f}, {"upper", 256.0f, 512.0f}}, {}}, error));

    int automatic_lower = LayerNamed(map, "Layer 1");
    int automatic_upper = LayerNamed(map, "Layer 2");
    ASSERT_GE(automatic_lower, 0);
    ASSERT_GE(automatic_upper, 0);
    EXPECT_FLOAT_EQ(map.get_layers()[automatic_lower].height, 96.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[automatic_upper].height, 192.0f);
    EXPECT_EQ(map.Locate({250.0f, 0.0f, 126.0f}, RadarLayerQuery::Below).layer, automatic_lower);
    EXPECT_EQ(map.Locate({270.0f, 0.0f, 136.0f}, RadarLayerQuery::Below).layer, automatic_upper);
    ASSERT_EQ(map.get_analysis().transitions.size(), 3u);

    bool found = false;

    for (const RadarLayerTransition& transition : map.get_analysis().transitions)
    {
        EXPECT_LT(transition.lower, transition.upper);

        if (transition.lower == automatic_lower && transition.upper == automatic_upper)
        {
            found = true;

            EXPECT_FLOAT_EQ(transition.height, 128.0f);
            EXPECT_EQ(transition.surface_ids, (std::vector<int>{2}));
        }
    }

    EXPECT_TRUE(found);

    ASSERT_TRUE(map.ApplyOverrides({{{"merged", -512.0f, 512.0f}}, {}}, error));
    EXPECT_TRUE(map.get_analysis().transitions.empty());
}

TEST(RadarMapLayers, CorrectedDetailBoundaryUsesHalfOpenMembership)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    int wall = AddWall(fixture, 0.0f, 128.0f, 64.0f, 0.0f, 512.0f);
    RadarLayerOverrides overrides{
        {{"lower", -512.0f, 256.0f}, {"upper", 256.0f, 512.0f}},
        {{"upper", {{0.0f, 0.0f, -16.0f}, {128.0f, 64.0f, 16.0f}}}},
    };
    RadarMapLayers map;
    std::string error;

    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;

    int lower = LayerNamed(map, "lower");
    int upper = LayerNamed(map, "upper");
    ASSERT_GE(lower, 0);
    ASSERT_GE(upper, 0);
    EXPECT_FLOAT_EQ(DetailArea(map, lower, wall), 32768.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, upper, wall), 32768.0f);
}

TEST(RadarMapLayers, EmptyCorrectionsRestoreAutomaticTransitionMetadata)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    std::string error;

    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_TRUE(map.ApplyOverrides({{{"merged", -512.0f, 512.0f}}, {}}, error));
    ASSERT_TRUE(map.ApplyOverrides({}, error));
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_FLOAT_EQ(map.get_layers()[0].height, 0.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[1].height, 256.0f);
    ASSERT_EQ(map.get_analysis().transitions.size(), 1u);
    EXPECT_FLOAT_EQ(map.get_analysis().transitions[0].height, 128.0f);
}

TEST(RadarMapLayers, ManualRangesCreateTransitionOnIsolatedRamp)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {128.0f, 0.0f, 64.0f}, 32.0f);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    ASSERT_TRUE(map.get_analysis().transitions.empty());

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 32.0f}, {"upper", 32.0f, 128.0f}}, {}}, error)) << error;

    int lower = LayerNamed(map, "lower");
    int upper = LayerNamed(map, "upper");
    ASSERT_GE(lower, 0);
    ASSERT_GE(upper, 0);
    EXPECT_FLOAT_EQ(map.get_layers()[lower].area, 2048.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[upper].area, 2048.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[lower].height, 16.0f);
    EXPECT_FLOAT_EQ(map.get_layers()[upper].height, 48.0f);
    ASSERT_EQ(map.get_analysis().transitions.size(), 1u);

    const RadarLayerTransition& transition = map.get_analysis().transitions[0];
    EXPECT_EQ(transition.lower, lower);
    EXPECT_EQ(transition.upper, upper);
    EXPECT_FLOAT_EQ(transition.height, 32.0f);
    EXPECT_EQ(transition.surface_ids, (std::vector<int>{0}));
}

TEST(RadarMapLayers, RegionClippingPreservesAcceptedSmallSupport)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    ASSERT_NEAR(map.get_layers()[0].area, 0.000324f, 0.000000001f);

    RadarLayerOverrides overrides{
        {{"manual", 128.0f, 256.0f}},
        {{"manual", {{0.005f, 0.005f, -1.0f}, {0.013f, 0.013f, 1.0f}}}},
    };
    std::string error;

    ASSERT_TRUE(MapLayerOverrides_Validate(overrides, error)) << error;

    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
    EXPECT_EQ(map.get_analysis().status, RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 2u);

    float total = 0.0f;

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        for (const RadarMapSurface& surface : map.get_surfaces(layer.id))
        {
            EXPECT_EQ(surface.surface_id, 0);

            total += ProjectedArea(surface);

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                EXPECT_NEAR(vertex.uv.x, vertex.pos.x / 0.018f, 0.00001f);
                EXPECT_NEAR(vertex.uv.y, vertex.pos.y / 0.018f, 0.00001f);
                EXPECT_EQ(vertex.uv, vertex.lightmap_uv);
            }
        }
    }

    EXPECT_NEAR(total, 0.000324f, 0.000000001f);
    EXPECT_GE(map.Locate({0.009f, 0.009f, 1.0f}, RadarLayerQuery::Below).layer, 0);

    int manual = LayerNamed(map, "manual");
    int automatic = LayerNamed(map, "Layer 1");
    ASSERT_GE(manual, 0);
    ASSERT_GE(automatic, 0);
    EXPECT_NEAR(map.get_layers()[manual].area, 0.000064f, 0.000000001f);
    EXPECT_NEAR(map.get_layers()[automatic].area, 0.000260f, 0.000000001f);
    EXPECT_EQ(map.Locate({0.009f, 0.009f, 1.0f}, RadarLayerQuery::Below).layer, manual);
    EXPECT_EQ(map.Locate({0.002f, 0.002f, 1.0f}, RadarLayerQuery::Below).layer, automatic);
    EXPECT_EQ(map.Intersect({{0.005f, 0.005f, -1.0f}, {0.013f, 0.013f, 1.0f}}), (std::vector<int>{manual}));
    EXPECT_EQ(map.Intersect({{0.013f, 0.005f, -1.0f}, {0.018f, 0.013f, 1.0f}}), (std::vector<int>{automatic}));
}

TEST(RadarMapLayers, HeightClippingPreservesAcceptedSmallSupport)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {0.018f, 0.0f, 0.009f}, 0.018f);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    ASSERT_NEAR(map.get_layers()[0].area, 0.000324f, 0.000000001f);

    RadarLayerOverrides overrides{
        {{"first", -0.1f, 0.00225f}, {"second", 0.00225f, 0.0045f}, {"third", 0.0045f, 0.00675f}, {"fourth", 0.00675f, 0.1f}},
        {},
    };
    std::string error;

    ASSERT_TRUE(MapLayerOverrides_Validate(overrides, error)) << error;

    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
    EXPECT_EQ(map.get_analysis().status, RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 4u);

    float total = 0.0f;

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        EXPECT_NEAR(layer.area, 0.000081f, 0.000000001f);

        for (const RadarMapSurface& surface : map.get_surfaces(layer.id))
        {
            total += ProjectedArea(surface);
        }
    }

    EXPECT_NEAR(total, 0.000324f, 0.000000001f);
}

TEST(RadarMapLayers, SmallInteriorOverlapDoesNotCreateTransition)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});
    fixture.AddFloor(64.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    ASSERT_TRUE(map.get_analysis().transitions.empty());

    RadarLayerOverrides overrides{
        {{"lower", -1.0f, 1.0f}, {"upper", 63.0f, 65.0f}},
        {{"lower", {{0.005f, 0.005f, -1.0f}, {0.013f, 0.013f, 1.0f}}}},
    };
    std::string error;

    ASSERT_TRUE(MapLayerOverrides_Validate(overrides, error)) << error;

    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
    ASSERT_EQ(map.get_layers().size(), 2u);

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        EXPECT_NEAR(layer.area, 0.000324f, 0.000000001f);
    }

    EXPECT_TRUE(map.get_analysis().transitions.empty());
}

TEST(RadarMapLayers, SmallInteriorOverlapRecordsManualVerticalConflict)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});
    fixture.AddFloor(64.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    ASSERT_TRUE(map.get_analysis().transitions.empty());

    RadarLayerOverrides overrides{
        {{"merged", -1.0f, 65.0f}},
        {{"merged", {{0.005f, 0.005f, -1.0f}, {0.013f, 0.013f, 1.0f}}}},
    };
    std::string error;

    ASSERT_TRUE(MapLayerOverrides_Validate(overrides, error)) << error;

    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_NEAR(map.get_layers()[0].area, 0.000648f, 0.000000001f);
    EXPECT_TRUE(map.get_analysis().transitions.empty());
    ASSERT_EQ(map.get_analysis().manual_conflicts.size(), 1u);

    const std::string& conflict = map.get_analysis().manual_conflicts[0];
    EXPECT_NE(conflict.find("merged"), std::string::npos);
    EXPECT_NE(conflict.find("0"), std::string::npos);
    EXPECT_NE(conflict.find("1"), std::string::npos);
}

TEST(RadarMapLayers, NearestFindsRemoteSupportAndPreservesSpatialTies)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {64.0f, 64.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {64.0f, 64.0f}});

    for (int i = 1; i < 24; i++)
    {
        float x = (float)i * 128.0f;
        fixture.AddFloor(0.0f, {{x, 0.0f}, {x + 64.0f, 64.0f}});
    }

    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);

    RadarLayerMatch remote = map.Locate({4096.0f, 32.0f, 10.0f}, RadarLayerQuery::Nearest);
    EXPECT_EQ(remote.layer, 0);
    EXPECT_EQ(remote.support, (ncl_math::Vector3{3008.0f, 32.0f, 0.0f}));

    RadarLayerMatch tied = map.Locate({32.0f, 32.0f, 128.0f}, RadarLayerQuery::Nearest, 1);
    EXPECT_EQ(tied.layer, 1);
    EXPECT_EQ(tied.support, (ncl_math::Vector3{32.0f, 32.0f, 256.0f}));
    EXPECT_EQ(map.Locate({32.0f, 32.0f, 128.0f}, RadarLayerQuery::Nearest).layer, 0);

    RadarLayerMatch edge = map.Locate({-1000.0f, -1000.0f, 300.0f}, RadarLayerQuery::Nearest);
    EXPECT_EQ(edge.layer, 1);
    EXPECT_EQ(edge.support, (ncl_math::Vector3{0.0f, 0.0f, 256.0f}));
}

TEST(RadarMapLayers, VolumeQueriesSpanCellsAndExcludeRemoteOrBoundaryOnlySupports)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-64.0f, -64.0f}, {64.0f, 64.0f}});
    fixture.AddFloor(256.0f, {{-64.0f, -64.0f}, {64.0f, 64.0f}});
    fixture.AddFloor(512.0f, {{1024.0f, 1024.0f}, {1088.0f, 1088.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 384.0f}, {"remote", 384.0f, 640.0f}}, {}}, error))
        << error;
    EXPECT_EQ(map.Intersect({{-32.0f, -32.0f, -1.0f}, {32.0f, 32.0f, 300.0f}}), (std::vector<int>{0, 1}));
    EXPECT_EQ(map.Intersect({{-64.0f, -64.0f, 256.0f}, {64.0f, 64.0f, 256.0f}}), (std::vector<int>{1}));
    EXPECT_TRUE(map.Intersect({{64.0f, -64.0f, -1.0f}, {96.0f, 64.0f, 1.0f}}).empty());
    EXPECT_TRUE(map.Intersect({{4096.0f, 4096.0f, -1000.0f}, {8192.0f, 8192.0f, 1000.0f}}).empty());
}

TEST(RadarMapLayers, NearestPreservesToleranceBeyondSmallFootprint)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});
    fixture.AddFloor(64.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});

    for (int i = 1; i < 24; i++)
    {
        float x = -(float)i;
        fixture.AddFloor(0.0f, {{x, 0.0f}, {x + 0.018f, 0.018f}});
        fixture.AddFloor(64.0f, {{x, 0.0f}, {x + 0.018f, 0.018f}});
    }

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -1.0f, 1.0f}, {"upper", 63.0f, 65.0f}}, {}}, error)) << error;

    int upper = map.Locate({0.009f, 0.009f, 64.0f}, RadarLayerQuery::Below).layer;
    ASSERT_GE(upper, 0);

    RadarLayerMatch match = map.Locate({0.06f, 0.009f, 32.0f}, RadarLayerQuery::Nearest, upper);
    EXPECT_EQ(match.layer, upper);
    EXPECT_EQ(match.support, (ncl_math::Vector3{0.06f, 0.009f, 64.0f}));
}

TEST(RadarMapLayers, VolumeIncludesAcceptedSmallPositiveSupports)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});
    fixture.AddFloor(64.0f, {{0.0f, 0.0f}, {0.018f, 0.018f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -1.0f, 1.0f}, {"upper", 63.0f, 65.0f}}, {}}, error)) << error;
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_EQ(map.Intersect({{0.0f, 0.0f, -1.0f}, {0.018f, 0.018f, 65.0f}}), (std::vector<int>{0, 1}));
    EXPECT_TRUE(map.Intersect({{0.018f, 0.0f, -1.0f}, {0.036f, 0.018f, 65.0f}}).empty());
}

TEST(RadarMapLayers, MajorOverlapTerrainRiseRemainsOneLayer)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(512.0f, {{1024.0f, -128.0f}, {1280.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {1024.0f, 0.0f, 512.0f}, 64.0f);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_EQ(map.Locate({1200.0f, 0.0f, 513.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_TRUE(map.get_analysis().transitions.empty());
}

TEST(RadarMapLayers, MajorOverlapSmallBridgeRemainsOneLayer)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(128.0f, {{64.0f, 64.0f}, {128.0f, 256.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_EQ(map.Locate({96.0f, 96.0f, 129.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_FLOAT_EQ(map.get_layers()[0].area, 274432.0f);
}

TEST(RadarMapLayers, MajorOverlapDisjointPatchesDoNotAccumulate)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {2048.0f, 1024.0f}});

    for (int i = 0; i < 4; i++)
    {
        float x = (float)i * 512.0f;
        fixture.AddFloor(128.0f, {{x, 0.0f}, {x + 128.0f, 512.0f}});
    }

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 1u);
}

TEST(RadarMapLayers, MajorOverlapDuplicatesDoNotInflateArea)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

    for (int i = 0; i < 4; i++)
    {
        fixture.AddFloor(128.0f, {{64.0f, 64.0f}, {320.0f, 320.0f}});
    }

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 1u);
}

TEST(RadarMapLayers, MajorOverlapGroupConflictIsNotTransitive)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(64.0f, {{512.0f, 0.0f}, {1024.0f, 512.0f}});
    fixture.AddFloor(128.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_NE(
        map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, map.Locate({64.0f, 64.0f, 129.0f}, RadarLayerQuery::Below).layer
    );
}

TEST(RadarMapLayers, MajorOverlapClearanceIsMeasuredInsideIntersection)
{
    RadarMapLayerFixture fixture;
    fixture.geometry.crouch_bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 36.0f}};
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {20.0f, 16384.0f}});
    fixture.AddFloor(128.0f, {{12.0f, 0.0f}, {32.0f, 16384.0f}});
    fixture.AddSolidBox({{12.0f, -1.0f, 16.0f}, {20.0f, 16385.0f, 64.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(map.Locate({10.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).support.z, 0.0f);
    EXPECT_FLOAT_EQ(map.Locate({22.0f, 64.0f, 129.0f}, RadarLayerQuery::Below).support.z, 128.0f);
}

TEST(RadarMapLayers, MajorOverlapClippedCornerDoesNotInflateUnion)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});

    for (int i = 0; i < 8; i++)
    {
        fixture.AddFloor(128.0f, {{64.0f, 64.0f}, {96.0f, 96.0f}});
        fixture.geometry.surfaces.back().vertices = {{{96.0f, 64.0f, 128.0f}}, {{96.0f, 96.0f, 128.0f}}, {{64.0f, 96.0f, 128.0f}}};
    }

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(map.get_analysis().largest_overlap_area, 512.0f);
}


TEST(RadarMapLayers, DistantStackRampsUseLocalEndpointHeights)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddFloor(2048.0f, {{1792.0f, -128.0f}, {2048.0f, 128.0f}});
    fixture.AddFloor(2304.0f, {{2560.0f, -128.0f}, {2816.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddRamp({2048.0f, 0.0f, 2048.0f}, {2560.0f, 0.0f, 2304.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);
    fixture.AddOverlapRoom(2, 3);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_EQ(map.Locate({300.0f, 0.0f, 151.0f}, RadarLayerQuery::Below).layer, 1);
    EXPECT_EQ(map.Locate({2240.0f, 0.0f, 2145.0f}, RadarLayerQuery::Below).layer, 0);
    ASSERT_EQ(map.get_analysis().transitions.size(), 2u);
    EXPECT_FLOAT_EQ(map.get_analysis().transitions[0].height, 128.0f);
    EXPECT_FLOAT_EQ(map.get_analysis().transitions[1].height, 2176.0f);
}

TEST(RadarMapLayers, RenderRestoresBlockedEdgesOnSeparateFloors)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddSolidBox({{0.0f, -16.0f, 20.0f}, {16.0f, 528.0f, 24.0f}});
    fixture.AddSolidBox({{0.0f, -16.0f, 276.0f}, {16.0f, 528.0f, 280.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    EXPECT_EQ(map.Locate({16.0f, 48.0f, 1.0f}, RadarLayerQuery::Below).layer, -1);
    EXPECT_EQ(map.Locate({16.0f, 48.0f, 257.0f}, RadarLayerQuery::Below).layer, -1);

    for (int layer = 0; layer < 2; layer++)
    {
        float area = 0.0f;

        for (const RadarMapSurface& surface : map.get_surfaces(layer))
        {
            EXPECT_EQ(surface.surface_id, layer);

            area += ProjectedArea(surface);

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                EXPECT_FLOAT_EQ(vertex.pos.z, (float)layer * 256.0f);
                EXPECT_NEAR(vertex.uv.x, vertex.pos.x / 512.0f, 0.000001f);
                EXPECT_NEAR(vertex.uv.y, vertex.pos.y / 512.0f, 0.000001f);
            }
        }

        EXPECT_FLOAT_EQ(area, 262144.0f);
        EXPECT_FLOAT_EQ(map.get_layers()[layer].area, 245760.0f);
    }
}

TEST(RadarMapLayers, RenderKeepsUnsupportedDetailsOutsideFloorFootprints)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

    for (int layer = 0; layer < 2; layer++)
    {
        RadarMapSurface detail{};
        detail.surface_id = 2 + layer;
        detail.normal = {-0.8f, 0.0f, 0.6f};
        float z = (float)layer * 256.0f;
        detail.vertices = {
            {{-16.0f, 32.0f, z + 1.0f}}, {{-8.0f, 32.0f, z + 11.666667f}}, {{-8.0f, 64.0f, z + 11.666667f}}, {{-16.0f, 64.0f, z + 1.0f}}
        };
        fixture.geometry.surfaces.push_back(std::move(detail));
    }

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);

    for (int layer = 0; layer < 2; layer++)
    {
        float area = 0.0f;

        for (const RadarMapSurface& surface : map.get_surfaces(layer))
        {
            EXPECT_NE(surface.surface_id, 3 - layer);

            if (surface.surface_id == 2 + layer)
            {
                area += ProjectedArea(surface);
            }
        }

        EXPECT_FLOAT_EQ(area, 256.0f);
    }

    EXPECT_EQ(map.Locate({-12.0f, 48.0f, 1.0f}, RadarLayerQuery::Below).layer, -1);
}

TEST(RadarMapLayers, RestoredRenderFootprintsKeepManualRegionBoundary)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 64.0f}});
    fixture.AddSolidBox({{0.0f, -16.0f, 20.0f}, {8.0f, 80.0f, 24.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"left", 128.0f, 256.0f}}, {{"left", {{0.0f, 0.0f, -1.0f}, {64.0f, 64.0f, 1.0f}}}}}, error));
    ASSERT_EQ(map.get_layers().size(), 2u);

    int left = LayerNamed(map, "left");
    ASSERT_GE(left, 0);

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        float area = 0.0f;

        for (const RadarMapSurface& surface : map.get_surfaces(layer.id))
        {
            area += ProjectedArea(surface);

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                if (layer.id == left)
                {
                    EXPECT_LE(vertex.pos.x, 64.0f);
                }
                else
                {
                    EXPECT_GE(vertex.pos.x, 64.0f);
                }
            }
        }

        EXPECT_FLOAT_EQ(area, 4096.0f);
    }
}

TEST(RadarMapLayers, RenderExcludesSkyCappedRoofWithoutFillingEmptySpace)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(128.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(160.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.surfaces.back().sky = true;
    fixture.AddFloor(0.0f, {{256.0f, 0.0f}, {384.0f, 128.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);

    float area = 0.0f;

    for (const RadarMapSurface& surface : map.get_surfaces(0))
    {
        EXPECT_NE(surface.surface_id, 1);

        area += ProjectedArea(surface);

        for (const RadarMapVertex& vertex : surface.vertices)
        {
            EXPECT_TRUE(vertex.pos.x <= 128.0f || vertex.pos.x >= 256.0f);
        }
    }

    EXPECT_FLOAT_EQ(area, 32768.0f);
}

TEST(RadarMapLayers, RestoredSourceUsesManualLayerSupportedByAnotherFace)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 64.0f}});
    fixture.AddFloor(0.0f, {{256.0f, 0.0f}, {320.0f, 64.0f}});
    fixture.AddSolidBox({{0.0f, -16.0f, 20.0f}, {8.0f, 80.0f, 24.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides(
        {{{"manual", 128.0f, 256.0f}},
         {{"manual", {{0.0f, 0.0f, -1.0f}, {24.0f, 64.0f, 1.0f}}}, {"manual", {{256.0f, 0.0f, -1.0f}, {320.0f, 64.0f, 1.0f}}}}},
        error
    ));
    int manual = LayerNamed(map, "manual");

    ASSERT_GE(manual, 0);

    float area = 0.0f;

    for (const RadarMapSurface& surface : map.get_surfaces(manual))
    {
        if (surface.surface_id == 0)
        {
            area += ProjectedArea(surface);
        }
    }

    EXPECT_FLOAT_EQ(area, 1536.0f);
}

TEST(RadarMapLayers, UnsupportedDetailKeepsExactManualRegionBoundary)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(0.0f, {{-64.0f, 0.0f}, {-32.0f, 64.0f}});
    RadarMapSurface detail{};
    detail.surface_id = 3;
    detail.normal = {-0.8f, 0.0f, 0.6f};
    detail.vertices = {{{-16.0f, 32.0f, 1.0f}}, {{-8.0f, 32.0f, 11.666667f}}, {{-8.0f, 64.0f, 11.666667f}}, {{-16.0f, 64.0f, 1.0f}}};
    fixture.geometry.surfaces.push_back(std::move(detail));

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"manual", 512.0f, 768.0f}}, {{"manual", {{-64.0f, 0.0f, -1.0f}, {-12.0f, 64.0f, 1.0f}}}}}, error));

    int manual = LayerNamed(map, "manual");
    ASSERT_GE(manual, 0);

    float manual_area = 0.0f;
    float automatic_area = 0.0f;

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        for (const RadarMapSurface& surface : map.get_surfaces(layer.id))
        {
            if (surface.surface_id != 3)
            {
                continue;
            }

            if (layer.id == manual)
            {
                manual_area += ProjectedArea(surface);
            }
            else
            {
                automatic_area += ProjectedArea(surface);
            }

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                if (layer.id == manual)
                {
                    EXPECT_LE(vertex.pos.x, -12.0f);
                }
                else
                {
                    EXPECT_GE(vertex.pos.x, -12.0f);
                }
            }
        }
    }

    EXPECT_FLOAT_EQ(manual_area, 128.0f);
    EXPECT_FLOAT_EQ(automatic_area, 128.0f);
}

TEST(RadarMapLayers, RenderExcludesUnsupportedTiledDetailsOnMultipleFloors)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    RadarMapSurface detail{};
    detail.surface_id = 2;
    detail.tiled = true;
    detail.normal = {-0.8f, 0.0f, 0.6f};
    detail.vertices = {{{-16.0f, 32.0f, 1.0f}}, {{-8.0f, 32.0f, 11.666667f}}, {{-8.0f, 64.0f, 11.666667f}}, {{-16.0f, 64.0f, 1.0f}}};
    fixture.geometry.surfaces.push_back(std::move(detail));

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        for (const RadarMapSurface& surface : map.get_surfaces(layer.id))
        {
            EXPECT_NE(surface.surface_id, 2);
        }
    }
}

TEST(RadarMapLayers, RenderRestoresExactTransitionInsideBlockedRamp)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddRamp({0.0f, 64.0f, 0.0f}, {512.0f, 64.0f, 256.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);
    fixture.AddSolidBox({{192.0f, -48.0f, -32.0f}, {448.0f, 48.0f, 300.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    ASSERT_FALSE(map.get_analysis().transitions.empty());
    EXPECT_FLOAT_EQ(map.get_analysis().transitions[0].height, 128.0f);

    for (int layer = 0; layer < 2; layer++)
    {
        float area = 0.0f;

        for (const RadarMapSurface& surface : map.get_surfaces(layer))
        {
            if (surface.surface_id != 2)
            {
                continue;
            }

            area += ProjectedArea(surface);

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                if (layer == 0)
                {
                    EXPECT_LE(vertex.pos.z, 128.0f);
                }
                else
                {
                    EXPECT_GE(vertex.pos.z, 128.0f);
                }
            }
        }

        EXPECT_FLOAT_EQ(area, 16384.0f);
    }
}

TEST(RadarMapLayers, StairTreadsWithBlockedCellCentersRemainConnected)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-512.0f, 0.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, 0.0f}, {1024.0f, 128.0f}});
    fixture.AddStairs(0.0f, 256.0f, 16, {{0.0f, 0.0f}, {512.0f, 128.0f}});
    fixture.AddOverlapRoom(0, 1);

    for (int step = 1; step < 16; step++)
    {
        fixture.AddSolidBox({{step * 32.0f, 0.0f, -64.0f}, {(step + 1) * 32.0f, 128.0f, (step + 1) * 16.0f}});
    }

    EXPECT_FALSE(MapGeometry_CheckClearance(fixture.geometry, {16.0f, 64.0f, 16.001f}));
    EXPECT_TRUE(MapGeometry_CheckClearance(fixture.geometry, {8.0f, 64.0f, 16.001f}));

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    ASSERT_EQ(map.get_analysis().transitions.size(), 1u);
    EXPECT_FLOAT_EQ(map.get_analysis().transitions[0].height, 128.0f);

    for (int step = 0; step < 16; step++)
    {
        RadarLayerMatch match = map.Locate({step * 32.0f + 8.0f, 64.0f, (step + 1) * 16.0f + 0.01f}, RadarLayerQuery::Below);
        EXPECT_FLOAT_EQ(match.support.z, (step + 1) * 16.0f);
        EXPECT_EQ(match.layer, step < 8 ? 0 : 1);

        float area = 0.0f;

        for (int layer = 0; layer < 2; layer++)
        {
            for (const RadarMapSurface& surface : map.get_surfaces(layer))
            {
                if (surface.surface_id == step + 2)
                {
                    area += ProjectedArea(surface);
                }
            }
        }

        EXPECT_FLOAT_EQ(area, 4096.0f);
    }
}

TEST(RadarMapLayers, EntirelyBlockedCellHasNoSupport)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(16.0f, {{0.0f, 0.0f}, {32.0f, 32.0f}});
    fixture.AddSolidBox({{0.0f, 0.0f, 16.0f}, {32.0f, 32.0f, 64.0f}});

    RadarMapLayers map;
    EXPECT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Failed);
}

namespace
{
    class AnalysisCancellationToken : public taskcoro::CancellationToken
    {
    public:
        explicit AnalysisCancellationToken(int limit) :
            limit_(limit)
        {}
        bool IsCanceled() const override
        {
            return ++checks_ >= limit_;
        }
        int get_check_count() const
        {
            return checks_;
        }

    private:
        int limit_;
        mutable int checks_{};
    };
} // namespace

TEST(RadarMapLayers, CancellationDiscardsPartialAnalysis)
{
    for (int limit : {1, 32, 256})
    {
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        AnalysisCancellationToken token(limit);

        RadarMapLayers map;
        EXPECT_EQ(map.Build(std::move(fixture.geometry), &token), RadarMapStatus::Waiting);
        EXPECT_EQ(map.get_analysis().status, RadarMapStatus::Waiting);
        EXPECT_TRUE(map.get_layers().empty());
        EXPECT_TRUE(map.get_analysis().transitions.empty());
        EXPECT_TRUE(map.get_surfaces(0).empty());
        EXPECT_EQ(map.Locate({16.0f, 16.0f, 1.0f}, RadarLayerQuery::Below).layer, -1);
    }
}

TEST(RadarMapLayers, CanceledOverridesPreserveCompletedAnalysis)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    AnalysisCancellationToken token(32);
    std::string error;

    EXPECT_FALSE(map.ApplyOverrides({}, error, &token));
    EXPECT_NE(error.find("canceled"), std::string::npos);
    EXPECT_EQ(map.get_analysis().status, RadarMapStatus::Ready);
    EXPECT_EQ(map.get_layers().size(), 1u);
    EXPECT_EQ(map.Locate({16.0f, 16.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarMapLayers, HideSubtractsOverlappingVolumesWithoutChangingSupports)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerOverrides overrides;
    std::string error;
    const std::vector<std::string> tokens{
        "radar_layers",
        "{",
        "hide",
        "{",
        "mins",
        "32 32 -1",
        "maxs",
        "80 96 1",
        "}",
        "hide",
        "{",
        "mins",
        "64 32 -1",
        "maxs",
        "96 96 1",
        "}",
        "}",
    };

    ASSERT_TRUE(MapLayerOverrides_Parse(tokens, overrides, error)) << error;
    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;

    float area = 0.0f;

    for (const RadarMapSurface& surface : map.get_surfaces(0))
    {
        area += ProjectedArea(surface);

        EXPECT_EQ(surface.surface_id, 0);

        for (const RadarMapVertex& vertex : surface.vertices)
        {
            EXPECT_FLOAT_EQ(vertex.uv.x, vertex.pos.x / 128.0f);
            EXPECT_FLOAT_EQ(vertex.uv.y, vertex.pos.y / 128.0f);
            EXPECT_EQ(vertex.uv, vertex.lightmap_uv);
        }
    }

    EXPECT_FLOAT_EQ(area, 12288.0f);
    ASSERT_EQ(map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(map.get_layers()[0].area, 16384.0f);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Nearest).layer, 0);

    ASSERT_TRUE(map.ApplyOverrides({}, error)) << error;

    area = 0.0f;

    for (const RadarMapSurface& surface : map.get_surfaces(0))
    {
        area += ProjectedArea(surface);
    }

    EXPECT_FLOAT_EQ(area, 16384.0f);
}

TEST(RadarMapLayers, HideAppliesAcrossAutomaticAndExplicitLayers)
{
    for (bool explicit_layers : {false, true})
    {
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

        RadarMapLayers map;
        ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

        RadarLayerOverrides overrides;
        std::string error;
        std::vector<std::string> tokens{"radar_layers", "{"};

        if (explicit_layers)
        {
            const std::vector<std::string> layers{
                "layer",
                "{",
                "name",
                "lower",
                "min_z",
                "-512",
                "max_z",
                "128",
                "}",
                "layer",
                "{",
                "name",
                "upper",
                "min_z",
                "128",
                "max_z",
                "512",
                "}",
            };
            tokens.insert(tokens.end(), layers.begin(), layers.end());
        }

        const std::vector<std::string> hide{"hide", "{", "mins", "128 128 -1", "maxs", "384 384 257", "}", "}"};
        tokens.insert(tokens.end(), hide.begin(), hide.end());
        ASSERT_TRUE(MapLayerOverrides_Parse(tokens, overrides, error)) << error;
        ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
        ASSERT_EQ(map.get_layers().size(), 2u);

        for (int layer = 0; layer < 2; layer++)
        {
            float area = 0.0f;

            for (const RadarMapSurface& surface : map.get_surfaces(layer))
            {
                area += ProjectedArea(surface);
            }

            EXPECT_FLOAT_EQ(area, 196608.0f);
            EXPECT_FLOAT_EQ(map.get_layers()[layer].area, 262144.0f);
            EXPECT_EQ(map.Locate({256.0f, 256.0f, layer * 256.0f + 1.0f}, RadarLayerQuery::Below).layer, layer);
        }
    }
}

TEST(RadarMapLayers, HideSplitsVerticalDetailAndInterpolatesTextureCoordinates)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    int wall = AddWall(fixture, 0.0f, 128.0f, 32.0f, 0.0f, 128.0f);

    for (RadarMapVertex& vertex : fixture.geometry.surfaces[wall].vertices)
    {
        vertex.lightmap_uv = {vertex.uv.x * 2.0f + 3.0f, vertex.uv.y * 4.0f + 5.0f};
    }

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerOverrides overrides;
    std::string error;
    const std::vector<std::string> tokens{
        "radar_layers",
        "{",
        "hide",
        "{",
        "mins",
        "32 0 32",
        "maxs",
        "96 64 96",
        "}",
        "}",
    };

    ASSERT_TRUE(MapLayerOverrides_Parse(tokens, overrides, error)) << error;
    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
    EXPECT_FLOAT_EQ(DetailArea(map, 0, wall), 12288.0f);

    for (const RadarMapSurface& surface : map.get_surfaces(0))
    {
        if (surface.surface_id != wall)
        {
            continue;
        }

        for (const RadarMapVertex& vertex : surface.vertices)
        {
            EXPECT_FLOAT_EQ(vertex.uv.x, vertex.pos.x / 128.0f);
            EXPECT_FLOAT_EQ(vertex.uv.y, vertex.pos.z / 128.0f);
            EXPECT_FLOAT_EQ(vertex.lightmap_uv.x, vertex.uv.x * 2.0f + 3.0f);
            EXPECT_FLOAT_EQ(vertex.lightmap_uv.y, vertex.uv.y * 4.0f + 5.0f);
        }
    }

    EXPECT_FLOAT_EQ(map.get_layers()[0].area, 16384.0f);
}

TEST(RadarMapLayers, HideUsesHalfOpenBoundsOnCoplanarFaces)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddFloor(128.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    int min_y = AddWall(fixture, 0.0f, 128.0f, 0.0f, 0.0f, 128.0f);
    int max_y = AddWall(fixture, 0.0f, 128.0f, 128.0f, 0.0f, 128.0f);
    int min_x = AddWall(fixture, 0.0f, 128.0f, 64.0f, 0.0f, 128.0f);
    int max_x = AddWall(fixture, 0.0f, 128.0f, 64.0f, 0.0f, 128.0f);

    for (int source : {min_x, max_x})
    {
        RadarMapSurface& wall = fixture.geometry.surfaces[source];
        wall.normal = {1.0f, 0.0f, 0.0f};

        for (RadarMapVertex& vertex : wall.vertices)
        {
            vertex.pos.y = vertex.pos.x;
            vertex.pos.x = source == min_x ? 0.0f : 128.0f;
        }
    }

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerOverrides overrides;
    std::string error;
    const std::vector<std::string> tokens{
        "radar_layers", "{", "layer", "{",     "name", "whole",       "min_z", "-1", "max_z", "256", "}",
        "hide",         "{", "mins",  "0 0 0", "maxs", "128 128 128", "}",     "}",
    };

    ASSERT_TRUE(MapLayerOverrides_Parse(tokens, overrides, error)) << error;
    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
    EXPECT_FLOAT_EQ(DetailArea(map, 0, 0), 0.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, 1), 16384.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, min_y), 0.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, max_y), 16384.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, min_x), 0.0f);
    EXPECT_FLOAT_EQ(DetailArea(map, 0, max_x), 16384.0f);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 129.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_FALSE(map.get_analysis().manual_conflicts.empty());
}

TEST(RadarMapLayers, HideClipsSlopedFacesAtZBounds)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {128.0f, 0.0f, 64.0f}, 128.0f);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerOverrides overrides;
    std::string error;
    const std::vector<std::string> tokens{
        "radar_layers",
        "{",
        "hide",
        "{",
        "mins",
        "-1 -65 16",
        "maxs",
        "129 65 48",
        "}",
        "}",
    };

    ASSERT_TRUE(MapLayerOverrides_Parse(tokens, overrides, error)) << error;
    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;

    float area = 0.0f;

    for (const RadarMapSurface& surface : map.get_surfaces(0))
    {
        area += ProjectedArea(surface);

        for (const RadarMapVertex& vertex : surface.vertices)
        {
            EXPECT_TRUE(vertex.pos.z <= 16.0f || vertex.pos.z >= 48.0f);
            EXPECT_FLOAT_EQ(vertex.uv.x, vertex.pos.x / 128.0f);
            EXPECT_FLOAT_EQ(vertex.uv.y, (vertex.pos.y + 64.0f) / 128.0f);
        }
    }

    EXPECT_FLOAT_EQ(area, 8192.0f);
    EXPECT_FLOAT_EQ(map.Locate({64.0f, 0.0f, 33.0f}, RadarLayerQuery::Below).support.z, 32.0f);
}

TEST(RadarMapLayers, CorrectionsKeepDistantTransitionsLocal)
{
    for (bool manual : {false, true})
    {
        SCOPED_TRACE(manual);

        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
        fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
        fixture.AddFloor(2048.0f, {{1792.0f, -128.0f}, {2048.0f, 128.0f}});
        fixture.AddFloor(2304.0f, {{2560.0f, -128.0f}, {2816.0f, 128.0f}});
        fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
        fixture.AddRamp({2048.0f, 0.0f, 2048.0f}, {2560.0f, 0.0f, 2304.0f}, 64.0f);
        fixture.AddOverlapRoom(0, 1);
        fixture.AddOverlapRoom(2, 3);

        RadarLayerOverrides overrides;

        if (manual)
        {
            overrides.layers = {{"lower", -64.0f, 128.0f}, {"upper", 128.0f, 512.0f}};
            overrides.regions = {
                {"lower", {{1792.0f, -128.0f, 2048.0f}, {2816.0f, 384.0f, 2176.0f}}},
                {"upper", {{1792.0f, -128.0f, 2176.0f}, {2816.0f, 384.0f, 2400.0f}}},
            };
        }
        else
        {
            fixture.AddFloor(4096.0f, {{5000.0f, 0.0f}, {5064.0f, 64.0f}});
            overrides.layers = {{"remote", 4000.0f, 5000.0f}};
        }

        RadarMapLayers map;
        std::string error;
        ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
        ASSERT_EQ(map.get_analysis().transitions.size(), 2u);
        EXPECT_FLOAT_EQ(map.get_analysis().transitions[0].height, 128.0f);
        EXPECT_FLOAT_EQ(map.get_analysis().transitions[1].height, 2176.0f);
        EXPECT_EQ(map.get_analysis().transitions[0].surface_ids, (std::vector<int>{4}));
        EXPECT_EQ(map.get_analysis().transitions[1].surface_ids, (std::vector<int>{5}));

        for (int ramp = 0; ramp < 2; ramp++)
        {
            for (bool rising : {false, true})
            {
                SCOPED_TRACE(ramp);
                SCOPED_TRACE(rising);

                auto observation = [&](float x, float time) {
                    RadarLayerObservation result{};
                    result.feet = {(float)ramp * 2048.0f + x, 0.0f, (float)ramp * 2048.0f + x * 0.5f};
                    result.support = result.feet;
                    result.time = time;
                    result.valid = true;
                    result.grounded = true;

                    return result;
                };
                RadarLayerSelection selection;
                RadarLayerObservation initial = observation(rising ? 200.0f : 312.0f, 0.0f);
                RadarLayerObservation target = observation(rising ? 386.0f : 126.0f, 1.0f);
                int initial_layer = map.Locate(initial.feet, RadarLayerQuery::Below).layer;
                int target_layer = map.Locate(target.feet, RadarLayerQuery::Below).layer;
                ASSERT_NE(initial_layer, target_layer);
                ASSERT_EQ(selection.Update(map, initial), initial_layer);
                EXPECT_EQ(selection.Update(map, target), initial_layer);
                EXPECT_EQ(selection.get_candidate(), target_layer);

                target.time = 1.151f;
                EXPECT_EQ(selection.Update(map, target), target_layer);
            }
        }
    }
}

TEST(RadarMapLayers, RenderKeepsFloorOutsideTriangularSkyFootprint)
{
    for (bool manual : {false, true})
    {
        SCOPED_TRACE(manual);

        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{384.0f, 384.0f}, {448.0f, 448.0f}});
        RadarMapSurface sky{};
        sky.surface_id = 1;
        sky.sky = true;
        sky.normal = {0.0f, 0.0f, -1.0f};
        sky.vertices = {{{0.0f, 0.0f, 32.0f}}, {{0.0f, 512.0f, 32.0f}}, {{512.0f, 0.0f, 32.0f}}};
        fixture.geometry.surfaces.push_back(std::move(sky));
        fixture.geometry.bounds.Include({0.0f, 0.0f});
        fixture.geometry.bounds.Include({512.0f, 512.0f});

        RadarMapLayers map;
        ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

        if (manual)
        {
            std::string error;
            ASSERT_TRUE(map.ApplyOverrides({{{"main", -1.0f, 1.0f}}, {}}, error)) << error;
        }

        ASSERT_EQ(map.get_layers().size(), 1u);
        EXPECT_EQ(map.Locate({416.0f, 416.0f, 0.0f}, RadarLayerQuery::Below).layer, 0);
        EXPECT_FLOAT_EQ(DetailArea(map, 0, 0), 4096.0f);
    }
}

TEST(RadarMapLayers, DisplayBoundsExcludeFilteredAndZeroProjectedAreaFaces)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddRamp({128.0f, 64.0f, 0.0f}, {256.0f, 64.0f, 256.0f}, 128.0f);
    fixture.AddFloor(512.0f, {{-1024.0f, -1024.0f}, {1024.0f, 1024.0f}});
    fixture.geometry.surfaces.back().sky = true;
    int wall = AddWall(fixture, 512.0f, 768.0f, 512.0f, 0.0f, 128.0f);
    RadarMapSurface clipped_wall{};
    clipped_wall.surface_id = (int)fixture.geometry.surfaces.size();
    clipped_wall.normal = ncl_math::Vector3{-288.2f, 268.2f, 0.0f}.Normalized();
    clipped_wall.vertices = {{{500.1f, 512.2f, 0.0f}}, {{768.3f, 800.4f, 0.0f}}, {{634.2f, 656.3f, 64.0f}}};
    ASSERT_NE(
        (clipped_wall.vertices[1].pos - clipped_wall.vertices[0].pos).Cross(clipped_wall.vertices[2].pos - clipped_wall.vertices[0].pos).z,
        0.0f
    );
    fixture.geometry.surfaces.push_back(std::move(clipped_wall));
    fixture.geometry.bounds = {{-2048.0f, -2048.0f}, {2048.0f, 2048.0f}};

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 1u);
    ASSERT_TRUE(std::any_of(map.get_surfaces(0).begin(), map.get_surfaces(0).end(), [wall](const RadarMapSurface& surface) {
        return surface.surface_id == wall;
    }));
    EXPECT_EQ(map.get_bounds().min, (ncl_math::Vector2{0.0f, 0.0f}));
    EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{256.0f, 128.0f}));
    EXPECT_EQ(map.get_analysis().layers[0].bounds.max, (ncl_math::Vector2{128.0f, 128.0f}));
}

TEST(RadarMapLayers, DisplayBoundsUseAllLayersAfterHides)
{
    for (bool explicit_layers : {false, true})
    {
        SCOPED_TRACE(explicit_layers);
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.AddFloor(256.0f, {{128.0f, -128.0f}, {640.0f, 384.0f}});
        fixture.geometry.bounds = {{-2048.0f, -2048.0f}, {2048.0f, 2048.0f}};

        RadarMapLayers map;
        ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        ASSERT_EQ(map.get_layers().size(), 2u);
        RadarLayerOverrides overrides;
        if (explicit_layers)
        {
            overrides.layers = {{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}};
        }
        overrides.hides = {{{-1.0f, -256.0f, -1.0f}, {128.0f, 1024.0f, 1.0f}}, {{512.0f, -256.0f, 255.0f}, {1024.0f, 1024.0f, 257.0f}}};
        std::string error;
        ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
        EXPECT_EQ(map.get_bounds().min, (ncl_math::Vector2{128.0f, -128.0f}));
        EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{512.0f, 512.0f}));
        EXPECT_EQ(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
        EXPECT_EQ(map.Intersect({{0.0f, 0.0f, -1.0f}, {128.0f, 128.0f, 1.0f}}), (std::vector<int>{0}));

        ASSERT_TRUE(map.ApplyOverrides({}, error)) << error;
        EXPECT_EQ(map.get_bounds().min, (ncl_math::Vector2{0.0f, -128.0f}));
        EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{640.0f, 512.0f}));
    }
}

TEST(RadarMapLayers, DisplayBoundsRestoreAfterHideOnlyOverrides)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.bounds = {{-1024.0f, -1024.0f}, {1024.0f, 1024.0f}};
    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerOverrides overrides;
    overrides.hides = {{{64.0f, -1.0f, -1.0f}, {256.0f, 256.0f, 1.0f}}};
    std::string error;
    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
    EXPECT_EQ(map.get_bounds().min, (ncl_math::Vector2{0.0f, 0.0f}));
    EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{64.0f, 128.0f}));
    EXPECT_EQ(map.Locate({96.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
    EXPECT_EQ(map.Intersect({{64.0f, 0.0f, -1.0f}, {128.0f, 128.0f, 1.0f}}), (std::vector<int>{0}));

    ASSERT_TRUE(map.ApplyOverrides({}, error)) << error;
    EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{128.0f, 128.0f}));

    map.Reset();
    EXPECT_EQ(map.get_bounds().min, ncl_math::Vector2{});
    EXPECT_EQ(map.get_bounds().max, ncl_math::Vector2{});
    fixture = {};
    fixture.AddFloor(0.0f, {{256.0f, 256.0f}, {384.0f, 384.0f}});
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.get_bounds().min, (ncl_math::Vector2{256.0f, 256.0f}));
    EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{384.0f, 384.0f}));
}

TEST(RadarMapLayers, DisplayBoundsFallBackToWorldWhenAllGeometryIsHidden)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.geometry.bounds = {{-1024.0f, -1024.0f}, {1024.0f, 1024.0f}};
    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    RadarLayerOverrides overrides;
    overrides.hides = {{{-1.0f, -1.0f, -1.0f}, {256.0f, 256.0f, 1.0f}}};
    std::string error;
    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;
    EXPECT_EQ(map.get_analysis().status, RadarMapStatus::Ready);
    EXPECT_TRUE(map.get_surfaces(0).empty());
    EXPECT_EQ(map.get_bounds().min, (ncl_math::Vector2{-1024.0f, -1024.0f}));
    EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{1024.0f, 1024.0f}));
    EXPECT_EQ(map.Locate({64.0f, 64.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);

    ASSERT_TRUE(map.ApplyOverrides({}, error)) << error;
    EXPECT_EQ(map.get_bounds().min, (ncl_math::Vector2{0.0f, 0.0f}));
    EXPECT_EQ(map.get_bounds().max, (ncl_math::Vector2{128.0f, 128.0f}));
}

TEST(RadarMapLayers, UnsupportedDetailUsesLowerSupportWhenNearestPlaneIsAboveIt)
{
    for (bool manual : {false, true})
    {
        SCOPED_TRACE(manual);
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.AddRamp({512.0f, 64.0f, 0.0f}, {576.0f, 64.0f, 64.0f}, 64.0f);
        fixture.AddRamp({584.0f, 48.0f, 1.0f}, {592.0f, 48.0f, 12.666667f}, 32.0f);
        ASSERT_LT(fixture.geometry.surfaces[3].normal.z, kMapSupportNormalZ);

        RadarMapLayers map;
        ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        ASSERT_EQ(map.get_layers().size(), 2u);
        if (manual)
        {
            std::string error;
            ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 384.0f}}, {}}, error)) << error;
        }

        ncl_math::Vector3 detail_center{588.0f, 48.0f, 6.833333f};
        EXPECT_EQ(map.Locate(detail_center, RadarLayerQuery::Nearest, -1, 0).surface_id, 2);
        EXPECT_EQ(map.Locate(detail_center, RadarLayerQuery::Below).layer, -1);
        float lower_area = 0.0f;
        float upper_area = 0.0f;
        for (const RadarLayerInfo& layer : map.get_layers())
        {
            for (const RadarMapSurface& surface : map.get_surfaces(layer.id))
            {
                if (surface.surface_id == 3)
                {
                    if (layer.id == 0)
                    {
                        lower_area += ProjectedArea(surface);
                    }
                    else
                    {
                        upper_area += ProjectedArea(surface);
                    }
                }
            }
        }
        EXPECT_FLOAT_EQ(lower_area, 256.0f);
        EXPECT_FLOAT_EQ(upper_area, 0.0f);
    }
}

TEST(RadarMapLayers, CombinedBuildDiscardsCancellationAcrossAnalysisAndRebuild)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    RadarLayerOverrides overrides{{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}};
    AnalysisCancellationToken counter(std::numeric_limits<int>::max());
    RadarMapLayers complete;
    std::string warning;
    ASSERT_EQ(complete.Build(fixture.geometry, overrides, warning, &counter), RadarMapStatus::Ready);
    for (int limit : {1, counter.get_check_count() / 2, counter.get_check_count() - 1})
    {
        AnalysisCancellationToken token(limit);
        RadarMapLayers map;
        EXPECT_EQ(map.Build(fixture.geometry, overrides, warning, &token), RadarMapStatus::Waiting);
        EXPECT_TRUE(map.get_layers().empty());
        EXPECT_TRUE(map.get_surfaces(0).empty());
        EXPECT_TRUE(map.Intersect({{0.0f, 0.0f, -1.0f}, {256.0f, 256.0f, 257.0f}}).empty());
    }
}
