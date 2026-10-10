#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include <goldsrc/format/bsp.h>

#include "hud/radar/map/MapGeometry.h"
#include "RadarMapLayerFixtures.h"

TEST(MapGeometry, ClipInterpolatesBothUvs)
{
    std::array<RadarMapVertex, 4> polygon{{
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
        {{64.0f, 0.0f, 64.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}},
        {{64.0f, 64.0f, 64.0f}, {1.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 64.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}},
    }};
    std::vector<RadarMapVertex> clipped = MapGeometry_Clip(polygon, {{0.0f, 0.0f, -1.0f}, {32.0f, 64.0f, 65.0f}});
    auto boundary = std::find_if(clipped.begin(), clipped.end(), [](const RadarMapVertex& vertex) { return vertex.pos.x == 32.0f; });

    ASSERT_NE(boundary, clipped.end());

    const RadarMapVertex& boundary_vertex = *boundary;
    EXPECT_FLOAT_EQ(boundary_vertex.pos.x, 32.0f);
    EXPECT_FLOAT_EQ(boundary_vertex.pos.z, 32.0f);
    EXPECT_FLOAT_EQ(boundary_vertex.uv.x, 0.5f);
    EXPECT_FLOAT_EQ(boundary_vertex.lightmap_uv.x, 0.5f);
}

TEST(MapGeometry, NarrowPolygonSurvivesCellClipping)
{
    std::array<RadarMapVertex, 4> polygon{{
        {{4.0f, 0.0f, 0.0f}},
        {{8.0f, 0.0f, 0.0f}},
        {{8.0f, 32.0f, 0.0f}},
        {{4.0f, 32.0f, 0.0f}},
    }};
    std::vector<RadarMapVertex> clipped = MapGeometry_Clip(polygon, {{0.0f, 0.0f, -1.0f}, {32.0f, 32.0f, 1.0f}});

    ASSERT_EQ(clipped.size(), 4u);

    float twice_area = 0.0f;

    for (size_t i = 0; i < clipped.size(); i++)
    {
        const ncl_math::Vector3& a = clipped[i].pos;
        const ncl_math::Vector3& b = clipped[(i + 1) % clipped.size()].pos;
        twice_area += a.x * b.y - b.x * a.y;
    }

    EXPECT_FLOAT_EQ(twice_area, 256.0f);
}

TEST(MapGeometry, ClipsAgainstAllSixBounds)
{
    std::array<RadarMapVertex, 4> polygon{{
        {{-16.0f, -16.0f, -16.0f}},
        {{48.0f, -16.0f, 48.0f}},
        {{48.0f, 48.0f, 48.0f}},
        {{-16.0f, 48.0f, -16.0f}},
    }};
    std::vector<RadarMapVertex> clipped = MapGeometry_Clip(polygon, {{0.0f, 0.0f, 8.0f}, {32.0f, 32.0f, 24.0f}});

    ASSERT_EQ(clipped.size(), 4u);
    EXPECT_TRUE(std::any_of(clipped.begin(), clipped.end(), [](const RadarMapVertex& v) {
        return v.pos == ncl_math::Vector3{8.0f, 0.0f, 8.0f};
    }));

    EXPECT_TRUE(std::any_of(clipped.begin(), clipped.end(), [](const RadarMapVertex& v) {
        return v.pos == ncl_math::Vector3{24.0f, 32.0f, 24.0f};
    }));
}

TEST(MapGeometry, RejectsPolygonOutsideBounds)
{
    std::array<RadarMapVertex, 3> polygon{{{{40.0f, 0.0f, 0.0f}}, {{48.0f, 0.0f, 0.0f}}, {{40.0f, 8.0f, 0.0f}}}};
    EXPECT_TRUE(MapGeometry_Clip(polygon, {{0.0f, 0.0f, -1.0f}, {32.0f, 32.0f, 1.0f}}).empty());
}

TEST(MapGeometry, ChecksClearanceAboveSlopedFloor)
{
    RadarMapGeometry geometry{};
    geometry.crouch_bounds = {{-16.0f, -16.0f, -18.0f}, {16.0f, 16.0f, 18.0f}};
    geometry.collision_hulls.push_back({{{{0.6f, 0.0f, 0.8f}, 24.0f}}, {{0, {goldsrc::CONTENTS_EMPTY, goldsrc::CONTENTS_SOLID}}}, 0, {}});

    EXPECT_TRUE(MapGeometry_CheckClearance(geometry, {64.0f, 0.0f, -36.0f}));
    EXPECT_FALSE(MapGeometry_CheckClearance(geometry, {64.0f, 0.0f, -52.0f}));
}

TEST(MapGeometry, RejectsSkyContents)
{
    RadarMapGeometry geometry{};
    geometry.collision_hulls.push_back({{}, {}, goldsrc::CONTENTS_SKY, {}});

    EXPECT_FALSE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 0.0f}));
}

TEST(MapGeometry, PreservesClearNonSolidContents)
{
    RadarMapGeometry geometry{};
    geometry.collision_hulls.push_back({{}, {}, goldsrc::CONTENTS_WATER, {}});

    EXPECT_TRUE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 0.0f}));
}

