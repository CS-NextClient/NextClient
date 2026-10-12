#include <gtest/gtest.h>

#include <span>

#include "hud/radar/markers/MarkerIdentity.h"

namespace
{
    constexpr MarkerIdentity::Params kParams{.match_tolerance = 48.0f, .break_strikes = 3};
} // namespace

TEST(MarkerIdentityPairing, ObservationOnAnEntityPairsIt)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{3, {104.0f, 100.0f}, 1}};

    identity.Update(entities, observations);

    EXPECT_EQ(identity.get_entity(3), 40);
    EXPECT_EQ(identity.get_index(40), 3);
}

TEST(MarkerIdentityPairing, NeighboursTakeTheirOwnEntity)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}, {41, {130.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{1, {100.0f, 100.0f}, 1}, {2, {130.0f, 100.0f}, 2}};

    identity.Update(entities, observations);

    EXPECT_EQ(identity.get_entity(1), 40);
    EXPECT_EQ(identity.get_entity(2), 41);
}

TEST(MarkerIdentityPairing, OneEntityAnswersForOneIndexOnly)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};

    // The second thing is hidden but inside the tolerance of the visible one.
    MarkerIdentity::Observation observations[] = {{1, {100.0f, 100.0f}, 1}, {2, {120.0f, 100.0f}, 2}};

    identity.Update(entities, observations);

    EXPECT_EQ(identity.get_entity(1), 40);
    EXPECT_EQ(identity.get_entity(2), -1);
}

TEST(MarkerIdentityPairing, ObservationFarFromEveryEntityDoesNotPair)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{3, {600.0f, 100.0f}, 1}};

    identity.Update(entities, observations);

    EXPECT_EQ(identity.get_entity(3), -1);
}

TEST(MarkerIdentityPairing, ObservationPairsWhenItsEntityEntersTheSnapshot)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{3, {100.0f, 100.0f}, 1}};

    identity.Update({}, observations);
    EXPECT_EQ(identity.get_entity(3), -1);

    identity.Update(entities, observations);
    EXPECT_EQ(identity.get_entity(3), 40);
    EXPECT_EQ(identity.get_index(40), 3);
}

TEST(MarkerIdentityPairing, RepeatedObservationDoesNotAccumulateBreakStrikes)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation paired[] = {{3, {100.0f, 100.0f}, 1}};
    MarkerIdentity::Observation elsewhere[] = {{3, {600.0f, 100.0f}, 2}};

    identity.Update(entities, paired);
    for (int frame = 0; frame < 10; frame++)
    {
        identity.Update(entities, elsewhere);
    }

    EXPECT_EQ(identity.get_entity(3), 40);
    EXPECT_EQ(identity.get_index(40), 3);
}

TEST(MarkerIdentityPairing, SustainedDisagreementDropsThePair)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation paired[] = {{3, {100.0f, 100.0f}, 1}};

    identity.Update(entities, paired);
    ASSERT_EQ(identity.get_entity(3), 40);

    for (std::uint32_t seq = 2; seq <= 4; seq++)
    {
        MarkerIdentity::Observation elsewhere[] = {{3, {600.0f, 100.0f}, seq}};
        identity.Update(entities, elsewhere);
    }

    EXPECT_EQ(identity.get_entity(3), -1);
}

TEST(MarkerIdentityPairing, ASingleDisagreementKeepsThePair)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation paired[] = {{3, {100.0f, 100.0f}, 1}};
    MarkerIdentity::Observation elsewhere[] = {{3, {600.0f, 100.0f}, 2}};

    identity.Update(entities, paired);
    identity.Update(entities, elsewhere);

    EXPECT_EQ(identity.get_entity(3), 40);
}

TEST(MarkerIdentityPairing, PairSurvivesTheEntityLeavingTheSnapshot)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    std::span<const MarkerIdentity::Entity> empty;

    MarkerIdentity::Observation paired[] = {{3, {100.0f, 100.0f}, 1}};
    identity.Update(entities, paired);
    ASSERT_EQ(identity.get_entity(3), 40);

    for (std::uint32_t seq = 2; seq <= 10; seq++)
    {
        MarkerIdentity::Observation away[] = {{3, {900.0f, 900.0f}, seq}};
        identity.Update(empty, away);
    }

    EXPECT_EQ(identity.get_entity(3), 40);
}

TEST(MarkerIdentityReset, ResetDropsThePairs)
{
    MarkerIdentity identity(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{3, {100.0f, 100.0f}, 1}};

    identity.Update(entities, observations);
    ASSERT_EQ(identity.get_entity(3), 40);

    identity.Reset();

    EXPECT_EQ(identity.get_entity(3), -1);
    EXPECT_EQ(identity.get_index(40), -1);
}

TEST(MarkerIdentity, SameXyDifferentFloorsDoNotPair)
{
    MarkerIdentity identity({.match_tolerance = 48.0f, .break_strikes = 3});
    MarkerIdentity::Entity entities[] = {{40, {64, 64}}};
    MarkerIdentity::Observation observations[] = {{3, {64, 64}, 1}};
    observations[0].pos = ncl_math::Vector3{64, 64, 256};
    identity.Update(entities, observations);

    EXPECT_EQ(identity.get_entity(3), -1);
}
