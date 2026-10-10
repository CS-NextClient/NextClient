#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <vector>

#include <goldsrc/engine/model.h>
#include <goldsrc/event_api.h>
#include <goldsrc/format/bsp.h>
#include <goldsrc/pmove.h>

#include "hud/radar/map/MapBspGeometry.h"

namespace
{
    const ncl_math::Aabb3 kCrouchBounds{{-16.0f, -16.0f, -18.0f}, {16.0f, 16.0f, 18.0f}};

    struct WorldFixture
    {
        goldsrc::model_t world{};
        std::array<goldsrc::mvertex_t, 4> vertexes{
            {{{0.0f, 0.0f, 0.0f}}, {{64.0f, 0.0f, 0.0f}}, {{64.0f, 64.0f, 0.0f}}, {{0.0f, 64.0f, 0.0f}}}
        };
        std::array<goldsrc::medge_t, 5> edges{{{}, {{0, 1}}, {{1, 2}}, {{3, 2}}, {{3, 0}}}};
        std::array<int, 4> surfedges{{1, 2, -3, 4}};
        std::array<goldsrc::msurface_t, 3> surfaces{};
        std::array<goldsrc::mplane_t, 2> planes{};
        std::array<goldsrc::dclipnode_t, 2> clipnodes{
            {{0, {goldsrc::CONTENTS_SOLID, goldsrc::CONTENTS_SOLID}}, {1, {goldsrc::CONTENTS_SOLID, goldsrc::CONTENTS_EMPTY}}}
        };
        goldsrc::glpoly_t poly{};

        WorldFixture()
        {
            planes[0].normal[2] = 1.0f;
            planes[1].normal[2] = 1.0f;
            planes[1].dist = 10.0f;

            for (goldsrc::msurface_t& surface : surfaces)
            {
                surface.plane = &planes[0];
                surface.numedges = 4;
                surface.polys = &poly;
            }

            poly.numverts = 4;

            for (size_t i = 0; i < vertexes.size(); i++)
            {
                ncl_math::VectorCopy(vertexes[i].position, poly.verts[i]);
                poly.verts[i][3] = vertexes[i].position[0] / 64.0f;
                poly.verts[i][4] = vertexes[i].position[1] / 64.0f;
                poly.verts[i][5] = vertexes[i].position[0] / 128.0f;
                poly.verts[i][6] = vertexes[i].position[1] / 128.0f;
            }

            world.type = goldsrc::mod_brush;
            world.maxs[0] = world.maxs[1] = 64.0f;
            world.maxs[2] = 128.0f;
            world.firstmodelsurface = 1;
            world.nummodelsurfaces = 1;
            world.numsurfaces = (int)surfaces.size();
            world.surfaces = surfaces.data();
            world.numvertexes = (int)vertexes.size();
            world.vertexes = vertexes.data();
            world.numedges = (int)edges.size();
            world.edges = edges.data();
            world.numsurfedges = (int)surfedges.size();
            world.surfedges = surfedges.data();
            world.numplanes = (int)planes.size();
            world.planes = planes.data();
            world.numclipnodes = (int)clipnodes.size();
            world.clipnodes = clipnodes.data();
            goldsrc::hull_t& crouch = world.hulls[3];
            crouch.clipnodes = clipnodes.data();
            crouch.planes = planes.data();
            crouch.firstclipnode = 1;
            crouch.lastclipnode = 1;
            ncl_math::VectorCopy(ncl_math::Vector3{-8.0f, -8.0f, 0.0f}.data(), crouch.clip_mins);
            ncl_math::VectorCopy(ncl_math::Vector3{24.0f, 24.0f, 36.0f}.data(), crouch.clip_maxs);
            goldsrc::hull_t& standing = world.hulls[1];
            standing = crouch;
            standing.firstclipnode = 0;
            standing.clip_mins[2] = -36.0f;
            standing.clip_maxs[2] = 36.0f;
        }
    };

    struct HullSelectionFake
    {
        WorldFixture fixture;
        goldsrc::event_api_s events{};
        std::unique_ptr<goldsrc::playermove_t> movement = std::make_unique<goldsrc::playermove_t>();
        std::vector<int> calls;
        bool return_null{};
    };

