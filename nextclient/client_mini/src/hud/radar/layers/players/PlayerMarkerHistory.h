#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "hud/radar/RadarLimits.h"

class PlayerMarkerHistory
{
public:
    struct Frame
    {
        float time{};
        int message{};
        int layer = -1;
        int resets{};
        int jobs{};
        int batches{};
        int quads{};
        std::uint64_t drawn{};
        std::uint64_t snapshot{};
        std::uint64_t broadcast{};
        std::uint64_t fading{};
        std::uint64_t fade_in{};
        std::uint64_t overlap{};
        std::uint64_t snapshot_steps{};
    };

    struct Sample
    {
        Frame frame{};
        std::uint64_t lost{};
        std::uint64_t updated{};
        std::uint64_t fading{};
        int min_jobs{};
        std::uint64_t fade_in{};
        std::uint64_t overlap{};
        std::uint64_t snapshot_steps{};
    };

    // Must precede samples_, whose type uses this array bound.
    static constexpr int kMaxSamples = 128;

private:
    std::array<Sample, kMaxSamples> samples_{};
    std::array<std::uint32_t, kRadarMaxPlayers> sequences_{};
    std::uint64_t last_drawn_{};
    float last_time_{};
    float bucket_time_{};
    int head_{};
    int count_{};

public:
    void Record(Frame frame, std::span<const std::uint32_t, kRadarMaxPlayers> sequences);
    int CopySamples(std::span<Sample> out) const;
    void Reset();
};

void PlayerMarkerHistory_PrintSample(const PlayerMarkerHistory::Sample& sample, void (*print)(const char*, ...));
