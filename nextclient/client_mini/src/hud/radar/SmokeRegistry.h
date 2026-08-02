#pragma once

#include <array>
#include <span>

#include <ncl_math/vec3.h>

// Fed from the game's createsmoke event: it carries the real cloud centre
// (m_vSmokeDetonate), where the bouncing grenade entity does not, and repeats once a
// second while the cloud lives.
class SmokeRegistry
{
public:
    struct Disc
    {
        ncl_math::Vector2 center;
        float radius; // world units
    };

    // `exact` marks the events that carry a trustworthy centre (the detonation itself and
    // the puffs that fill in their angles field); the rest only refresh a cloud's expiry.
    void OnSmoke(const ncl_math::Vector3& center, bool exact, float time);

    bool Blocks(const ncl_math::Vector3& a, const ncl_math::Vector3& b, float time) const;

    int Collect(std::span<Disc> out, float time);

    void Reset();

private:
    static constexpr int kMaxClouds = 16;

    struct Cloud
    {
        ncl_math::Vector3 center;
        float start; // detonation time; the radius grows from here
        float last_seen;
        bool active;
    };

    // Slot holding this centre, else a free one, else -1. Expires stale slots on the way,
    // because Collect runs only on the frames the radar draws while events keep arriving.
    int Acquire(const ncl_math::Vector3& center, float time);

    static float ComputeRadiusAt(const Cloud& cloud, float time);

    std::array<Cloud, kMaxClouds> clouds_{};
};