    HullSelectionFake* g_HullSelection{};

    void SetTraceHull(int hull)
    {
        g_HullSelection->movement->usehull = hull;
        g_HullSelection->calls.push_back(hull);
    }

    void* SelectBspHull(goldsrc::physent_t* entity, float* offset)
    {
        EXPECT_EQ(g_HullSelection->movement->usehull, 1);
        EXPECT_EQ(entity->model, &g_HullSelection->fixture.world);
        EXPECT_EQ(ncl_math::Vector3::Ref(entity->origin), ncl_math::Vector3{});
        EXPECT_EQ(ncl_math::Vector3::Ref(entity->angles), ncl_math::Vector3{});

        g_HullSelection->calls.push_back(-1);
        offset[2] = 123.0f;

        return g_HullSelection->return_null ? nullptr : &entity->model->hulls[3];
    }

    class MapBspHullSelection : public testing::Test
    {
    protected:
        HullSelectionFake fake_;
        void SetUp() override
        {
            g_HullSelection = &fake_;
            fake_.movement->usehull = 2;
            fake_.movement->PM_HullForBsp = SelectBspHull;
            fake_.events.EV_SetTraceHull = SetTraceHull;
        }

        void TearDown() override
        {
            g_HullSelection = nullptr;
        }

        const goldsrc::hull_t* Select(std::string& reason)
        {
            return MapBspGeometry_SelectPlayerHull(*fake_.movement, fake_.events, fake_.fixture.world, 1, &reason);
        }
    };
} // namespace

TEST(MapBspGeometry, RespectsWorldSurfaceRange)
{
    WorldFixture fixture;
    RadarMapGeometry geometry{};
    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry));
    ASSERT_EQ(geometry.surfaces.size(), 1u);
    EXPECT_EQ(geometry.surfaces[0].surface_id, 1);
    EXPECT_EQ(geometry.bounds.min, (ncl_math::Vector2{0.0f, 0.0f}));
    EXPECT_EQ(geometry.bounds.max, (ncl_math::Vector2{64.0f, 64.0f}));
}

TEST(MapBspGeometry, CrouchHullUsesItsOffset)
{
    WorldFixture fixture;
    RadarMapGeometry geometry{};
    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry));
    ASSERT_EQ(geometry.collision_hulls.size(), 1u);
    EXPECT_EQ(geometry.collision_hulls[0].offset, (ncl_math::Vector3{8.0f, 8.0f, 18.0f}));
    EXPECT_TRUE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 0.0f}));
    EXPECT_FALSE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 16.0f}));
}

TEST(MapBspGeometry, AcceptsCrouchBoundsWithDifferentHullHeight)
{
    WorldFixture fixture;
    const ncl_math::Aabb3 bounds{{-16.0f, -16.0f, -18.0f}, {16.0f, 16.0f, 32.0f}};
    RadarMapGeometry geometry{};
    std::string reason = "stale";

    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], bounds, geometry, &reason));
    EXPECT_TRUE(reason.empty());
    EXPECT_EQ(geometry.crouch_bounds.max, bounds.max);
    ASSERT_EQ(geometry.collision_hulls.size(), 1u);
    EXPECT_EQ(geometry.collision_hulls[0].offset, (ncl_math::Vector3{8.0f, 8.0f, 18.0f}));
    EXPECT_TRUE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 0.0f}));
    EXPECT_FALSE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 16.0f}));
}

