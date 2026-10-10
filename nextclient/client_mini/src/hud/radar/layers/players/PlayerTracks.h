#pragma once

#include <array>
#include <span>

#include <ncl_math/vec3.h>

#include "hud/radar/RadarLimits.h"

struct PlayerTrackPoint
{
    ncl_math::Vector3 pos{};
    int layer = -1;
};

class PlayerTracks
{
public:
    // Must precede Buf, whose type uses this array bound.
    static constexpr int kMaxPoints = 34;

private:
    struct Buf
    {
        std::array<PlayerTrackPoint, kMaxPoints> points;
        int count;
        int head;
        float last_time;
    };

    std::array<Buf, kRadarMaxPlayers + 1> tracks_{};
    bool reset_pending_{};
    float suppress_until_{};

    static constexpr float kInterval = 0.10f;
    static constexpr float kJumpDist = 300.0f;
    static constexpr float kResetDelay = 0.5f;

public:
    // Arms a suppression window resolved by the next BeginFrame; without it trails
    // trace the engine's round-reset teleport interpolation.
    void Reset();
    void BeginFrame(float time);
    bool IsSuppressed(float time) const;

    void Clear(int player);

    bool is_sample_due(int player, float time) const;
    void Sample(int player, ncl_math::Vector3 pos, int layer, float time, bool break_on_jump = true);

    // Filled oldest-to-newest, up to out.size().
    int CopyPoints(int player, std::span<PlayerTrackPoint> out) const;
};
