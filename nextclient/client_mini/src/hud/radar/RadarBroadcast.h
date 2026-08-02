#pragma once

#include <cstdint>

#include <ncl_math/vec3.h>

// Live radar state fed by server user messages that broadcast positions outside
// the local PVS: the dropped/planted bomb (BombDrop/BombPickup, Terrorists) and
// the hostages (HostagePos/HostageK, Counter-Terrorists). The advanced radar
// reads this so those objectives stay visible even when their entities are not in
// the client's potentially-visible set.
namespace RadarBroadcast
{

    struct Bomb
    {
        bool active;
        ncl_math::Vector3 pos;
        bool planted;
        float time; // client time the last message arrived
        // Bumped on every message, clears included, so a consumer sees a repeat of the
        // same position as a new message (RadarPolicy classifies the message stream).
        std::uint32_t seq;
    };

    struct Hostage
    {
        bool active;
        int index; // the server's hostage index, as sent in the message
        ncl_math::Vector2 pos;
        float time; // client time the last message arrived
        std::uint32_t seq;
    };

    // A zero position is the engine's "hide" sentinel for the planted bomb; callers
    // pass it through to ClearBomb rather than SetBomb.
    void SetBomb(ncl_math::Vector3 pos, bool planted, float time);
    void ClearBomb(float time);
    const Bomb& GetBomb();

    // Hostage positions are keyed by the server's logical hostage index. The server
    // only resends on movement, so entries never time out here: they are cleared by
    // HostageK or by a round/level reset. A message re-purposed as a marker does expire,
    // but on its own timestamp (see RadarPolicy), not by dropping the entry.
    void SetHostage(int index, ncl_math::Vector2 pos, float time);
    void KillHostage(int index);
    int CollectHostages(Hostage* out, int max_out);

    void ResetHostages();
    void Reset();
} // namespace RadarBroadcast
