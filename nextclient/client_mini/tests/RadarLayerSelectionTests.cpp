#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "RadarMapLayerFixtures.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/map/layers/RadarMapLayers.h"

namespace
{
    RadarMapLayers StackedMap(float upper_height = 256.0f)
    {
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
        fixture.AddFloor(upper_height, {{0.0f, 0.0f}, {256.0f, 256.0f}});

        RadarMapLayers map;
        EXPECT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

        std::string error;
        EXPECT_TRUE(map.ApplyOverrides({{{"lower", -128.0f, upper_height * 0.5f}, {"upper", upper_height * 0.5f, 512.0f}}, {}}, error))
            << error;
        EXPECT_EQ(map.get_layers().size(), 2u);

        return map;
    }

    RadarLayerObservation Grounded(float height, float time = 0.0f)
    {
        return {{64.0f, 64.0f, height}, ncl_math::Vector3{64.0f, 64.0f, height}, time, 1, true, true, false, false};
    }
} // namespace

TEST(RadarLayerSelection, ConfirmsAfterPoint15Seconds)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation = Grounded(0.0f);

    ASSERT_EQ(selection.Update(map, observation), 0);

    observation = Grounded(144.0f, 0.5f);
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), -1);

    observation = Grounded(145.0f, 1.0f);
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), 1);
    EXPECT_FLOAT_EQ(selection.get_candidate_since(), 1.0f);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Pending);

    observation.time = 1.149f;
    EXPECT_EQ(selection.Update(map, observation), 0);

    observation.time = 1.151f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), -1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Confirmed);
}

TEST(RadarLayerSelection, ExactConfirmationThreshold)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(145.0f);
    EXPECT_EQ(selection.Update(map, observation), 0);

    observation.time = 0.15f;
    EXPECT_EQ(selection.Update(map, observation), 1);
}

TEST(RadarLayerSelection, MarginCapsAtQuarterGap)
{
    RadarMapLayers map = StackedMap(48.0f);
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(36.0f, 1.0f);
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), -1);

    observation = Grounded(37.0f, 1.1f);
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), 1);

    observation.time = 1.251f;
    EXPECT_EQ(selection.Update(map, observation), 1);
}

TEST(RadarLayerSelection, JumpKeepsFloor)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(200.0f, 1.0f);
    observation.grounded = false;
    observation.support.reset();
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Airborne);

    observation.time = 1.3f;
    observation.feet.z = 145.0f;
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), -1);
}

TEST(RadarLayerSelection, VerticalTeleportSelectsImmediately)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 0.01f);
    observation.discontinuity = true;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
    EXPECT_EQ(selection.get_candidate(), -1);
}

TEST(RadarLayerSelection, TargetChangeCancelsPendingSwitch)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);
    ASSERT_EQ(selection.get_candidate(), 1);

    EXPECT_EQ(selection.Update(map, Grounded(130.0f, 1.1f)), 0);
    EXPECT_EQ(selection.get_candidate(), -1);

    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.2f)), 0);
    EXPECT_FLOAT_EQ(selection.get_candidate_since(), 1.2f);

    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.349f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.351f)), 1);
}

TEST(RadarLayerSelection, FreeCameraOutsideBounds)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation{{400.0f, 64.0f, 256.0f}, {}, 0.0f, 2, true, false, true, false};

    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Nearest);

    observation.feet.z = 128.0f;
    observation.time = 1.0f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), -1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Nearest);

    observation.feet.z = 111.0f;
    observation.time = 2.0f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), 0);

    observation.time = 2.151f;
    EXPECT_EQ(selection.Update(map, observation), 0);
}

TEST(RadarLayerSelection, FreeCameraUsesSupportBelowWithinProjection)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation{{64.0f, 64.0f, 200.0f}, {}, 0.0f, 2, true, false, true, false};

    ASSERT_EQ(selection.Update(map, observation), 0);

    observation.feet.z = 300.0f;
    observation.time = 1.0f;
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), 1);

    observation.time = 1.151f;
    EXPECT_EQ(selection.Update(map, observation), 1);
}

