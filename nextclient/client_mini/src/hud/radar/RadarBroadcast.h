#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <ncl_math/vec3.h>

// Fed by the user messages that carry positions outside the local PVS: BombDrop/BombPickup (living
// Terrorists, except the plant and the clears, which reach everyone) and HostagePos/HostageK (living
// Counter-Terrorists).
class RadarBroadcast
{
public:
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
        ncl_math::Vector2 pos;
        float time;
        std::uint32_t seq;
    };

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
    void SetHostage(int index, ncl_math::Vector2 pos, float time);
    void KillHostage(int index);
    int CollectHostages(std::span<Hostage> out) const;

    void ResetHostages();

    // Drops the bomb as a message, so the sequence advances and a consumer classifying the
    // stream sees the clear rather than a silent gap.
    void Reset(float time);

private:
    // The key is the message index byte: CHostage numbers its own 1..N, while a re-purposed
    // HostagePos carries whatever the mod put there, so the table spans the whole byte range.
    static constexpr int kHostageIndexRange = 256;

    Bomb bomb_{};
    std::array<Hostage, kHostageIndexRange> hostages_{};
    // Outside the structs so a clear does not reset it along with them.
    std::uint32_t seq_{};
};
