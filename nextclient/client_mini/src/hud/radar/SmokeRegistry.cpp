#include "SmokeRegistry.h"
#include "../../main.h"

#include <ncl_math/vec3.h>
#include <event_args.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace
{
    constexpr int kMaxClouds = 16;
    constexpr float kRadius = 150.0f;
    constexpr float kGrow = 1.5f; // seconds to reach full radius from detonation
    constexpr float kRise = 60.0f; // sphere centre above the floor
    constexpr float kMatch = 180.0f; // event centre proximity to an existing cloud
    constexpr float kExpire = 2.5f; // drop a cloud with no event for this long (puffs ~1s)
    constexpr float kMinDraw = 16.0f;

    struct Cloud
    {
        ncl_math::Vector3 center;
        float start; // detonation time (radius grows from here)
        float last; // last event time (expiry)
        bool active;
    };
    std::array<Cloud, kMaxClouds> g_Clouds;

    void (*g_OrigCreatesmoke)(event_args_t*);

    float CloudRadius(const Cloud& c, float time)
    {
        float age = time - c.start;

        if (age <= 0.0f)
        {
            return 0.0f;
        }

        float result = age >= kGrow ? kRadius : (age / kGrow) * kRadius;
        return result;
    }

    bool NonZero(const float* v)
    {
        bool result = std::fabs(v[0]) > 1.0f || std::fabs(v[1]) > 1.0f || std::fabs(v[2]) > 1.0f;
        return result;
    }

    void OnCreateSmoke(event_args_t* args)
    {
        // Detonation carries the exact centre in origin (iparam2 == 1); the per-
        // second puffs carry it in angles. Prefer angles when present, else origin.
        bool deto = (args->iparam2 == 1);
        const float* exact = NonZero(args->angles) ? args->angles : args->origin;
        float now = gEngfuncs.GetClientTime();

        int slot = -1, free = -1;

        for (int i = 0; i < kMaxClouds; i++)
        {
            if (g_Clouds[i].active)
            {
                ncl_math::Vector3 delta = g_Clouds[i].center - ncl_math::Vector3::Ref(exact);

                if (delta.LengthSqr() < kMatch * kMatch)
                {
                    slot = i;
                    break;
                }
            }
            else if (free < 0)
            {
                free = i;
            }
        }

        if (slot < 0)
        {
            slot = free;
        }

        if (slot < 0)
        {
            return;
        }

        Cloud& c = g_Clouds[slot];

        if (!c.active)
        {
            c.active = true;
            c.start = now;
            c.center = ncl_math::Vector3::Ref(exact);
        }
        else if (NonZero(args->angles) || deto)
        {
            // refine to the exact centre without resetting the growth timer
            c.center = ncl_math::Vector3::Ref(exact);
        }

        c.last = now;
    }

    void CreateSmoke_Hook(event_args_t* args)
    {
        OnCreateSmoke(args);

        if (g_OrigCreatesmoke)
        {
            g_OrigCreatesmoke(args);
        }
    }
} // namespace

RadarSmoke::EventCallback_t RadarSmoke::WrapEventHook(const char* name, EventCallback_t cb)
{
    if (name && std::strstr(name, "createsmoke"))
    {
        g_OrigCreatesmoke = cb;
        return CreateSmoke_Hook;
    }

    return cb;
}

bool RadarSmoke::Blocks(const ncl_math::Vector3& a, const ncl_math::Vector3& b, float time)
{
    for (int i = 0; i < kMaxClouds; i++)
    {
        if (!g_Clouds[i].active || time - g_Clouds[i].last > kExpire)
        {
            continue;
        }

        float r = CloudRadius(g_Clouds[i], time);

        if (r < kMinDraw)
        {
            continue;
        }

        ncl_math::Vector3 c = g_Clouds[i].center;
        c.z += kRise;

        if (ncl_math::SegPointDistSq(a.data(), b.data(), c.data()) < r * r)
        {
            return true;
        }
    }

    return false;
}

int RadarSmoke::Collect(Disc* out, int max, float time)
{
    int n = 0;

    for (int i = 0; i < kMaxClouds && n < max; i++)
    {
        if (!g_Clouds[i].active)
        {
            continue;
        }

        if (time - g_Clouds[i].last > kExpire)
        {
            g_Clouds[i] = Cloud{};
            continue;
        }

        float r = CloudRadius(g_Clouds[i], time);

        if (r < kMinDraw)
        {
            continue;
        }

        out[n].center = {g_Clouds[i].center.x, g_Clouds[i].center.y};
        out[n].radius = r;
        n++;
    }

    return n;
}

void RadarSmoke::Reset()
{
    for (int i = 0; i < kMaxClouds; i++)
    {
        g_Clouds[i] = Cloud{};
    }
}
