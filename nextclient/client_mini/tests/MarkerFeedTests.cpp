#include <gtest/gtest.h>

#include <span>

#include "hud/radar/markers/MarkerFeed.h"
#include "hud/radar/layers/players/PlayerTracks.h"

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
    EXPECT_EQ(feed.get_markers()[0].pos, (ncl_math::Vector3{100.0f, 100.0f}));
}

TEST(MarkerFeedMerge, LateSnapshotEntityDoesNotDuplicateAStationaryBroadcast)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Entity entities[] = {{40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation observations[] = {{3, {100.0f, 100.0f}, 1}};

    feed.Update(NoEntities(), observations, 10.0f);
    ASSERT_EQ(feed.get_markers().size(), 1u);
    EXPECT_EQ(feed.get_markers()[0].ent, -1);

    for (int frame = 1; frame <= 10; frame++)
    {
        feed.Update(entities, observations, 10.0f + frame * 0.1f);
        ASSERT_EQ(feed.get_markers().size(), 1u);
        EXPECT_EQ(feed.get_markers()[0].ent, 40);
        EXPECT_EQ(feed.get_markers()[0].index, 3);
        EXPECT_EQ(feed.get_markers()[0].pos, (ncl_math::Vector3{100.0f, 100.0f}));
    }
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

TEST(MarkerFeedMotion, AThingLeavingTheSnapshotHoldsWhereTheSnapshotLastShowedIt)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Observation report[] = {{3, {0.0f, 0.0f}, 1}};
    MarkerIdentity::Entity at_report[] = {{40, {0.0f, 0.0f}}};
    MarkerIdentity::Entity walked_on[] = {{40, {100.0f, 0.0f}}};

    feed.Update(at_report, report, 10.0f);
    ASSERT_EQ(feed.get_entity(3), 40);

    feed.Update(walked_on, report, 10.5f);

    // The only report is half a second older than the position the snapshot last drew.
    feed.Update(NoEntities(), report, 10.6f);

    ASSERT_EQ(feed.get_markers().size(), 1u);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 100.0f);
}

TEST(MarkerFeedMotion, TheReportAfterLeavingWalksOnFromTheHeldPosition)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Observation first[] = {{3, {0.0f, 0.0f}, 1}};
    MarkerIdentity::Observation next[] = {{3, {200.0f, 0.0f}, 2}};
    MarkerIdentity::Entity at_report[] = {{40, {0.0f, 0.0f}}};
    MarkerIdentity::Entity walked_on[] = {{40, {100.0f, 0.0f}}};

    feed.Update(at_report, first, 10.0f);
    feed.Update(walked_on, first, 10.5f);
    feed.Update(NoEntities(), first, 10.6f);

    feed.Update(NoEntities(), next, 11.0f); // one second after the previous report
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 100.0f);

    feed.Update(NoEntities(), next, 11.5f);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 150.0f);
}

TEST(MarkerFeedMotion, AReportTakenWhileCarriedDoesNotPullTheMarkerBack)
{
    MarkerFeed feed(kParams);

    MarkerIdentity::Observation first[] = {{3, {0.0f, 0.0f}, 1}};
    MarkerIdentity::Observation lagging[] = {{3, {80.0f, 0.0f}, 2}};
    MarkerIdentity::Entity at_report[] = {{40, {0.0f, 0.0f}}};
    MarkerIdentity::Entity ahead[] = {{40, {100.0f, 0.0f}}};

    feed.Update(at_report, first, 10.0f);
    feed.Update(ahead, lagging, 11.0f);

    feed.Update(NoEntities(), lagging, 11.2f);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 100.0f);

    feed.Update(NoEntities(), lagging, 12.0f);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 100.0f);
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

