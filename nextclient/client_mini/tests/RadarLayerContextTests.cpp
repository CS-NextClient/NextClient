#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include <goldsrc/pmove.h>
#include <goldsrc/event_api.h>
#include <goldsrc/engine/model.h>

#include "hud/radar/map/Overview.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "RadarMapLayerFixtures.h"
#include "hud/radar/render/RadarView.h"

TEST(RadarLayerContext, UsesObservedPlayer)
{
    for (int mode : {goldsrc::OBS_CHASE_LOCKED, goldsrc::OBS_CHASE_FREE, goldsrc::OBS_IN_EYE, goldsrc::OBS_MAP_CHASE})
    {
        RadarLayerSource source = RadarLayerContext_ChooseSource(mode, 1, 5, true);
        EXPECT_FALSE(source.camera);
        EXPECT_EQ(source.entity, 5);
    }

    EXPECT_EQ(RadarLayerContext_ChooseSource(goldsrc::OBS_NONE, 1, 5, false).entity, 1);
    EXPECT_EQ(RadarLayerContext_ChooseSource(goldsrc::OBS_IN_EYE, 1, 0, true).entity, 0);
}

TEST(RadarLayerContext, FreeCameraMatchesRadarViewPolicy)
{
    for (int mode :
         {goldsrc::OBS_NONE,
          goldsrc::OBS_CHASE_LOCKED,
          goldsrc::OBS_CHASE_FREE,
          goldsrc::OBS_ROAMING,
          goldsrc::OBS_IN_EYE,
          goldsrc::OBS_MAP_FREE,
          goldsrc::OBS_MAP_CHASE})
    {
        EXPECT_EQ(RadarLayerContext_ChooseSource(mode, 1, 5, true).camera, RadarView_IsFreeCamera(mode));
    }
}

TEST(RadarLayerContext, ModesFiveAndSixOnly)
{
    for (int mode = 0; mode <= 6; mode++)
    {
        EXPECT_EQ(RadarLayerContext_IsLayeredMode(static_cast<RadarMapMode>(mode)), mode == 5 || mode == 6);
    }
}

TEST(RadarLayerContext, CrouchPreservesFeet)
{
    EXPECT_EQ(RadarLayerContext_Feet({10, 20, 36}, {-16, -16, -36}), ncl_math::Vector3(10, 20, 0));
    EXPECT_EQ(RadarLayerContext_Feet({10, 20, 18}, {-16, -16, -18}), ncl_math::Vector3(10, 20, 0));
}

TEST(RadarLayerContext, VerticalTeleportSelectsImmediately)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});
    fixture.AddFloor(256, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;

    RadarLayerContext context;
    context.Update(map, {{100, 100, 0}, ncl_math::Vector3(100, 100, 0), 1, 1, true, true}, 1, 320);

    ASSERT_EQ(context.get_view().active, 0);

    context.Update(map, {{100, 100, 256}, ncl_math::Vector3(100, 100, 256), 1.01f, 1, true, true}, 1.01f, 320);

    EXPECT_EQ(context.get_view().active, 1);
    EXPECT_EQ(context.get_selection().get_reason(), RadarLayerReason::Discontinuity);
}

TEST(RadarLayerContext, FastFallRetainsConfirmation)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});
    fixture.AddFloor(256, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;

    RadarLayerContext context;
    context.Update(map, {{100, 100, 256}, {}, 1, 1, true, false}, 1, 1600);

    ASSERT_EQ(context.get_view().active, 1);

    context.Update(map, {{100, 100, 96}, {}, 1.1f, 1, true, false}, 1.1f, 1600);

    EXPECT_EQ(context.get_view().active, 1);
    EXPECT_EQ(context.get_selection().get_candidate(), 0);
    EXPECT_EQ(context.get_selection().get_reason(), RadarLayerReason::Pending);
}

TEST(RadarLayerContext, SnapshotElapsedAllowsReachableMotion)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    RadarLayerContext context;
    context.Update(map, {{10, 10, 0}, ncl_math::Vector3(10, 10, 0), 1, 5, true, true}, 1, 320);
    context.Update(map, {{10, 10, 0}, ncl_math::Vector3(10, 10, 0), 1.08f, 5, true, true}, 1, 320);
    context.Update(map, {{42, 10, 0}, ncl_math::Vector3(42, 10, 0), 1.1f, 5, true, true}, 1.1f, 320);

    EXPECT_EQ(context.get_selection().get_reason(), RadarLayerReason::Unchanged);
}