TEST(RadarLayerSelection, LostSupportHoldsSelection)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.2f);
    observation.support.reset();
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Invalid);
    EXPECT_EQ(selection.get_candidate(), -1);

    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.3f)), 0);
    EXPECT_FLOAT_EQ(selection.get_candidate_since(), 1.3f);

    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.451f)), 1);
}

TEST(RadarLayerSelection, FallReachesLowerFloor)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(256.0f)), 1);

    RadarLayerObservation observation = Grounded(113.0f, 1.0f);
    observation.grounded = false;
    observation.support.reset();
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), -1);

    observation.feet.z = 112.0f;
    observation.time = 1.1f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), -1);

    observation.feet.z = 111.0f;
    observation.time = 1.2f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), 0);

    observation.feet.z = 80.0f;
    observation.time = 1.349f;
    EXPECT_EQ(selection.Update(map, observation), 1);

    observation.time = 1.351f;
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Confirmed);
}

TEST(RadarLayerSelection, SameHeightRegionTransition)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerOverrides overrides;
    overrides.layers = {{"left", -64.0f, 64.0f}, {"right", 64.0f, 128.0f}};
    overrides.regions.push_back({"right", {{128.0f, 0.0f, -64.0f}, {256.0f, 256.0f, 64.0f}}});
    std::string error;

    ASSERT_TRUE(map.ApplyOverrides(overrides, error)) << error;

    int left = -1;
    int right = -1;

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        if (layer.name == "left")
        {
            left = layer.id;
        }

        if (layer.name == "right")
        {
            right = layer.id;
        }
    }

    ASSERT_GE(left, 0);
    ASSERT_GE(right, 0);
    ASSERT_FLOAT_EQ(map.get_layers()[left].height, 0.0f);
    ASSERT_FLOAT_EQ(map.get_layers()[right].height, 0.0f);

    RadarLayerSelection selection;
    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), left);

    RadarLayerObservation observation = Grounded(0.0f, 1.0f);
    observation.feet.x = 128.0f;
    observation.support->x = 128.0f;
    EXPECT_EQ(selection.Update(map, observation), left);
    EXPECT_EQ(selection.get_candidate(), right);

    observation.time = 1.149f;
    EXPECT_EQ(selection.Update(map, observation), left);

    observation.time = 1.151f;
    EXPECT_EQ(selection.Update(map, observation), right);

    observation.feet.x = 127.99f;
    observation.support->x = 127.99f;
    observation.time = 2.0f;
    EXPECT_EQ(selection.Update(map, observation), right);
    EXPECT_EQ(selection.get_candidate(), left);
}

TEST(RadarLayerSelection, LandingOnOtherSupportSelectsImmediately)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.0f);
    observation.grounded = false;
    observation.support.reset();
    EXPECT_EQ(selection.Update(map, observation), 0);

    observation = Grounded(256.0f, 1.01f);
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Grounded);
    EXPECT_EQ(selection.get_candidate(), -1);
}

TEST(RadarLayerSelection, LandingOnMovingPlatformStillConfirms)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(145.0f, 1.0f);
    observation.grounded = false;
    observation.support.reset();
    EXPECT_EQ(selection.Update(map, observation), 0);

    observation = Grounded(145.0f, 1.01f);
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), 1);

    observation.time = 1.161f;
    EXPECT_EQ(selection.Update(map, observation), 1);
}

TEST(RadarLayerSelection, GroundedRampKeepsConfirmation)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    RadarLayerSelection selection;
    RadarLayerObservation observation = Grounded(0.0f);
    observation.feet.x = -64.0f;
    observation.feet.y = 0.0f;
    observation.support = observation.feet;
    ASSERT_EQ(selection.Update(map, observation), 0);

    observation.feet = {280.0f, 0.0f, 140.0f};
    observation.support = observation.feet;
    observation.time = 0.5f;
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), -1);

    observation.feet = {300.0f, 0.0f, 150.0f};
    observation.support = observation.feet;
    observation.time = 1.0f;
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), 1);

    observation.time = 1.151f;
    EXPECT_EQ(selection.Update(map, observation), 1);
}

