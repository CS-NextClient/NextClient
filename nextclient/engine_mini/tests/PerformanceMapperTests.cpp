#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/engine/server_state.h>
#include <goldsrc/entity.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/view.h>

#include "performance/PerformanceMapper.h"
#include "performance/performance_dump.h"

namespace
{
    struct PlaneModel
    {
        goldsrc::model_t model{};
        std::vector<goldsrc::mplane_t> planes;
        std::vector<goldsrc::msurface_t> surfaces;
        std::vector<goldsrc::mvertex_t> vertices;
        std::vector<goldsrc::medge_t> edges;
        std::vector<int> surfedges;
        goldsrc::texture_t texture{};
        goldsrc::mtexinfo_t texinfo{};

        explicit PlaneModel(std::initializer_list<float> distances)
        {
            planes.resize(distances.size());
            surfaces.resize(distances.size());
            vertices.resize(distances.size() * 4);
            edges.resize(distances.size() * 4 + 1);
            surfedges.resize(distances.size() * 4);
            std::snprintf(texture.name, sizeof(texture.name), "WALL");
            texinfo.texture = &texture;
            size_t index = 0;
            for (float distance : distances)
            {
                planes[index].normal[0] = 1;
                planes[index].dist = distance;
                surfaces[index].plane = &planes[index];
                surfaces[index].firstedge = int(index * 4);
                surfaces[index].numedges = 4;
                surfaces[index].texinfo = &texinfo;
                for (int edge = 0; edge < 4; ++edge)
                {
                    size_t vertex = index * 4 + edge;
                    vertices[vertex].position[0] = distance;
                    vertices[vertex].position[1] = edge == 0 || edge == 3 ? -10.0f : 10.0f;
                    vertices[vertex].position[2] = edge < 2 ? -10.0f : 10.0f;
                    edges[vertex + 1].v[0] = static_cast<unsigned short>(vertex);
                    edges[vertex + 1].v[1] = static_cast<unsigned short>(index * 4 + (edge + 1) % 4);
                    surfedges[vertex] = int(vertex + 1);
                }
                ++index;
            }
            model.type = goldsrc::mod_brush;
            model.nummodelsurfaces = int(surfaces.size());
            model.surfaces = surfaces.data();
            model.vertexes = vertices.data();
            model.edges = edges.data();
            model.surfedges = surfedges.data();
            model.mins[0] = distances.size() ? *std::min_element(distances.begin(), distances.end()) : 0;
            model.maxs[0] = distances.size() ? *std::max_element(distances.begin(), distances.end()) : 0;
            model.mins[1] = model.mins[2] = -10;
            model.maxs[1] = model.maxs[2] = 10;
        }
    };

    class PerformanceMapperTest : public testing::Test
    {
    protected:
        performance::PerformanceMapper mapper;
        std::unique_ptr<goldsrc::client_state_t> client = std::make_unique<goldsrc::client_state_t>();
        std::unique_ptr<goldsrc::client_static_t> session = std::make_unique<goldsrc::client_static_t>();
        std::unique_ptr<goldsrc::server_t> server = std::make_unique<goldsrc::server_t>();
        PlaneModel world{{100}};
        goldsrc::refdef_t view{};

        void SetUp() override
        {
            client->worldmodel = &world.model;
            view.vrect = {0, 0, 640, 480, nullptr};
            session->state = goldsrc::ca_active;
            session->netchan.remote_address.type = goldsrc::NA_LOOPBACK;
            server->active = true;
            mapper.SetLevel(2, session.get(), server.get());
        }

        void Capture(std::span<goldsrc::cl_entity_t* const> entities = {})
        {
            mapper.UpdateWorld(client.get(), 1);
            mapper.BeginFrame();
            int wpoly = 0;
            int epoly = 0;
            mapper.BeginView(wpoly, epoly);
            mapper.CaptureView(1234, 60, entities, view);
        }

        std::string Property(const char* name)
        {
            for (const performance::PerfMapProperty& property : mapper.get_snapshot().properties)
            {
                if (property.name == name)
                {
                    return property.value;
                }
            }
            return {};
        }
    };
} // namespace

TEST(PerformanceMapperSessionTest, LevelTwoDefaultsToSummaryWithoutLocalSession)
{
    performance::PerformanceMapper mapper;
    mapper.SetLevel(2);
    EXPECT_EQ(mapper.get_snapshot().level, 1);
    EXPECT_TRUE(mapper.is_enabled());
}