TEST(RadarLayerLifecycle, ResetDoesNotRetainMarkerLayerIds)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    RadarLayerContext context;
    context.Update(map, {{10, 10, 0}, {}, 1, 1, true}, 1, 320);

    ASSERT_TRUE(context.get_view().enabled);

    context.Reset();

    EXPECT_FALSE(context.get_view().enabled);
    EXPECT_EQ(context.get_view().active, -1);
    EXPECT_EQ(context.get_view().map, nullptr);
    EXPECT_EQ(context.get_selection().get_active(), -1);
}

namespace
{
    struct TraceFake
    {
        goldsrc::event_api_s events{};
        std::unique_ptr<goldsrc::playermove_t> movement = std::make_unique<goldsrc::playermove_t>();
        goldsrc::physent_t hit{};
        goldsrc::model_t model{};
        goldsrc::pmtrace_t trace{};
        ncl_math::Aabb3 bounds{{-16, -16, -36}, {16, 16, 36}};
        std::vector<int> calls;
        ncl_math::Vector3 start{};
        ncl_math::Vector3 end{};
    };
    TraceFake* g_Trace;

    void SetupPrediction(int local, int players)
    {
        EXPECT_EQ(local, 0);
        EXPECT_EQ(players, 1);

        g_Trace->calls.push_back(1);
    }

    void PushPrediction()
    {
        g_Trace->calls.push_back(2);
    }

    void PopPrediction()
    {
        g_Trace->calls.push_back(5);
    }

    void SetHull(int hull)
    {
        g_Trace->movement->usehull = hull;
        g_Trace->calls.push_back(hull == 2 ? 3 : 6);
    }

    goldsrc::physent_t* GetPhysent(int index)
    {
        EXPECT_EQ(index, 7);
        EXPECT_EQ(g_Trace->movement->usehull, 2);

        return &g_Trace->hit;
    }

    void PlayerTrace(float* start, float* end, int flags, int ignore, goldsrc::pmtrace_t* out)
    {
        EXPECT_EQ(flags, goldsrc::PM_STUDIO_IGNORE);
        EXPECT_EQ(ignore, -1);
        EXPECT_EQ(g_Trace->movement->usehull, 2);

        g_Trace->start = ncl_math::Vector3::Ref(start);
        g_Trace->end = ncl_math::Vector3::Ref(end);
        g_Trace->calls.push_back(4);
        *out = g_Trace->trace;
    }

    class RadarLayerTrace : public testing::Test
    {
    protected:
        TraceFake fake_;
        void SetUp() override
        {
            g_Trace = &fake_;
            fake_.movement->usehull = 1;
            fake_.events.EV_SetUpPlayerPrediction = SetupPrediction;
            fake_.events.EV_PushPMStates = PushPrediction;
            fake_.events.EV_PopPMStates = PopPrediction;
            fake_.events.EV_SetTraceHull = SetHull;
            fake_.events.EV_PlayerTrace = PlayerTrace;
            fake_.events.EV_GetPhysent = GetPhysent;
            fake_.trace.fraction = .1f;
            fake_.trace.ent = 7;
            fake_.trace.plane.normal[2] = 1;
            fake_.trace.plane.dist = 256;
            fake_.trace.endpos[0] = 100;
            fake_.trace.endpos[1] = 100;
            fake_.trace.endpos[2] = 256.03125f;
            fake_.hit.model = &fake_.model;
        }

        void TearDown() override
        {
            g_Trace = nullptr;
        }

        std::optional<RadarLayerContact> Run(ncl_math::Vector3 feet)
        {
            return RadarLayerContext_TraceSupport(fake_.events, *fake_.movement, feet, fake_.bounds);
        }
    };

    TEST_F(RadarLayerTrace, PairsPredictionAndRestoresTraceHull)
    {
        ASSERT_TRUE(Run({100, 100, 256.03125f}));
        EXPECT_EQ(fake_.calls, (std::vector<int>{1, 2, 3, 4, 5, 6}));
        EXPECT_EQ(fake_.movement->usehull, 1);
        EXPECT_EQ(fake_.start, ncl_math::Vector3(100, 100, 258.03125f));
        EXPECT_EQ(fake_.end, ncl_math::Vector3(100, 100, 192.03125f));
    }

