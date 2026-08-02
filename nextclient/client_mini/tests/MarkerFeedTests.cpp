#include <gtest/gtest.h>

#include <span>

#include "hud/radar/MarkerFeed.h"

namespace
{
    constexpr MarkerFeed::Params kParams{
        .identity = {.match_tolerance = 48.0f, .break_strikes = 3},
        .min_interval = 0.2f,
        .max_interval = 1.5f,
        .snap_dist = 600.0f,
    };

    constexpr MarkerFeed::Params kNamedParams{
        .identity = {.match_tolerance = 48.0f, .break_strikes = 3},
        .index_is_entity = true,
        .min_interval = 0.2f,
        .max_interval = 1.5f,
        .snap_dist = 600.0f,
    };

    std::span<const MarkerIdentity::Entity> NoEntities()
    {
        return {};
    }
} // namespace

TEST(MarkerFeedMerge, PairedThingIsReportedOnceFromTheSnapshot)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{3, {100.0f, 100.0f}, 1}};

    feed.Update(entities, observations, 10.0f);

    ASSERT_EQ(feed.get_markers().size(), 1u);
    EXPECT_EQ(feed.get_markers()[0].ent, 40);
    EXPECT_EQ(feed.get_markers()[0].index, 3);
    EXPECT_EQ(feed.get_markers()[0].pos, (ncl_math::Vector2{100.0f, 100.0f}));
}

TEST(MarkerFeedMerge, BroadcastTheSnapshotDoesNotCarryAddsAMarker)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{3, {100.0f, 100.0f}, 1}, {4, {900.0f, 100.0f}, 2}};

    feed.Update(entities, observations, 10.0f);

    ASSERT_EQ(feed.get_markers().size(), 2u);
    EXPECT_EQ(feed.get_markers()[1].ent, -1);
    EXPECT_EQ(feed.get_markers()[1].index, 4);
}

TEST(MarkerFeedMerge, APairedThingOutsideTheSnapshotComesBackFromTheBroadcast)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{3, {100.0f, 100.0f}, 1}};

    feed.Update(entities, observations, 10.0f);
    ASSERT_EQ(feed.get_entity(3), 40);

    feed.Update(NoEntities(), observations, 11.0f);

    ASSERT_EQ(feed.get_markers().size(), 1u);
    EXPECT_EQ(feed.get_markers()[0].ent, -1);
    EXPECT_EQ(feed.get_markers()[0].index, 3);
}

TEST(MarkerFeedMotion, BroadcastWalksBetweenReports)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Observation first[] = {{3, {0.0f, 0.0f}, 1}};
    MarkerIdentity::Observation second[] = {{3, {100.0f, 0.0f}, 2}};

    feed.Update(NoEntities(), first, 10.0f);
    ASSERT_EQ(feed.get_markers()[0].pos.x, 0.0f);

    feed.Update(NoEntities(), second, 11.0f); // one second later, so the segment lasts one second
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 0.0f);

    feed.Update(NoEntities(), second, 11.5f);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 50.0f);

    feed.Update(NoEntities(), second, 12.0f);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 100.0f);

    feed.Update(NoEntities(), second, 12.5f); // nothing new to walk toward
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 100.0f);
}

TEST(MarkerFeedMotion, AJumpBeyondReachIsPlacedNotWalked)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Observation first[] = {{3, {0.0f, 0.0f}, 1}};
    MarkerIdentity::Observation teleported[] = {{3, {5000.0f, 0.0f}, 2}};

    feed.Update(NoEntities(), first, 10.0f);
    feed.Update(NoEntities(), teleported, 11.0f);

    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 5000.0f);
}

TEST(MarkerFeedMotion, ResetMotionPlacesTheNextReport)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Observation first[] = {{3, {0.0f, 0.0f}, 1}};
    MarkerIdentity::Observation second[] = {{3, {100.0f, 0.0f}, 2}};

    feed.Update(NoEntities(), first, 10.0f);
    feed.ResetMotion();
    feed.Update(NoEntities(), second, 11.0f);

    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 100.0f);
}

TEST(MarkerFeedNamedIndex, IndexIsEntitySkipsPairing)
{
    MarkerFeed feed(kNamedParams);

    MarkerIdentity::Entity entities[] = {{5, {100.0f, 100.0f}}};

    // Both far from the entity: without pairing, index 5 is still the entity's own marker.
    MarkerIdentity::Observation observations[] = {{5, {900.0f, 900.0f}, 1}, {6, {800.0f, 800.0f}, 2}};

    feed.Update(entities, observations, 10.0f);

    EXPECT_EQ(feed.get_entity(6), 6);
    ASSERT_EQ(feed.get_markers().size(), 2u);
    EXPECT_EQ(feed.get_markers()[0].ent, 5);
    EXPECT_EQ(feed.get_markers()[0].index, 5);
    EXPECT_EQ(feed.get_markers()[1].index, 6);
}