TEST(MarkerFeedNamedIndex, LeavingTheSnapshotKeepsTheTrailWhileBroadcastPositionsInterpolate)
{
    MarkerFeed feed(kNamedParams);
    PlayerTracks tracks;
    MarkerIdentity::Entity entity[] = {{2, {100.0f, 100.0f}}};
    MarkerIdentity::Observation report[] = {{2, {100.0f, 100.0f}, 1}};
    feed.Update(entity, report, 10.0f);
    tracks.Sample(2, feed.get_markers()[0].pos, -1, 10.0f);
    entity[0].pos.x = 200.0f;
    feed.Update(entity, report, 10.5f);
    tracks.Sample(2, feed.get_markers()[0].pos, -1, 10.5f);

    feed.Update({}, report, 10.7f);
    ASSERT_EQ(feed.get_markers().size(), 1u);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 200.0f);
    tracks.Sample(2, feed.get_markers()[0].pos, -1, 10.7f);
    report[0] = {2, {300.0f, 100.0f}, 2};
    feed.Update({}, report, 11.0f);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 200.0f);
    feed.Update({}, report, 11.5f);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 250.0f);
    tracks.Sample(2, feed.get_markers()[0].pos, -1, 11.5f);

    PlayerTrackPoint points[PlayerTracks::kMaxPoints];
    ASSERT_EQ(tracks.CopyPoints(2, points), 3);
    EXPECT_EQ(points[0].pos, (ncl_math::Vector3{100.0f, 100.0f}));
    EXPECT_EQ(points[2].pos, (ncl_math::Vector3{250.0f, 100.0f}));
}

TEST(MarkerFeedNamedIndex, ResettingOnePlayerDropsItsMotionForSlotReuse)
{
    MarkerFeed feed(kNamedParams);
    MarkerIdentity::Entity entity[] = {{2, {100.0f, 100.0f}}};
    MarkerIdentity::Observation report[] = {{2, {50.0f, 100.0f}, 1}};
    feed.Update(entity, report, 10.0f);
    feed.ResetMotion(2);
    report[0] = {2, {200.0f, 100.0f}, 2};
    feed.Update({}, report, 11.0f);

    ASSERT_EQ(feed.get_markers().size(), 1u);
    EXPECT_EQ(feed.get_markers()[0].pos, (ncl_math::Vector3{200.0f, 100.0f}));
}


class MarkerFeedSnapshotReturn : public testing::TestWithParam<bool>
{
protected:
    MarkerFeed feed_{GetParam() ? kNamedParams : kParams};
    MarkerIdentity::Entity entities_[1] = {{GetParam() ? 3 : 40, {100.0f, 100.0f}}};
    MarkerIdentity::Observation reports_[1] = {{3, {100.0f, 100.0f}, 1}};

    void SetUp() override
    {
        feed_.Update(entities_, reports_, 10.0f);

        ASSERT_EQ(feed_.get_entity(3), entities_[0].ent);

        feed_.Update({}, reports_, 10.5f);
        reports_[0] = {3, {200.0f, 100.0f}, 2, {100.0f, 100.0f}, 1.0f};
        feed_.Update({}, reports_, 11.0f);
        feed_.Update({}, reports_, 11.5f);

        ASSERT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 150.0f);
    }

    void ReturnToSnapshot()
    {
        entities_[0].pos.x = 300.0f;
        feed_.Update(entities_, reports_, 12.0f);
    }
};

TEST_P(MarkerFeedSnapshotReturn, CorrectsAtLimitedSpeedAndThenTracksWithoutDelay)
{
    ReturnToSnapshot();

    ASSERT_EQ(feed_.get_markers().size(), 1u);
    EXPECT_EQ(feed_.get_markers()[0].ent, entities_[0].ent);
    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 150.0f);

    entities_[0].pos.x = 320.0f;
    feed_.Update(entities_, reports_, 12.5f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 220.0f, 0.001f);

    entities_[0].pos.x = 340.0f;
    feed_.Update(entities_, reports_, 13.0f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 290.0f, 0.001f);

    entities_[0].pos.x = 360.0f;
    feed_.Update(entities_, reports_, 13.5f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 360.0f);

    entities_[0].pos.x = 370.0f;
    feed_.Update(entities_, reports_, 13.6f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 370.0f);
}