    TEST_F(RadarLayerTrace, CollisionEpsilonLandingConfirmsExactStationarySupport)
    {
        auto contact = Run({100, 100, 256.03125f});
        ASSERT_TRUE(contact);
        EXPECT_TRUE(contact->grounded);
        EXPECT_EQ(contact->support, ncl_math::Vector3(100, 100, 256));

        RadarMapLayerFixture fixture;
        fixture.AddFloor(0, {{0, 0}, {256, 256}});
        fixture.AddFloor(256, {{0, 0}, {256, 256}});

        RadarMapLayers map;
        ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

        std::string error;
        ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;

        RadarLayerContext context;
        context.Update(map, {{100, 100, 0}, {}, 1, 5, true, false}, 1, 3200);
        context.Update(map, {{100, 100, 280}, {}, 1.1f, 5, true, false}, 1.1f, 3200);

        ASSERT_EQ(context.get_view().active, 0);

        context.Update(map, {{100, 100, 256.03125f}, contact->support, 1.12f, 5, true, contact->grounded}, 1.12f, 3200);

        EXPECT_EQ(context.get_view().active, 1);
        EXPECT_EQ(context.get_selection().get_reason(), RadarLayerReason::Grounded);
    }

    TEST_F(RadarLayerTrace, AirborneNearSurfaceDoesNotBecomeGrounded)
    {
        auto contact = Run({100, 100, 257});
        ASSERT_TRUE(contact);
        EXPECT_FALSE(contact->grounded);
    }

    TEST_F(RadarLayerTrace, MovingPlatformUsesPhysentPlaneOffset)
    {
        fake_.trace.plane.dist = 16;
        fake_.hit.origin[2] = 240;
        auto contact = Run({100, 100, 256.03125f});

        ASSERT_TRUE(contact);
        EXPECT_TRUE(contact->grounded);
        EXPECT_EQ(contact->support.z, 256);
    }

    TEST_F(RadarLayerTrace, SlopedContactUsesPlayerHullCorner)
    {
        fake_.trace.plane.normal[0] = -.6f;
        fake_.trace.plane.normal[2] = .8f;
        fake_.trace.plane.dist = -60;
        fake_.trace.endpos[2] = .0390625f;
        auto contact = Run({100, 100, 12.0390625f});

        ASSERT_TRUE(contact);
        EXPECT_TRUE(contact->grounded);
        EXPECT_NEAR(contact->support.z, 0, .001f);
    }

    TEST_F(RadarLayerTrace, InvalidHitsRestorePredictionWithoutPublishingSupport)
    {
        for (int failure = 0; failure < 4; failure++)
        {
            fake_.trace.startsolid = failure == 0;
            fake_.trace.allsolid = failure == 1;
            fake_.trace.fraction = failure == 2 ? 1 : .1f;
            fake_.trace.plane.normal[2] = failure == 3 ? .6f : 1;
            fake_.calls.clear();

            EXPECT_FALSE(Run({100, 100, 256}));
            EXPECT_EQ(fake_.calls, (std::vector<int>{1, 2, 3, 4, 5, 6}));
            EXPECT_EQ(fake_.movement->usehull, 1);
        }
    }
} // namespace

TEST(RadarLayerContext, RepeatedSnapshotNoInterpolationFlagIsConsumedOnce)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    RadarLayerContext context;
    context.Update(map, {{10, 10, 0}, ncl_math::Vector3(10, 10, 0), 1, 5, true, true, false, true}, 1, 320);
    context.Update(map, {{10, 10, 0}, ncl_math::Vector3(10, 10, 0), 1.01f, 5, true, true, false, true}, 1, 320);

    EXPECT_EQ(context.get_selection().get_reason(), RadarLayerReason::Unchanged);
}

TEST(RadarLayerContext, MovingPlatformRetainsOrdinaryConfirmation)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0, {{0, 0}, {256, 256}});
    fixture.AddFloor(256, {{0, 0}, {256, 256}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    std::string error;
    ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, 128.0f}, {"upper", 128.0f, 512.0f}}, {}}, error)) << error;

    RadarLayerContext context;

    for (int step = 0; step <= 5; step++)
    {
        float height = (float)step * 30;
        float time = 1 + (float)step * .1f;
        context.Update(map, {{100, 100, height}, ncl_math::Vector3(100, 100, height), time, 1, true, true}, time, 320);
    }

    EXPECT_EQ(context.get_view().active, 0);
    EXPECT_EQ(context.get_selection().get_candidate(), 1);
    EXPECT_EQ(context.get_selection().get_reason(), RadarLayerReason::Pending);

    context.Update(map, {{100, 100, 180}, ncl_math::Vector3(100, 100, 180), 1.65f, 1, true, true}, 1.65f, 320);

    EXPECT_EQ(context.get_view().active, 1);
    EXPECT_EQ(context.get_selection().get_reason(), RadarLayerReason::Confirmed);
}
