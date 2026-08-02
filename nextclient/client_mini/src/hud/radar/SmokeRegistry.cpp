#include "SmokeRegistry.h"

namespace
{
    constexpr float kRadius = 150.0f;
    constexpr float kGrow = 1.5f; // seconds to reach full radius from detonation
    constexpr float kRise = 60.0f; // sphere centre above the floor
    constexpr float kMatch = 180.0f; // event centre proximity to an existing cloud
    constexpr float kExpire = 2.5f; // drop a cloud with no event for this long (puffs ~1s)
    constexpr float kMinDraw = 16.0f;
} // namespace

int SmokeRegistry::Acquire(const ncl_math::Vector3& center, float time)
{
    int free = -1;

    for (int i = 0; i < kMaxClouds; i++)
    {
        Cloud& cloud = clouds_[i];

        if (cloud.active && time - cloud.last_seen > kExpire)
        {
            cloud = Cloud{};
        }

        if (!cloud.active)
        {
            if (free < 0)
            {
                free = i;
            }

            continue;
        }

        if ((cloud.center - center).LengthSqr() < kMatch * kMatch)
        {
            return i;
        }
    }

    return free;
}

float SmokeRegistry::ComputeRadiusAt(const Cloud& cloud, float time)
{
    float age = time - cloud.start;

    if (age <= 0.0f)
    {
        return 0.0f;
    }

    return age >= kGrow ? kRadius : (age / kGrow) * kRadius;
}

void SmokeRegistry::OnSmoke(const ncl_math::Vector3& center, bool exact, float time)
{
    int slot = Acquire(center, time);

    if (slot < 0)
    {
        return;
    }

    Cloud& cloud = clouds_[slot];

    if (!cloud.active)
    {
        cloud.active = true;
        cloud.start = time;
        cloud.center = center;
    }
    else if (exact)
    {
        cloud.center = center; // refine without resetting the growth timer
    }

    cloud.last_seen = time;
}

bool SmokeRegistry::Blocks(const ncl_math::Vector3& a, const ncl_math::Vector3& b, float time) const
{
    for (const Cloud& cloud : clouds_)
    {
        if (!cloud.active || time - cloud.last_seen > kExpire)
        {
            continue;
        }

        float radius = ComputeRadiusAt(cloud, time);

        if (radius < kMinDraw)
        {
            continue;
        }

        ncl_math::Vector3 c = cloud.center;
        c.z += kRise;

        if (ncl_math::SegPointDistSq(a.data(), b.data(), c.data()) < radius * radius)
        {
            return true;
        }
    }

    return false;
}

int SmokeRegistry::Collect(std::span<Disc> out, float time)
{
    int n = 0;

    for (Cloud& cloud : clouds_)
    {
        if (n >= (int)out.size())
        {
            break;
        }

        if (!cloud.active)
        {
            continue;
        }

        if (time - cloud.last_seen > kExpire)
        {
            cloud = Cloud{};
            continue;
        }

        float radius = ComputeRadiusAt(cloud, time);

        if (radius < kMinDraw)
        {
            continue;
        }

        out[n].center = {cloud.center.x, cloud.center.y};
        out[n].radius = radius;
        n++;
    }

    return n;
}

void SmokeRegistry::Reset()
{
    clouds_ = {};
}