TEST_F(PerformanceMapperTest, LevelTwoRequiresActiveLoopbackClientAndServerWithoutDemoPlayback)
{
    EXPECT_EQ(mapper.get_snapshot().level, 2);
    client->maxclients = 32;
    mapper.SetLevel(2, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 2);

    session->netchan.remote_address.type = goldsrc::NA_IP;
    mapper.SetLevel(2, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 1);

    session->netchan.remote_address.type = goldsrc::NA_LOOPBACK;
    server->active = false;
    mapper.SetLevel(2, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 1);

    server->active = true;
    session->demoplayback = true;
    mapper.SetLevel(2, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 1);

    session->demoplayback = false;
    session->state = goldsrc::ca_connected;
    mapper.SetLevel(2, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 1);

    session->state = goldsrc::ca_active;
    mapper.SetLevel(2, nullptr, server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 1);
    mapper.SetLevel(2, session.get(), nullptr);
    EXPECT_EQ(mapper.get_snapshot().level, 1);

    mapper.SetLevel(2, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 2);
    mapper.SetLevel(1, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 1);
    mapper.SetLevel(0, session.get(), server.get());
    EXPECT_FALSE(mapper.is_enabled());
}

TEST_F(PerformanceMapperTest, LeavingLocalSessionDiscardsDetailsAndRestoresThemOnReturn)
{
    Capture();
    ASSERT_FALSE(mapper.get_snapshot().properties.empty());
    ASSERT_FALSE(mapper.get_snapshot().outline.empty());

    session->netchan.remote_address.type = goldsrc::NA_IP;
    mapper.SetLevel(2, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().level, 1);
    EXPECT_TRUE(mapper.get_snapshot().properties.empty());
    EXPECT_TRUE(mapper.get_snapshot().outline.empty());
    EXPECT_FALSE(mapper.HandleKey('e', true, true));
    mapper.ResetInput();

    Capture();
    EXPECT_TRUE(mapper.get_snapshot().target_hit);
    EXPECT_EQ(mapper.get_snapshot().wpoly, 1234u);
    EXPECT_TRUE(mapper.get_snapshot().properties.empty());
    EXPECT_TRUE(mapper.get_snapshot().outline.empty());
    EXPECT_TRUE(mapper.HandleKey('e', true, true));

    uint32_t required = performance::dump::FormatMapper(mapper.get_snapshot(), nullptr, 0);
    std::string output(required, '\0');
    performance::dump::FormatMapper(mapper.get_snapshot(), output.data(), required);
    EXPECT_EQ(output.find("Mapper: level=1"), 0u);
    EXPECT_EQ(output.find("texture:"), std::string::npos);
    uint64_t revision = mapper.get_snapshot().revision;
    mapper.SetLevel(2, session.get(), server.get());
    EXPECT_EQ(mapper.get_snapshot().revision, revision);
    EXPECT_TRUE(mapper.get_snapshot().view_measured);

    session->netchan.remote_address.type = goldsrc::NA_LOOPBACK;
    mapper.SetLevel(2, session.get(), server.get());
    Capture();
    EXPECT_EQ(mapper.get_snapshot().level, 2);
    EXPECT_FALSE(mapper.get_snapshot().properties.empty());
    EXPECT_FALSE(mapper.get_snapshot().outline.empty());
}

TEST_F(PerformanceMapperTest, DumpKeyFiresOncePerGameplayPressAndPreservesOtherKeys)
{
    Capture();
    EXPECT_FALSE(mapper.HandleKey('f', true, true));
    EXPECT_TRUE(mapper.HandleKey('e', true, true));
    EXPECT_FALSE(mapper.HandleKey('e', true, true));
    EXPECT_FALSE(mapper.HandleKey('e', false, true));
    EXPECT_TRUE(mapper.HandleKey('e', true, true));
    EXPECT_FALSE(mapper.HandleKey('e', false, false));
    EXPECT_FALSE(mapper.HandleKey('e', true, false));
    EXPECT_FALSE(mapper.HandleKey('e', true, true));
    mapper.HandleKey('e', false, true);
    mapper.SetLevel(0);
    EXPECT_FALSE(mapper.HandleKey('e', true, true));
}

TEST_F(PerformanceMapperTest, InputDeactivationRearmsDumpKeyWithoutHudRelease)
{
    Capture();
    ASSERT_TRUE(mapper.HandleKey('e', true, true));
    mapper.ResetInput();
    EXPECT_TRUE(mapper.HandleKey('e', true, true));
    EXPECT_FALSE(mapper.HandleKey('e', true, true));
}