TEST(RadarLayerSelection, SourceChangeCancelsPendingSwitch)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.01f);
    observation.source = 2;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), -1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
}

TEST(RadarLayerSelection, TimeRollbackCancelsPendingSwitch)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(256.0f, 0.5f)), 1);
    EXPECT_EQ(selection.get_candidate(), -1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
}

TEST(RadarLayerSelection, InvalidObservationHoldsAndCancelsCandidate)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.2f);
    observation.valid = false;
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_active(), 0);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Invalid);
    EXPECT_EQ(selection.get_candidate(), -1);
}

TEST(RadarLayerSelection, NextValidObservationInitializesAfterInvalidInput)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation = Grounded(256.0f);
    observation.valid = false;
    EXPECT_EQ(selection.Update(map, observation), -1);

    observation.valid = true;
    observation.support.reset();
    observation.time = 1.0f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Initial);
}

TEST(RadarLayerSelection, InitialPositionOutsideProjectionUsesNearest)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation{{400.0f, 64.0f, 256.0f}, {}, 0.0f, 1, true, false, false, false};

    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Nearest);
}

TEST(RadarLayerSelection, CrouchingUsesNormalizedFeet)
{
    RadarMapLayers map = StackedMap(48.0f);
    RadarLayerSelection selection;
    RadarLayerObservation observation = Grounded(0.0f);
    observation.feet = RadarLayerContext_Feet({64.0f, 64.0f, 36.0f}, {-16.0f, -16.0f, -36.0f});
    ASSERT_EQ(observation.feet, (ncl_math::Vector3{64.0f, 64.0f, 0.0f}));

    ASSERT_EQ(selection.Update(map, observation), 0);

    observation.feet = RadarLayerContext_Feet({64.0f, 64.0f, 18.0f}, {-16.0f, -16.0f, -18.0f});
    ASSERT_EQ(observation.feet, (ncl_math::Vector3{64.0f, 64.0f, 0.0f}));
    observation.time = 1.0f;
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), -1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Unchanged);
}

TEST(RadarLayerSelection, ResetClearsPendingStateAndInitializesImmediately)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);
    ASSERT_EQ(selection.get_candidate(), 1);

    selection.Reset();

    EXPECT_EQ(selection.get_active(), -1);
    EXPECT_EQ(selection.get_candidate(), -1);
    EXPECT_FLOAT_EQ(selection.get_candidate_since(), 0.0f);

    EXPECT_EQ(selection.Update(map, Grounded(256.0f, 0.01f)), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Initial);
}

TEST(RadarLayerSelection, InvalidAirborneObservationDoesNotConfirmLanding)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.0f);
    observation.valid = false;
    observation.grounded = false;
    observation.support.reset();
    EXPECT_EQ(selection.Update(map, observation), 0);

    observation = Grounded(256.0f, 1.1f);
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Pending);

    observation.time = 1.251f;
    EXPECT_EQ(selection.Update(map, observation), 1);
}

TEST(RadarLayerSelection, AirborneOutsideProjectionHoldsSelection)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(256.0f)), 1);

    RadarLayerObservation observation{{400.0f, 64.0f, 0.0f}, {}, 1.0f, 1, true, false, false, false};
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), -1);

    observation.time = 1.3f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Airborne);
}

TEST(RadarLayerSelection, FreeCameraNearestTieUsesLowestIdInitially)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;
    RadarLayerObservation observation{{400.0f, 64.0f, 128.0f}, {}, 0.0f, 2, true, false, true, false};

    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Nearest);
}

TEST(RadarLayerSelection, InvalidDiscontinuitySelectsAfterRecovery)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 1.1f);
    observation.valid = false;
    observation.discontinuity = true;
    EXPECT_EQ(selection.Update(map, observation), 0);
    EXPECT_EQ(selection.get_candidate(), -1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Invalid);

    observation.valid = true;
    observation.discontinuity = false;
    observation.time = 1.2f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
}

