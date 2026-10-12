#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <ncl_math/vec3.h>

#include "hud/radar/RadarLimits.h"

class RadarPolicy;

// Fed by the user messages that carry positions outside the local PVS: Radar (living teammates),
// BombDrop/BombPickup (living Terrorists, except the plant and the clears, which reach everyone),
// and HostagePos/HostageK (living Counter-Terrorists).
class RadarBroadcast
{
public:
    struct Player
    {
        ncl_math::Vector3 pos;
        std::uint32_t seq;
        ncl_math::Vector3 previous_pos{};
        float time{};
        float interval{};
    };

    struct Bomb
    {
        bool active;
        ncl_math::Vector3 pos;
        bool planted;
        float time; // client time the last message arrived
        // Bumped on every message, clears included, so a repeat of the same position still
        // reads as a new message.
        std::uint32_t seq;
    };

    struct Hostage
    {
        bool active;
        int index; // CHostage::m_iHostageIndex, a spawn counter unrelated to the edict index
        ncl_math::Vector3 pos;
        float time;
        std::uint32_t seq;
        ncl_math::Vector3 previous_pos{};
        float interval{};
    };

private:
    std::array<Player, kRadarMaxPlayers + 1> players_{};
    Bomb bomb_{};

    // Must precede hostages_, whose type uses this array bound.
    // The key is the message index byte: CHostage numbers its own 1..N, while a re-purposed
    // HostagePos carries whatever the mod put there, so the table spans the whole byte range.
    static constexpr int kHostageIndexRange = 256;

    std::array<Hostage, kHostageIndexRange> hostages_{};
    // Outside the structs so a clear does not reset it along with them.
    std::uint32_t seq_{};

public:
    void SetPlayer(int index, const ncl_math::Vector3& pos, float time);
    void ResetPlayers();

    const Player& get_player(int index) const
    {
        return players_[index];
    }

    // A zero position is the engine's "hide" sentinel and is stored like any other; ClearBomb
    // is what hides the marker.
    void SetBomb(const ncl_math::Vector3& pos, bool planted, float time);
    void ClearBomb(float time);

    const Bomb& get_bomb() const
    {
        return bomb_;
    }

    // The server only resends on movement, so entries never time out here: they are cleared
    // by HostageK or by a round/level reset.
    void SetHostage(int index, ncl_math::Vector3 pos, float time);
    void KillHostage(int index);
    int CollectHostages(std::span<Hostage> out) const;
    int CollectHostageMarkers(std::span<Hostage> out, const RadarPolicy& policy) const;

    void ResetHostages();

    // Drops the bomb as a message, so the sequence advances and a consumer classifying the
    // stream sees the clear rather than a silent gap.
    void Reset(float time);
};