TEST_F(PerformanceMapperTest, LevelTwoOutlinesOnlyTheHitWorldPolygonAndClearsOnMissingView)
{
    Capture();
    const std::vector<performance::PerfMapLine>& outline = mapper.get_snapshot().outline;
    ASSERT_EQ(outline.size(), 4u);
    EXPECT_FLOAT_EQ(outline[0].start.x, 100);
    EXPECT_FLOAT_EQ(outline[0].start.y, -10);
    EXPECT_FLOAT_EQ(outline[0].end.y, 10);
    mapper.BeginFrame();
    EXPECT_TRUE(mapper.get_snapshot().outline.empty());
    mapper.SetLevel(1);
    Capture();
    EXPECT_TRUE(mapper.get_snapshot().outline.empty());
}

TEST_F(PerformanceMapperTest, ImmediateMapDumpContainsUntruncatedPropertiesWithoutCpuCapture)
{
    std::string value(600, 'x');
    std::string entities = "{\"classname\" \"worldspawn\" \"message\" \"" + value + "\"}";
    world.model.entities = entities.data();
    Capture();
    const performance::PerfMapperSnapshot& snapshot = mapper.get_snapshot();
    uint32_t required = performance::dump::FormatMapper(snapshot, nullptr, 0);
    std::string output(required, '\0');
    EXPECT_EQ(performance::dump::FormatMapper(snapshot, output.data(), required), required);
    EXPECT_EQ(output.find("Mapper: level=2"), 0u);
    EXPECT_NE(output.find("wpoly=1234 epoly=60"), std::string::npos);
    EXPECT_NE(output.find("Object: worldspawn"), std::string::npos);
    EXPECT_NE(output.find("bsp.message: " + value), std::string::npos);
    EXPECT_EQ(output.find("CPU capture"), std::string::npos);
    std::array<char, 8> truncated{};
    EXPECT_EQ(performance::dump::FormatMapper(snapshot, truncated.data(), uint32_t(truncated.size())), required);
    EXPECT_EQ(truncated.back(), '\0');
}

TEST(PerformanceMapperDumpTest, ObjectIdentityAppearsOnlyAtLevelTwoWithAndWithoutTarget)
{
    std::unique_ptr<performance::PerfSnapshot> performance = std::make_unique<performance::PerfSnapshot>();
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = true;
    mapper.target_name = "func_door";
    mapper.target_position = {4, 5, 6};
    mapper.target_origin = {7, 8, 9};
    mapper.properties.push_back({"bsp.targetname", "door"});

    for (int level : {1, 2})
    {
        for (bool hit : {false, true})
        {
            SCOPED_TRACE(testing::Message() << "level=" << level << " hit=" << hit);
            mapper.level = level;
            mapper.target_hit = hit;
            for (bool full_capture : {false, true})
            {
                uint32_t required = full_capture ? performance::dump::Format(*performance, nullptr, 0, nullptr, &mapper)
                                                 : performance::dump::FormatMapper(mapper, nullptr, 0);
                std::string output(required, '\0');
                if (full_capture)
                {
                    performance::dump::Format(*performance, output.data(), required, nullptr, &mapper);
                }
                else
                {
                    performance::dump::FormatMapper(mapper, output.data(), required);
                }

                EXPECT_NE(output.find(hit ? "Look: 4.0 5.0 6.0" : "Look: -"), std::string::npos);
                EXPECT_EQ(output.find("Object:") != std::string::npos, level == 2);
                EXPECT_EQ(output.find("Object origin:") != std::string::npos, level == 2);
                if (level == 2)
                {
                    EXPECT_NE(output.find(hit ? "Object origin: 7.0 8.0 9.0" : "Object origin: -"), std::string::npos);
                }
                EXPECT_EQ(output.find("func_door") != std::string::npos, level == 2 && hit);
                EXPECT_EQ(output.find("bsp.targetname") != std::string::npos, level == 2 && hit);
            }
        }
    }
}