TEST_P(MarkerFeedSnapshotReturn, LargerGapsTakeLongerAtTheSameCorrectionSpeed)
{
    MarkerFeed nearby_feed = feed_;
    MarkerIdentity::Entity nearby[] = {{entities_[0].ent, {250.0f, 100.0f}}};
    MarkerIdentity::Entity distant[] = {{entities_[0].ent, {550.0f, 100.0f}}};
    nearby_feed.Update(nearby, reports_, 12.0f);
    feed_.Update(distant, reports_, 12.0f);
    nearby_feed.Update(nearby, reports_, 12.5f);
    feed_.Update(distant, reports_, 12.5f);

    EXPECT_NEAR(nearby_feed.get_markers()[0].pos.x, 200.0f, 0.001f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 200.0f, 0.001f);

    nearby_feed.Update(nearby, reports_, 13.0f);
    feed_.Update(distant, reports_, 13.0f);

    EXPECT_FLOAT_EQ(nearby_feed.get_markers()[0].pos.x, 250.0f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 250.0f, 0.001f);

    feed_.Update(distant, reports_, 16.0f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 550.0f);
}

TEST_P(MarkerFeedSnapshotReturn, MovingTargetCorrectionDoesNotDependOnFrameRate)
{
    for (int fps : {30, 60, 144})
    {
        SCOPED_TRACE(fps);
        MarkerFeed feed = feed_;
        MarkerIdentity::Entity entities[] = {{entities_[0].ent, {450.0f, 100.0f}}};
        feed.Update(entities, reports_, 12.0f);

        ASSERT_FLOAT_EQ(feed.get_markers()[0].pos.x, 150.0f);

        for (int frame = 1; frame <= fps; frame++)
        {
            float elapsed = (float)frame / (float)fps;
            entities[0].pos.x = 450.0f + 250.0f * elapsed;
            feed.Update(entities, reports_, 12.0f + elapsed);
        }

        EXPECT_NEAR(feed.get_markers()[0].pos.x, 500.0f, 0.001f);
    }
}

TEST_P(MarkerFeedSnapshotReturn, DiagonalCorrectionUsesWorldDistance)
{
    entities_[0].pos = {390.0f, 420.0f};
    feed_.Update(entities_, reports_, 12.0f);
    feed_.Update(entities_, reports_, 12.5f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 180.0f, 0.001f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.y, 140.0f, 0.001f);

    feed_.Update(entities_, reports_, 16.0f);

    EXPECT_EQ(feed_.get_markers()[0].pos, entities_[0].pos);
}

TEST_P(MarkerFeedSnapshotReturn, SmallGapDoesNotOvershoot)
{
    entities_[0].pos.x = 155.0f;
    feed_.Update(entities_, reports_, 12.0f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 150.0f);

    feed_.Update(entities_, reports_, 12.1f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 155.0f);

    entities_[0].pos.x = 175.0f;
    feed_.Update(entities_, reports_, 12.2f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 175.0f);
}

TEST_P(MarkerFeedSnapshotReturn, LeavingDuringBlendHoldsDrawnPositionUntilNextReport)
{
    ReturnToSnapshot();
    entities_[0].pos.x = 320.0f;
    feed_.Update(entities_, reports_, 12.35f);
    feed_.Update({}, reports_, 12.36f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 205.0f, 0.001f);

    reports_[0] = {3, {400.0f, 100.0f}, 3, {200.0f, 100.0f}, 2.0f};
    feed_.Update({}, reports_, 13.0f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 205.0f, 0.001f);

    feed_.Update({}, reports_, 13.75f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 302.5f, 0.001f);
}

TEST_P(MarkerFeedSnapshotReturn, RepeatedReturnStartsAtLastDrawnPosition)
{
    ReturnToSnapshot();
    entities_[0].pos.x = 320.0f;
    feed_.Update(entities_, reports_, 12.35f);
    feed_.Update({}, reports_, 12.36f);
    entities_[0].pos.x = 330.0f;
    feed_.Update(entities_, reports_, 12.4f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 205.0f, 0.001f);

    entities_[0].pos.x = 340.0f;
    feed_.Update(entities_, reports_, 12.75f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 250.0f, 0.001f);

    entities_[0].pos.x = 350.0f;
    feed_.Update(entities_, reports_, 13.11f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 296.0f, 0.001f);

    entities_[0].pos.x = 360.0f;
    feed_.Update(entities_, reports_, 13.7f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 360.0f);
}

