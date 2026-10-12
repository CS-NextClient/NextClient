#include <cstdarg>
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include "hud/radar/layers/players/PlayerMarkerHistory.h"

TEST(PlayerMarkerHistory, BriefGroupLossAndFadesSurviveUntilTheNextSample)
{
    PlayerMarkerHistory history;
    std::array<std::uint32_t, kRadarMaxPlayers> sequences{};
    sequences[0] = sequences[31] = 1;
    constexpr std::uint64_t players = 1ULL | (1ULL << 31);
    history.Record({.time = 10.0f, .message = 100, .jobs = 2, .drawn = players, .broadcast = players}, sequences);
    sequences[0] = sequences[31] = 2;
    history.Record({.time = 10.01f, .message = 101, .jobs = 0}, sequences);
    history.Record(
        {.time = 10.02f,
         .message = 102,
         .jobs = 2,
         .drawn = players,
         .broadcast = players,
         .fading = players,
         .fade_in = 1ULL,
         .overlap = 1ULL << 31,
         .snapshot_steps = 1ULL << 31},
        sequences
    );
    history.Record({.time = 10.04f, .message = 103, .jobs = 2, .drawn = players, .broadcast = players}, sequences);
    std::array<PlayerMarkerHistory::Sample, PlayerMarkerHistory::kMaxSamples> samples{};
    ASSERT_EQ(history.CopySamples(samples), 1);
    EXPECT_EQ(samples[0].frame.drawn, players);
    EXPECT_EQ(samples[0].frame.message, 103);
    EXPECT_EQ(samples[0].lost, players);
    EXPECT_EQ(samples[0].updated, players);
    EXPECT_EQ(samples[0].fading, players);
    EXPECT_EQ(samples[0].min_jobs, 0);
    EXPECT_EQ(samples[0].fade_in, 1ULL);
    EXPECT_EQ(samples[0].overlap, 1ULL << 31);
    EXPECT_EQ(samples[0].snapshot_steps, 1ULL << 31);
}

TEST(PlayerMarkerHistory, KeepsBoundedChronologicalHistoryThroughResets)
{
    PlayerMarkerHistory history;
    std::array<std::uint32_t, kRadarMaxPlayers> sequences{};
    for (int frame = 0; frame < 150; frame++)
    {
        history.Record({.time = 10.0f + frame * 0.1f, .message = frame, .resets = frame / 50, .jobs = 64}, sequences);
    }
    std::array<PlayerMarkerHistory::Sample, PlayerMarkerHistory::kMaxSamples> samples{};
    ASSERT_EQ(history.CopySamples(samples), 128);
    EXPECT_EQ(samples.front().frame.message, 22);
    EXPECT_EQ(samples.back().frame.message, 149);
    EXPECT_EQ(samples.front().frame.resets, 0);
    EXPECT_EQ(samples.back().frame.resets, 2);
    for (int index = 1; index < 128; index++)
    {
        EXPECT_GT(samples[index].frame.time, samples[index - 1].frame.time);
    }
}

TEST(PlayerMarkerHistory, TimeRewindAndExplicitResetDiscardUnrelatedHistory)
{
    PlayerMarkerHistory history;
    std::array<std::uint32_t, kRadarMaxPlayers> sequences{};
    sequences[63] = 4;
    history.Record({.time = 10.0f, .drawn = 1ULL << 63}, sequences);
    history.Record({.time = 9.0f, .message = 20}, sequences);
    std::array<PlayerMarkerHistory::Sample, PlayerMarkerHistory::kMaxSamples> samples{};
    ASSERT_EQ(history.CopySamples(samples), 1);
    EXPECT_EQ(samples[0].frame.message, 20);
    EXPECT_EQ(samples[0].lost, 0);
    EXPECT_EQ(samples[0].updated, 1ULL << 63);
    history.Reset();
    EXPECT_EQ(history.CopySamples(samples), 0);
}

namespace
{
    std::string g_ConsoleLine;

    void EngineConsole(const char* format, ...)
    {
        ASSERT_STREQ(format, "%s") << "The engine console must receive preformatted 64-bit masks.";
        char text[512];
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(text, sizeof(text), format, arguments);
        va_end(arguments);
        g_ConsoleLine = text;
    }
} // namespace

TEST(PlayerMarkerHistory, ConsoleReceivesCompleteMasksAsPreformattedText)
{
    g_ConsoleLine.clear();
    PlayerMarkerHistory::Sample sample{
        .frame =
            {
                .time = 13.48f,
                .message = 499,
                .layer = 0,
                .resets = 10,
                .jobs = 13,
                .batches = 2,
                .quads = 21,
                .drawn = 0x800000018402cac3ULL,
                .snapshot = 0x0000000080000000ULL,
                .broadcast = 0x800000010402cac3ULL,
            },
        .lost = 0x8000000000000001ULL,
        .updated = 0x4000000000000800ULL,
        .fading = 0x2000000000000800ULL,
        .min_jobs = 11,
        .fade_in = 0x1000000000000040ULL,
        .overlap = 0x0800000000000100ULL,
        .snapshot_steps = 0x0400000000000200ULL,
    };
    PlayerMarkerHistory_PrintSample(sample, EngineConsole);
    EXPECT_EQ(
        g_ConsoleLine,
        "history t=13.480 msg=499 layer=0 resets=10 jobs=13 min_jobs=11 batches=2 quads=21 "
        "drawn=800000018402cac3 snapshot=0000000080000000 broadcast=800000010402cac3 "
        "updated=4000000000000800 fading=2000000000000800 lost=8000000000000001 "
        "fade_in=1000000000000040 overlap=0800000000000100 snapshot_steps=0400000000000200\n"
    );
}