TEST_F(PerformanceMapperTest, CountsMapBrushEntitiesAndOnlyVisibleBrushModels)
{
    char entities[] =
        "// map\n{\"classname\" \"worldspawn\"} {\"classname\" \"func_door\" \"model\" \"*1\"} "
        "{\"classname\" \"func_wall\" \"model\" \"*2\"} {\"classname\" \"env_sprite\" \"model\" \"a.spr\"}";
    world.model.entities = entities;
    PlaneModel brush{{50}};
    goldsrc::model_t studio{};
    studio.type = goldsrc::mod_studio;
    goldsrc::cl_entity_t world_entity{};
    goldsrc::cl_entity_t brush_entity{};
    goldsrc::cl_entity_t studio_entity{};
    world_entity.model = &world.model;
    brush_entity.model = &brush.model;
    studio_entity.model = &studio;
    goldsrc::cl_entity_t* visible[] = {&world_entity, &brush_entity, &studio_entity, nullptr};
    Capture(visible);
    EXPECT_TRUE(mapper.get_snapshot().map_entities_available);
    EXPECT_EQ(mapper.get_snapshot().brush_entities, 2u);
    EXPECT_EQ(mapper.get_snapshot().pvs_brush_entities, 1u);
}

TEST_F(PerformanceMapperTest, WorldHitHasExactCoordinatesTextureAndBspProperties)
{
    char entities[] = "{\"classname\" \"worldspawn\" \"message\" \"Test map\"}";
    world.model.entities = entities;
    client->simorg[2] = -28;
    view.vieworg[1] = 3;
    Capture();
    const performance::PerfMapperSnapshot& snapshot = mapper.get_snapshot();
    EXPECT_TRUE(snapshot.target_hit);
    EXPECT_EQ(snapshot.target_name, "worldspawn");
    EXPECT_FLOAT_EQ(snapshot.position.z, -28);
    EXPECT_FLOAT_EQ(snapshot.target_position.x, 100);
    EXPECT_FLOAT_EQ(snapshot.target_position.y, 3);
    EXPECT_FLOAT_EQ(snapshot.target_origin.x, 0);
    EXPECT_FLOAT_EQ(snapshot.target_origin.y, 0);
    EXPECT_FLOAT_EQ(snapshot.target_origin.z, 0);
    EXPECT_EQ(Property("texture"), "WALL");
    EXPECT_EQ(Property("bsp.message"), "Test map");
}

TEST_F(PerformanceMapperTest, RotatedNonsolidBrushUsesGeometryAndInlineModelMetadata)
{
    char entities[] = "{\"classname\" \"worldspawn\"} {\"classname\" \"func_door\" \"targetname\" \"door\" \"model\" \"*1\"}";
    world.model.entities = entities;
    PlaneModel brush{{10}};
    std::snprintf(brush.model.name, sizeof(brush.model.name), "*1");
    goldsrc::cl_entity_t entity{};
    entity.index = 55;
    entity.origin[0] = 50;
    entity.angles[1] = 90;
    entity.model = &brush.model;
    goldsrc::cl_entity_t* visible[] = {&entity};
    view.vieworg[0] = 50;
    view.viewangles[1] = 90;
    Capture(visible);
    EXPECT_EQ(mapper.get_snapshot().target_name, "func_door (door)");
    EXPECT_NEAR(mapper.get_snapshot().target_position.x, 50, 0.001f);
    EXPECT_NEAR(mapper.get_snapshot().target_position.y, 10, 0.001f);
    EXPECT_FLOAT_EQ(mapper.get_snapshot().target_origin.x, 50);
    EXPECT_FLOAT_EQ(mapper.get_snapshot().target_origin.y, 0);
    EXPECT_FLOAT_EQ(mapper.get_snapshot().target_origin.z, 0);
    EXPECT_EQ(Property("index"), "55");
    EXPECT_EQ(Property("solid"), "0");
    const std::vector<performance::PerfMapLine>& outline = mapper.get_snapshot().outline;
    ASSERT_EQ(outline.size(), 4u);
    EXPECT_NEAR(outline[0].start.x, 60, 0.001f);
    EXPECT_NEAR(outline[0].start.y, 10, 0.001f);
    EXPECT_NEAR(outline[0].end.x, 40, 0.001f);
}

TEST_F(PerformanceMapperTest, WorldOccludesFartherEntitiesAndTraceRejectsOutsidePolygon)
{
    PlaneModel brush{{150}};
    std::snprintf(brush.model.name, sizeof(brush.model.name), "*1");
    goldsrc::cl_entity_t entity{};
    entity.model = &brush.model;
    goldsrc::cl_entity_t* visible[] = {&entity};
    Capture(visible);
    EXPECT_EQ(mapper.get_snapshot().target_name, "worldspawn");
    view.vieworg[1] = 20;
    Capture(visible);
    EXPECT_FALSE(mapper.get_snapshot().target_hit);
    EXPECT_TRUE(mapper.get_snapshot().outline.empty());
    EXPECT_TRUE(mapper.get_snapshot().properties.empty());
}