TEST(MapGeometry, FixtureFloorAndBoxBlockTheCrouchVolume)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
    fixture.AddSolidBox({{0.0f, 0.0f, 24.0f}, {32.0f, 32.0f, 32.0f}});

    ASSERT_EQ(fixture.geometry.surfaces.size(), 1u);
    EXPECT_EQ(fixture.geometry.surfaces[0].vertices[2].pos, (ncl_math::Vector3{128.0f, 128.0f, 0.0f}));
    EXPECT_FALSE(MapGeometry_CheckClearance(fixture.geometry, {16.0f, 16.0f, 0.0f}));
    EXPECT_TRUE(MapGeometry_CheckClearance(fixture.geometry, {64.0f, 64.0f, 0.0f}));
}

TEST(MapGeometry, FixtureRampAndStairsPreserveSupportHeights)
{
    RadarMapLayerFixture fixture;
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {64.0f, 0.0f, 32.0f}, 32.0f);
    fixture.AddStairs(32.0f, 64.0f, 2, {{64.0f, -16.0f}, {128.0f, 16.0f}});

    ASSERT_EQ(fixture.geometry.surfaces.size(), 3u);

    const RadarMapSurface& ramp = fixture.geometry.surfaces[0];
    ASSERT_EQ(ramp.vertices.size(), 4u);
    EXPECT_EQ(ramp.vertices[0].pos, (ncl_math::Vector3{0.0f, -16.0f, 0.0f}));
    EXPECT_EQ(ramp.vertices[2].pos, (ncl_math::Vector3{64.0f, 16.0f, 32.0f}));
    EXPECT_NEAR(ramp.normal.x, -0.4472136f, 0.000001f);
    EXPECT_NEAR(ramp.normal.z, 0.8944272f, 0.000001f);
    EXPECT_FLOAT_EQ(fixture.geometry.surfaces[1].vertices[0].pos.z, 48.0f);
    EXPECT_FLOAT_EQ(fixture.geometry.surfaces[2].vertices[0].pos.z, 64.0f);
    EXPECT_EQ(fixture.geometry.bounds.min, (ncl_math::Vector2{0.0f, -16.0f}));
    EXPECT_EQ(fixture.geometry.bounds.max, (ncl_math::Vector2{128.0f, 16.0f}));
}

TEST(MapGeometry, ReusedClipBuffersPreserveUvsAndClearRejectedOutput)
{
    std::array<RadarMapVertex, 4> polygon{{
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {2.0f, 4.0f}},
        {{64.0f, 0.0f, 64.0f}, {1.0f, 0.0f}, {4.0f, 4.0f}},
        {{64.0f, 64.0f, 64.0f}, {1.0f, 1.0f}, {4.0f, 8.0f}},
        {{0.0f, 64.0f, 0.0f}, {0.0f, 1.0f}, {2.0f, 8.0f}},
    }};
    std::vector<RadarMapVertex> clipped;
    std::vector<RadarMapVertex> scratch;

    for (int repeat = 0; repeat < 2; repeat++)
    {
        MapGeometry_Clip(polygon, {{16.0f, 0.0f, 8.0f}, {32.0f, 64.0f, 40.0f}}, clipped, scratch);

        ASSERT_EQ(clipped.size(), 4u);

        for (const RadarMapVertex& vertex : clipped)
        {
            EXPECT_TRUE(vertex.pos.x == 16.0f || vertex.pos.x == 32.0f);
            EXPECT_FLOAT_EQ(vertex.pos.z, vertex.pos.x);
            EXPECT_FLOAT_EQ(vertex.uv.x, vertex.pos.x / 64.0f);
            EXPECT_FLOAT_EQ(vertex.uv.y, vertex.pos.y / 64.0f);
            EXPECT_FLOAT_EQ(vertex.lightmap_uv.x, vertex.uv.x * 2.0f + 2.0f);
            EXPECT_FLOAT_EQ(vertex.lightmap_uv.y, vertex.uv.y * 4.0f + 4.0f);
        }

        MapGeometry_Clip(clipped, {{20.0f, 0.0f, 8.0f}, {28.0f, 64.0f, 40.0f}}, clipped, scratch);

        ASSERT_EQ(clipped.size(), 4u);

        for (const RadarMapVertex& vertex : clipped)
        {
            EXPECT_TRUE(vertex.pos.x == 20.0f || vertex.pos.x == 28.0f);
            EXPECT_FLOAT_EQ(vertex.uv.x, vertex.pos.x / 64.0f);
            EXPECT_FLOAT_EQ(vertex.lightmap_uv.x, vertex.uv.x * 2.0f + 2.0f);
        }

        MapGeometry_Clip(polygon, {{80.0f, 0.0f, -1.0f}, {96.0f, 64.0f, 65.0f}}, clipped, scratch);

        EXPECT_TRUE(clipped.empty());

        MapGeometry_Clip(polygon, {{32.0f, 0.0f, -1.0f}, {32.0f, 64.0f, 65.0f}}, clipped, scratch);

        EXPECT_TRUE(clipped.empty());
    }
}

TEST(MapGeometry, PlaneEndpointsAndRepeatedVerticesPreserveCyclicOrder)
{
    const std::array<RadarMapVertex, 5> polygon{
        {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{1.0f, 1.0f, 0.0f}}, {{0.0f, 1.0f, 0.0f}}, {{0.0f, 1.0f, 0.0f}}}
    };
    std::vector<RadarMapVertex> clipped = MapGeometry_Clip(polygon, {{0.0f, 0.0f, -1.0f}, {0.5f, 1.0f, 1.0f}});
    ASSERT_EQ(clipped.size(), 4u);
    const std::array<ncl_math::Vector3, 4> expected{{{0.0f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f}, {0.5f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
    for (size_t i = 0; i < expected.size(); i++)
    {
        EXPECT_EQ(clipped[i].pos, expected[i]);
    }
}
