#pragma once

#include <array>
#include <span>
#include <vector>

#include <ncl_math/vec3.h>

#include "hud/radar/RadarLimits.h"
#include "hud/radar/RadarTeam.h"

class PlayerLastSeen
{
public:
    struct Observation
    {
        int player;
        ncl_math::Vector2 pos;
        RadarTeam team;
    };

    struct Mark
    {
        int player;
        ncl_math::Vector2 pos;
        RadarTeam team;
        float time;
        int alpha;
    };

    void Update(std::span<const Observation> visible, float time);
    void Forget(int player);
    void OnPlayerDeath(int player);
    void Reset();

    std::span<const Mark> get_marks() const
    {
        return marks_;
    }

private:
    std::array<Observation, kRadarMaxPlayers + 1> previous_{};
    std::vector<Mark> marks_;
};