TEST(MapBspGeometry, CopiesDirectedEdgesAndBothUvs)
{
    WorldFixture fixture;
    fixture.surfaces[1].flags = goldsrc::SURF_PLANEBACK | goldsrc::SURF_DRAWTILED;
    RadarMapGeometry geometry{};

    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry));
    ASSERT_EQ(geometry.surfaces.size(), 1u);

    const RadarMapSurface& surface = geometry.surfaces[0];
    ASSERT_EQ(surface.vertices.size(), 4u);
    EXPECT_EQ(surface.normal, (ncl_math::Vector3{0.0f, 0.0f, -1.0f}));
    EXPECT_TRUE(surface.tiled);
    EXPECT_EQ(surface.vertices[2].pos, (ncl_math::Vector3{64.0f, 64.0f, 0.0f}));
    EXPECT_EQ(surface.vertices[2].uv, (ncl_math::Vector2{1.0f, 1.0f}));
    EXPECT_EQ(surface.vertices[2].lightmap_uv, (ncl_math::Vector2{0.5f, 0.5f}));

    fixture.poly.verts[2][3] = 7.0f;
    fixture.planes[1].dist = -100.0f;

    EXPECT_FLOAT_EQ(surface.vertices[2].uv.x, 1.0f);
    EXPECT_TRUE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 0.0f}));
}

TEST(MapBspGeometry, PreservesSkyFlag)
{
    WorldFixture fixture;
    fixture.surfaces[1].flags = goldsrc::SURF_DRAWSKY;
    RadarMapGeometry geometry{};

    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry));
    ASSERT_EQ(geometry.surfaces.size(), 1u);
    EXPECT_TRUE(geometry.surfaces[0].sky);
}

TEST(MapBspGeometry, RejectsMissingSelectedHullClipnodes)
{
    WorldFixture fixture;
    RadarMapGeometry geometry{};
    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry));

    fixture.world.hulls[3].clipnodes = nullptr;
    std::string reason;
    EXPECT_FALSE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry, &reason));
    EXPECT_NE(reason.find("stage=collision_hull clipnodes=missing"), std::string::npos);
    EXPECT_TRUE(geometry.surfaces.empty());
    EXPECT_TRUE(geometry.collision_hulls.empty());
}

TEST(MapBspGeometry, UnreadyPolygonsClearTheOutput)
{
    WorldFixture fixture;
    RadarMapGeometry geometry{};
    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry));

    fixture.surfaces[1].polys = nullptr;
    std::string reason;
    EXPECT_FALSE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry, &reason));
    EXPECT_NE(reason.find("stage=surface_uvs surface=1 flags=0"), std::string::npos);
    EXPECT_NE(reason.find("polys=missing poly_vertices=0"), std::string::npos);
    EXPECT_TRUE(geometry.surfaces.empty());
    EXPECT_TRUE(geometry.collision_hulls.empty());

    fixture.surfaces[1].flags = goldsrc::SURF_DRAWSKY;

    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry, &reason));
    EXPECT_TRUE(reason.empty());
    EXPECT_TRUE(geometry.surfaces[0].sky);
}

TEST(MapBspGeometry, InterpolatesUvsAtACollinearSurfedge)
{
    WorldFixture fixture;
    std::array<goldsrc::mvertex_t, 5> vertexes{{
        {{0.0f, 0.0f, 0.0f}},
        {{32.0f, 0.0f, 0.0f}},
        {{64.0f, 0.0f, 0.0f}},
        {{64.0f, 64.0f, 0.0f}},
        {{0.0f, 64.0f, 0.0f}},
    }};
    std::array<goldsrc::medge_t, 6> edges{{{}, {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 4}}, {{4, 0}}}};
    std::array<int, 5> surfedges{{1, 2, 3, 4, 5}};
    fixture.world.vertexes = vertexes.data();
    fixture.world.numvertexes = (int)vertexes.size();
    fixture.world.edges = edges.data();
    fixture.world.numedges = (int)edges.size();
    fixture.world.surfedges = surfedges.data();
    fixture.world.numsurfedges = (int)surfedges.size();
    fixture.surfaces[1].numedges = 5;
    RadarMapGeometry geometry{};

    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry));
    ASSERT_EQ(geometry.surfaces[0].vertices.size(), 5u);

    const RadarMapVertex& midpoint = geometry.surfaces[0].vertices[1];
    EXPECT_EQ(midpoint.pos, (ncl_math::Vector3{32.0f, 0.0f, 0.0f}));
    EXPECT_EQ(midpoint.uv, (ncl_math::Vector2{0.5f, 0.0f}));
    EXPECT_EQ(midpoint.lightmap_uv, (ncl_math::Vector2{0.25f, 0.0f}));

    vertexes[1].position[1] = 0.002f;

    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry));
    EXPECT_FLOAT_EQ(geometry.surfaces[0].vertices[1].uv.y, 0.00003125f);
    EXPECT_FLOAT_EQ(geometry.surfaces[0].vertices[1].lightmap_uv.y, 0.000015625f);
}