TEST_F(PerformanceMapperTest, AmbiguousInlineMetadataDoesNotInventAClassname)
{
    char entities[] = "{\"model\" \"*1\" \"classname\" \"func_door\"} {\"model\" \"*1\" \"classname\" \"func_wall\"}";
    world.model.entities = entities;
    PlaneModel brush{{50}};
    std::snprintf(brush.model.name, sizeof(brush.model.name), "*1");
    goldsrc::cl_entity_t entity{};
    entity.model = &brush.model;
    goldsrc::cl_entity_t* visible[] = {&entity};
    Capture(visible);
    EXPECT_EQ(mapper.get_snapshot().target_name, "*1");
    EXPECT_TRUE(Property("bsp.classname").empty());
}

TEST_F(PerformanceMapperTest, ShaderTrianglesCombineOncePerViewAndInsetPreservesMainView)
{
    mapper.UpdateWorld(client.get(), 1);
    mapper.BeginFrame();
    int wpoly = 123;
    int epoly = 456;
    mapper.BeginView(wpoly, epoly);
    EXPECT_EQ(wpoly, 0);
    EXPECT_EQ(epoly, 0);
    mapper.AddShaderTriangles(140);
    mapper.CaptureView(1234, 60, {}, view);
    EXPECT_EQ(mapper.get_snapshot().epoly, 200u);
    view.vrect = {0, 0, 160, 120, nullptr};
    mapper.BeginView(wpoly, epoly);
    mapper.CaptureView(1, 2, {}, view);
    EXPECT_EQ(mapper.get_snapshot().wpoly, 1234u);
    EXPECT_EQ(mapper.get_snapshot().epoly, 200u);
    mapper.BeginFrame();
    EXPECT_FALSE(mapper.get_snapshot().view_measured);
    EXPECT_FALSE(mapper.get_snapshot().target_hit);
}

TEST_F(PerformanceMapperTest, SurfaceTreeFindsNearestFaceWithNegativeSurfedges)
{
    PlaneModel many{{900, 800, 700, 600, 500, 400, 300, 200, 100, 1200, 1100, 1000}};
    for (int edge = 0; edge < 4; ++edge)
    {
        many.surfedges[32 + edge] = -(36 - edge);
    }
    client->worldmodel = &many.model;
    Capture();
    EXPECT_TRUE(mapper.get_snapshot().target_hit);
    EXPECT_FLOAT_EQ(mapper.get_snapshot().target_position.x, 100);
}

TEST_F(PerformanceMapperTest, MissingOrTruncatedEntityLumpLeavesMapCountUnavailable)
{
    Capture();
    EXPECT_FALSE(mapper.get_snapshot().map_entities_available);
    char truncated[] = "{\"model\" \"*1\"} {\"classname\" \"unterminated";
    world.model.entities = truncated;
    mapper.Reset();
    Capture();
    EXPECT_FALSE(mapper.get_snapshot().map_entities_available);
}

TEST_F(PerformanceMapperTest, QuotedBracesRemainPropertyValues)
{
    char entities[] = "{\"classname\" \"worldspawn\" \"message\" \"}\"}";
    world.model.entities = entities;
    Capture();
    EXPECT_TRUE(mapper.get_snapshot().map_entities_available);
    EXPECT_EQ(Property("bsp.message"), "}");
}

TEST_F(PerformanceMapperTest, MapGenerationDisconnectAndDisableDiscardPointersAndTarget)
{
    Capture();
    ASSERT_TRUE(mapper.get_snapshot().target_hit);
    mapper.UpdateWorld(client.get(), 2);
    EXPECT_FALSE(mapper.get_snapshot().view_measured);
    EXPECT_TRUE(mapper.get_snapshot().properties.empty());
    client->worldmodel = nullptr;
    mapper.UpdateWorld(client.get(), 2);
    EXPECT_FALSE(mapper.get_snapshot().map_loaded);
    EXPECT_FALSE(mapper.get_snapshot().target_hit);
    mapper.SetLevel(0);
    EXPECT_FALSE(mapper.is_enabled());
}