TEST_P(MarkerFeedSnapshotReturn, NewReportChangesSpeedWithoutMovingAlreadyDrawnPosition)
{
    ReturnToSnapshot();
    reports_[0] = {3, {310.0f, 100.0f}, 3, {200.0f, 100.0f}, 1.35f};
    entities_[0].pos.x = 320.0f;
    feed_.Update(entities_, reports_, 12.35f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 205.0f, 0.001f);

    feed_.Update(entities_, reports_, 12.35f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 205.0f, 0.001f);

    entities_[0].pos.x = 340.0f;
    feed_.Update(entities_, reports_, 12.75f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 257.5926f, 0.001f);

    entities_[0].pos.x = 360.0f;
    feed_.Update(entities_, reports_, 14.0f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 360.0f);
}

TEST_P(MarkerFeedSnapshotReturn, LeavingAndReturningDuringBothInterpolationsKeepsPositionContinuous)
{
    ReturnToSnapshot();
    entities_[0].pos.x = 320.0f;
    feed_.Update(entities_, reports_, 12.35f);
    ASSERT_NEAR(feed_.get_markers()[0].pos.x, 205.0f, 0.001f);

    reports_[0] = {3, {400.0f, 100.0f}, 3, {200.0f, 100.0f}, 1.5f};
    feed_.Update({}, reports_, 12.5f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 205.0f, 0.001f);
    feed_.Update({}, reports_, 12.8f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 244.0f, 0.001f);

    reports_[0] = {3, {400.0f, 100.0f}, 4, {400.0f, 100.0f}, 0.4f};
    entities_[0].pos.x = 400.0f;
    feed_.Update(entities_, reports_, 12.9f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 244.0f, 0.001f);
    entities_[0].pos.x = 420.0f;
    feed_.Update(entities_, reports_, 13.25f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 351.5f, 0.001f);

    feed_.Update({}, reports_, 13.3f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 351.5f, 0.001f);
    entities_[0].pos.x = 440.0f;
    feed_.Update(entities_, reports_, 13.4f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 351.5f, 0.001f);
    entities_[0].pos.x = 460.0f;
    feed_.Update(entities_, reports_, 13.75f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 459.0f, 0.001f);
    entities_[0].pos.x = 480.0f;
    feed_.Update(entities_, reports_, 14.15f);
    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 480.0f, 0.001f);
    entities_[0].pos.x = 500.0f;
    feed_.Update(entities_, reports_, 14.85f);
    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 500.0f);
}

TEST_P(MarkerFeedSnapshotReturn, MultiplePvsChangesAtSameTimeKeepLastDrawnPosition)
{
    ReturnToSnapshot();
    feed_.Update({}, reports_, 12.0f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 150.0f);

    entities_[0].pos.x = 320.0f;
    feed_.Update(entities_, reports_, 12.0f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 150.0f);

    reports_[0] = {3, {400.0f, 100.0f}, 3, {200.0f, 100.0f}, 1.0f};
    feed_.Update({}, reports_, 12.0f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 150.0f);

    entities_[0].pos.x = 330.0f;
    feed_.Update(entities_, reports_, 12.0f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 150.0f);

    entities_[0].pos.x = 350.0f;
    feed_.Update(entities_, reports_, 12.35f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 240.0f, 0.001f);

    entities_[0].pos.x = 370.0f;
    feed_.Update(entities_, reports_, 12.75f);

    EXPECT_NEAR(feed_.get_markers()[0].pos.x, 340.0f, 0.001f);

    entities_[0].pos.x = 390.0f;
    feed_.Update(entities_, reports_, 13.85f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 390.0f);
}

TEST_P(MarkerFeedSnapshotReturn, UnusableReportPairsUseDefaultPlayerSpeed)
{
    MarkerIdentity::Observation reports[] = {
        {3, {200.0f, 100.0f}, 2},
        {3, {200.0f, 100.0f}, 2, {100.0f, 100.0f}, -1.0f},
        {3, {200.0f, 100.0f}, 2, {200.0f, 100.0f}, 1.0f},
        {3, {200.0f, 100.0f}, 2, {5000.0f, 100.0f}, 1.0f},
    };
    entities_[0].pos.x = 300.0f;

    for (const MarkerIdentity::Observation& report : reports)
    {
        MarkerFeed feed = feed_;
        feed.Update(entities_, {&report, 1}, 12.0f);

        EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 150.0f);

        feed.Update(entities_, {&report, 1}, 12.5f);

        EXPECT_NEAR(feed.get_markers()[0].pos.x, 275.0f, 0.001f);

        feed.Update(entities_, {&report, 1}, 12.7f);

        EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 300.0f);
    }
}