TEST(MapBspGeometry, IdentifiesMissingWorldArrayAndClearsReasonOnSuccess)
{
    WorldFixture fixture;
    fixture.world.vertexes = nullptr;
    RadarMapGeometry geometry;
    std::string reason;

    EXPECT_FALSE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry, &reason));
    EXPECT_NE(reason.find("stage=world_data"), std::string::npos);
    EXPECT_NE(reason.find("vertices=missing"), std::string::npos);
    EXPECT_TRUE(geometry.surfaces.empty());

    fixture.world.vertexes = fixture.vertexes.data();

    ASSERT_TRUE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry, &reason));
    EXPECT_TRUE(reason.empty());
}

TEST(MapBspGeometry, IdentifiesInvalidCollisionPlane)
{
    WorldFixture fixture;
    fixture.clipnodes[1].planenum = fixture.world.numplanes;
    RadarMapGeometry geometry;
    std::string reason;

    EXPECT_FALSE(MapBspGeometry_Collect(fixture.world, fixture.world.hulls[3], kCrouchBounds, geometry, &reason));
    EXPECT_NE(reason.find("stage=collision_plane node=1 plane=2 world_planes=2"), std::string::npos);
    EXPECT_TRUE(geometry.surfaces.empty());
    EXPECT_TRUE(geometry.collision_hulls.empty());
}

TEST_F(MapBspHullSelection, SelectsEngineHullAndRestoresTraceHull)
{
    std::string reason = "stale";
    const goldsrc::hull_t* hull = Select(reason);
    ASSERT_EQ(hull, &fake_.fixture.world.hulls[3]);
    EXPECT_TRUE(reason.empty());
    EXPECT_EQ(fake_.calls, (std::vector<int>{1, -1, 2}));
    EXPECT_EQ(fake_.movement->usehull, 2);

    const ncl_math::Aabb3 bounds{{-16.0f, -16.0f, -18.0f}, {16.0f, 16.0f, 32.0f}};
    RadarMapGeometry geometry{};
    ASSERT_TRUE(MapBspGeometry_Collect(fake_.fixture.world, *hull, bounds, geometry, &reason));
    EXPECT_EQ(geometry.collision_hulls[0].offset, (ncl_math::Vector3{8.0f, 8.0f, 18.0f}));
    EXPECT_TRUE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 0.0f}));
    EXPECT_FALSE(MapGeometry_CheckClearance(geometry, {0.0f, 0.0f, 16.0f}));
}

TEST_F(MapBspHullSelection, RestoresTraceHullWhenCallbackHasNoHull)
{
    fake_.return_null = true;
    std::string reason;
    EXPECT_EQ(Select(reason), nullptr);
    EXPECT_EQ(fake_.calls, (std::vector<int>{1, -1, 2}));
    EXPECT_EQ(fake_.movement->usehull, 2);
    EXPECT_EQ(reason, "stage=crouch_hull callback=PM_HullForBsp selected_hull=missing");

    fake_.return_null = false;

    ASSERT_NE(Select(reason), nullptr);
    EXPECT_TRUE(reason.empty());
}

TEST_F(MapBspHullSelection, MissingCallbacksLeaveTraceHullUnchanged)
{
    std::string reason;
    fake_.movement->PM_HullForBsp = nullptr;

    EXPECT_EQ(Select(reason), nullptr);
    EXPECT_EQ(reason, "stage=crouch_hull missing=PM_HullForBsp");

    fake_.movement->PM_HullForBsp = SelectBspHull;
    fake_.events.EV_SetTraceHull = nullptr;

    EXPECT_EQ(Select(reason), nullptr);
    EXPECT_EQ(reason, "stage=crouch_hull missing=EV_SetTraceHull");
    EXPECT_TRUE(fake_.calls.empty());
    EXPECT_EQ(fake_.movement->usehull, 2);
}
