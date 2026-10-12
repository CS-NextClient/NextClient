#include <array>

#include <gtest/gtest.h>

#include "hud/radar/RadarFrame.h"
#include "hud/radar/layers/players/PlayerMarkerFade.h"
#include "hud/radar/layers/players/PlayerMarkerHistory.h"
#include "hud/radar/layers/players/PlayerTracks.h"

TEST(RadarFrameTime, CapturedClockCorrectionsKeepThePreviousFrameTime)
{
    struct Correction
    {
        float before;
        float after;
    };
    for (Correction correction :
         {Correction{1195.549438f, 1195.548828f}, {1198.450562f, 1198.450439f}, {1232.328857f, 1232.328247f}, {1232.328247f, 1232.327759f}})
    {
        SCOPED_TRACE(correction.before);
        EXPECT_FLOAT_EQ(RadarFrame_NormalizeTime(correction.after, correction.before), correction.before);
    }
}

TEST(RadarFrameTime, ForwardTimeAndSubstantialRewindReachTheFrameUnchanged)
{
    EXPECT_FLOAT_EQ(RadarFrame_NormalizeTime(1195.56f, 1195.55f), 1195.56f);
    EXPECT_FLOAT_EQ(RadarFrame_NormalizeTime(1195.55f, 1195.55f), 1195.55f);
    EXPECT_FLOAT_EQ(RadarFrame_NormalizeTime(1194.55f, 1195.55f), 1194.55f);
    EXPECT_FLOAT_EQ(RadarFrame_NormalizeTime(0.0f, 1235.0f), 0.0f);
    float held = RadarFrame_NormalizeTime(10.994f, 11.0f);
    EXPECT_FLOAT_EQ(held, 11.0f);
    EXPECT_FLOAT_EQ(RadarFrame_NormalizeTime(10.98f, held), 10.98f);
}

TEST(RadarFrameTime, ClockCorrectionPreservesConcurrentPlayerCrossfades)
{
    std::array<PlayerMarkerFade, 32> fades;
    for (int player = 0; player < 32; player++)
    {
        SCOPED_TRACE(player);
        float x = 100.0f + player * 200.0f;
        fades[player].Update({x, 0, 0}, 1195.0f, true, 1);
        fades[player].Update({x + 100, 0, 0}, 1195.5f, false, 2);
        PlayerMarkerFade::Visible before = fades[player].Update({x + 100, 0, 0}, 1195.549438f, false, 2);
        float time = RadarFrame_NormalizeTime(1195.548828f, 1195.549438f);
        PlayerMarkerFade::Visible after = fades[player].Update({x + 100, 0, 0}, time, false, 2);
        EXPECT_FLOAT_EQ(after.pos.x, x + 100);
        EXPECT_FLOAT_EQ(after.previous_pos.x, x);
        EXPECT_FLOAT_EQ(after.alpha, before.alpha);
        EXPECT_FLOAT_EQ(after.previous_alpha, before.previous_alpha);
        EXPECT_GT(after.previous_alpha, 0.0f);
        EXPECT_FLOAT_EQ(fades[player].Update({x + 100, 0, 0}, 1195.64f, false, 2).alpha, 1.0f);
    }
}

TEST(RadarFrameTime, RepeatedSmallCorrectionsPreserveHistoryAndTrailDeadlines)
{
    PlayerMarkerHistory history;
    PlayerTracks tracks;
    std::array<std::uint32_t, kRadarMaxPlayers> sequences{};
    sequences[0] = 3;
    history.Record({.time = 1232.1f, .message = 1580, .drawn = 1}, sequences);
    history.Record({.time = 1232.328857f, .message = 1588, .drawn = 1}, sequences);
    tracks.Sample(1, {100, 0, 0}, 0, 1232.1f);
    tracks.Sample(1, {110, 0, 0}, 0, 1232.328857f);
    float time = 1232.328857f;
    for (float incoming : {1232.328247f, 1232.327759f})
    {
        time = RadarFrame_NormalizeTime(incoming, time);
        history.Record({.time = time, .message = 1588, .drawn = 1}, sequences);
        EXPECT_FALSE(tracks.is_sample_due(1, time));
        tracks.Sample(1, {120, 0, 0}, 0, time);
    }
    std::array<PlayerMarkerHistory::Sample, PlayerMarkerHistory::kMaxSamples> samples{};
    ASSERT_EQ(history.CopySamples(samples), 2);
    EXPECT_EQ(samples[0].frame.message, 1580);
    EXPECT_EQ(samples[1].updated, 0);
    std::array<PlayerTrackPoint, PlayerTracks::kMaxPoints> points{};
    ASSERT_EQ(tracks.CopyPoints(1, points), 2);
    EXPECT_FLOAT_EQ(points[1].pos.x, 110.0f);
}