TEST_P(MarkerFeedSnapshotReturn, ReturnBeyondSnapDistanceIsImmediate)
{
    entities_[0].pos.x = 751.0f;
    feed_.Update(entities_, reports_, 12.0f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 751.0f);

    entities_[0].pos.x = 770.0f;
    feed_.Update(entities_, reports_, 12.1f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 770.0f);
}

TEST_P(MarkerFeedSnapshotReturn, TeleportDuringBlendCancelsCorrection)
{
    ReturnToSnapshot();
    entities_[0].pos.x = 2000.0f;
    feed_.Update(entities_, reports_, 12.1f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 2000.0f);

    entities_[0].pos.x = 2020.0f;
    feed_.Update(entities_, reports_, 12.15f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 2020.0f);
}

TEST_P(MarkerFeedSnapshotReturn, ResetMotionDropsReturnBlend)
{
    ReturnToSnapshot();
    feed_.ResetMotion(3);
    entities_[0].pos.x = 320.0f;
    feed_.Update(entities_, reports_, 12.1f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 320.0f);
}

TEST_P(MarkerFeedSnapshotReturn, BackwardsTimeDropsActiveCorrectionWithoutLosingIdentity)
{
    ReturnToSnapshot();
    entities_[0].pos.x = 320.0f;
    feed_.Update(entities_, reports_, 12.1f);
    feed_.Update(entities_, reports_, 12.05f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 320.0f);
    EXPECT_EQ(feed_.get_entity(3), entities_[0].ent);

    feed_.Update({}, reports_, 12.06f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 320.0f);
}

TEST_P(MarkerFeedSnapshotReturn, BackwardsTimeCannotReplayCompletedCorrection)
{
    ReturnToSnapshot();
    feed_.Update(entities_, reports_, 14.0f);

    ASSERT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 300.0f);

    feed_.Update(entities_, reports_, 12.1f);

    EXPECT_FLOAT_EQ(feed_.get_markers()[0].pos.x, 300.0f);
}

INSTANTIATE_TEST_SUITE_P(PlayersAndHostages, MarkerFeedSnapshotReturn, testing::Bool());

TEST(MarkerFeedMotion, InterpolatesZWithXy)
{
    MarkerFeed feed(kParams);
    MarkerIdentity::Observation report[] = {{3, {0, 0}, 1}};
    feed.Update({}, report, 10.0f);
    report[0].pos = ncl_math::Vector3{100, 100, 64};
    report[0].seq = 2;
    feed.Update({}, report, 11.0f);
    feed.Update({}, report, 11.5f);

    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.x, 50.0f);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.z, 32.0f);
}

TEST(MarkerFeedMotion, VerticalTeleportSnaps)
{
    MarkerFeed feed(kParams);
    MarkerIdentity::Observation report[] = {{3, {64, 64}, 1}};
    feed.Update({}, report, 10.0f);
    report[0].pos = ncl_math::Vector3{64, 64, 1000};
    report[0].seq = 2;
    feed.Update({}, report, 11.0f);

    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.z, 1000.0f);
}

TEST(MarkerFeedMotion, SnapshotNoInterpResetsPlayback)
{
    MarkerFeed feed(kNamedParams);
    MarkerIdentity::Observation reports[] = {{1, {64, 64, 1}, 1}};
    feed.Update({}, reports, 10);
    MarkerIdentity::Entity entities[] = {{1, {64, 64, 257}, true}};
    feed.Update(entities, reports, 10.1f);

    ASSERT_EQ(feed.get_markers().size(), 1u);
    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.z, 257);

    feed.Update({}, reports, 10.2f);

    EXPECT_FLOAT_EQ(feed.get_markers()[0].pos.z, 257);
}
