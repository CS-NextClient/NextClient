#pragma once

#include <ncl_math/vec3.h>

typedef struct event_args_s event_args_t;

// Tracks smoke clouds for the advanced radar from the game's own createsmoke
// event, which carries the exact detonation centre (m_vSmokeDetonate). This is
// accurate where the bouncing grenade entity is not: the event fires at the real
// cloud position, repeatedly (one detonation event plus a puff event each second).
namespace RadarSmoke
{
    struct Disc
    {
        ncl_math::Vector2 center; // world centre
        float radius; // world units
    };

    using EventCallback_t = void (*)(event_args_t*);

    // Called from the CL_HookEvent hook for every event registration. For the
    // createsmoke event it stores the original handler and returns a wrapper that
    // feeds this registry and then chains to the original; other events pass through.
    EventCallback_t WrapEventHook(const char* name, EventCallback_t cb);

    // True if the segment a..b passes through any active cloud.
    bool Blocks(const ncl_math::Vector3& a, const ncl_math::Vector3& b, float time);

    // Active clouds for drawing; returns the count written to out (<= max).
    int Collect(Disc* out, int max, float time);

    void Reset();
} // namespace RadarSmoke