TEST(RadarLayerSelection, InvalidTimeRollbackSelectsAfterRecovery)
{
    RadarMapLayers map = StackedMap();
    RadarLayerSelection selection;

    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(145.0f, 1.0f)), 0);

    RadarLayerObservation observation = Grounded(256.0f, 0.5f);
    observation.valid = false;
    EXPECT_EQ(selection.Update(map, observation), 0);

    observation.valid = true;
    observation.time = 1.2f;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_reason(), RadarLayerReason::Discontinuity);
}


TEST(RadarLayerSelection, DistantStacksUseLocalSupportForConfirmation)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(2048.0f, {{2048.0f, 0.0f}, {2560.0f, 512.0f}});
    fixture.AddFloor(2304.0f, {{2048.0f, 0.0f}, {2560.0f, 512.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);

    RadarLayerSelection selection;
    ASSERT_EQ(selection.Update(map, Grounded(0.0f)), 0);
    EXPECT_EQ(selection.Update(map, Grounded(256.0f, 1.0f)), 0);
    EXPECT_EQ(selection.get_candidate(), 1);

    EXPECT_EQ(selection.Update(map, Grounded(256.0f, 1.151f)), 1);

    RadarLayerObservation observation = Grounded(2304.0f, 2.0f);
    observation.feet.x = 2112.0f;
    observation.support = observation.feet;
    observation.discontinuity = true;
    ASSERT_EQ(selection.Update(map, observation), 1);

    observation.discontinuity = false;
    observation.time = 3.0f;
    observation.feet.z = 2048.0f;
    observation.support = observation.feet;
    EXPECT_EQ(selection.Update(map, observation), 1);
    EXPECT_EQ(selection.get_candidate(), 0);

    observation.time = 3.151f;
    EXPECT_EQ(selection.Update(map, observation), 0);
}


TEST(RadarLayerSelection, DistantRampsWithNearbyHeightsKeepLocalTransition)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{-256.0f, -128.0f}, {0.0f, 128.0f}});
    fixture.AddFloor(256.0f, {{512.0f, -128.0f}, {768.0f, 128.0f}});
    fixture.AddFloor(64.0f, {{1792.0f, -128.0f}, {2048.0f, 128.0f}});
    fixture.AddFloor(320.0f, {{2560.0f, -128.0f}, {2816.0f, 128.0f}});
    fixture.AddRamp({0.0f, 0.0f, 0.0f}, {512.0f, 0.0f, 256.0f}, 64.0f);
    fixture.AddRamp({2048.0f, 0.0f, 64.0f}, {2560.0f, 0.0f, 320.0f}, 64.0f);
    fixture.AddOverlapRoom(0, 1);
    fixture.AddOverlapRoom(2, 3);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    ASSERT_EQ(map.get_layers().size(), 2u);
    ASSERT_EQ(map.get_analysis().transitions.size(), 2u);

    for (int ramp = 0; ramp < 2; ramp++)
    {
        for (bool rising : {true, false})
        {
            SCOPED_TRACE(ramp);
            SCOPED_TRACE(rising);
            float x_offset = (float)ramp * 2048.0f;
            float z_offset = (float)ramp * 64.0f;
            auto observation = [&](float x, float time) {
                RadarLayerObservation result = Grounded(z_offset + x * 0.5f, time);
                result.feet.x = x_offset + x;
                result.feet.y = 0.0f;
                result.support = result.feet;

                return result;
            };
            int initial = rising ? 0 : 1;
            int target = rising ? 1 : 0;
            RadarLayerSelection selection;

            ASSERT_EQ(selection.Update(map, observation(rising ? 200.0f : 312.0f, 0.0f)), initial);
            EXPECT_EQ(selection.Update(map, observation(rising ? 290.0f : 222.0f, 1.0f)), initial);
            EXPECT_EQ(selection.get_candidate(), target);

            EXPECT_EQ(selection.Update(map, observation(rising ? 322.0f : 190.0f, 1.14f)), initial);
            EXPECT_EQ(selection.get_candidate(), target);
            EXPECT_FLOAT_EQ(selection.get_candidate_since(), 1.0f);

            EXPECT_EQ(selection.Update(map, observation(rising ? 386.0f : 126.0f, 1.151f)), target);
            EXPECT_EQ(selection.get_reason(), RadarLayerReason::Confirmed);
        }
    }
}