TEST_F(PerformanceMapperTest, LevelOneKeepsLookCoordinatesWithoutObjectIdentityOrProperties)
{
    mapper.SetLevel(1);
    goldsrc::model_t model{};
    model.type = goldsrc::mod_studio;
    std::snprintf(model.name, sizeof(model.name), "models/test.mdl");
    model.mins[0] = 40;
    model.maxs[0] = 50;
    model.mins[1] = model.mins[2] = -5;
    model.maxs[1] = model.maxs[2] = 5;
    goldsrc::cl_entity_t entity{};
    entity.model = &model;
    goldsrc::cl_entity_t* visible[] = {&entity};
    Capture(visible);
    EXPECT_TRUE(mapper.get_snapshot().target_bounds);
    EXPECT_TRUE(mapper.get_snapshot().target_name.empty());
    EXPECT_FLOAT_EQ(mapper.get_snapshot().target_position.x, 40);
    EXPECT_TRUE(mapper.get_snapshot().properties.empty());
    EXPECT_EQ(mapper.get_snapshot().pvs_brush_entities, 0u);
}

TEST_F(PerformanceMapperTest, ScaledSpriteUsesItsRenderedBounds)
{
    goldsrc::model_t model{};
    model.type = goldsrc::mod_sprite;
    std::snprintf(model.name, sizeof(model.name), "sprites/test.spr");
    for (int axis = 0; axis < 3; ++axis)
    {
        model.mins[axis] = -5;
        model.maxs[axis] = 5;
    }
    goldsrc::cl_entity_t entity{};
    entity.model = &model;
    entity.origin[0] = 50;
    entity.curstate.scale = 2;
    goldsrc::cl_entity_t* visible[] = {&entity};
    view.vieworg[1] = 8;
    Capture(visible);
    EXPECT_EQ(mapper.get_snapshot().target_name, "sprites/test.spr");
    EXPECT_FLOAT_EQ(mapper.get_snapshot().target_position.x, 40);
    ASSERT_EQ(mapper.get_snapshot().outline.size(), 12u);
    EXPECT_FLOAT_EQ(mapper.get_snapshot().outline[0].start.x, 40);
    EXPECT_FLOAT_EQ(mapper.get_snapshot().outline[0].end.x, 60);
}

TEST_F(PerformanceMapperTest, StudioBoundsUseTheStudioPitchConvention)
{
    goldsrc::studiohdr_t header{};
    header.bbmin[0] = 40;
    header.bbmax[0] = 50;
    header.bbmin[1] = header.bbmin[2] = -5;
    header.bbmax[1] = header.bbmax[2] = 5;
    goldsrc::model_t model{};
    model.type = goldsrc::mod_studio;
    model.cache.data = &header;
    goldsrc::cl_entity_t entity{};
    entity.model = &model;
    entity.angles[0] = 90;
    goldsrc::cl_entity_t* visible[] = {&entity};
    view.viewangles[0] = -90;
    Capture(visible);
    EXPECT_TRUE(mapper.get_snapshot().target_bounds);
    EXPECT_NEAR(mapper.get_snapshot().target_position.z, 40, 0.001f);
}

TEST_F(PerformanceMapperTest, StudioUsesCachedSequenceBoundsWhenModelBoundsAreZero)
{
    struct StudioFile
    {
        goldsrc::studiohdr_t header{};
        goldsrc::mstudioseqdesc_t sequence{};
    } file;
    file.header.length = sizeof(file);
    file.header.numseq = 1;
    file.header.seqindex = offsetof(StudioFile, sequence);
    file.sequence.bbmin[0] = 40;
    file.sequence.bbmax[0] = 50;
    file.sequence.bbmin[1] = file.sequence.bbmin[2] = -5;
    file.sequence.bbmax[1] = file.sequence.bbmax[2] = 5;
    goldsrc::model_t model{};
    model.type = goldsrc::mod_studio;
    model.cache.data = &file;
    std::snprintf(model.name, sizeof(model.name), "models/cached.mdl");
    goldsrc::cl_entity_t entity{};
    entity.model = &model;
    goldsrc::cl_entity_t* visible[] = {&entity};
    Capture(visible);
    EXPECT_TRUE(mapper.get_snapshot().target_bounds);
    EXPECT_EQ(mapper.get_snapshot().target_name, "models/cached.mdl");
    EXPECT_FLOAT_EQ(mapper.get_snapshot().target_position.x, 40);
}
