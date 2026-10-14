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
        int player{};
        ncl_math::Vector3 pos{};
        RadarTeam team{};
        float yaw{};
        bool has_dir{};
        bool carrier{};
    };

    struct Mark
    {
        int player{};
        ncl_math::Vector3 pos{};
        RadarTeam team{};
        float time{};
        int alpha{};
        float yaw{};
        bool has_dir{};
        bool carrier{};
        bool uncertain{};
    };

private:
    std::array<Observation, kRadarMaxPlayers + 1> previous_{};
    std::vector<Mark> marks_;

public:
    void Update(std::span<const Observation> visible, float time);
    void Forget(int player);
    void OnPlayerDeath(int player);
    void Reset();

    std::span<const Mark> get_marks() const
    {
        return marks_;
    }
};
