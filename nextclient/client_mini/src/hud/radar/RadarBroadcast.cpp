#include "RadarBroadcast.h"

#include <array>

namespace
{
    // Hostages are keyed by the per-message index byte (READ_BYTE in
    // UserMsg_HostagePosHandler), so the table spans the whole byte range:
    // depending on the build that id is a small logical index or the hostage
    // entity index, and a smaller table would silently drop the larger ids.
    constexpr int kMaxHostages = 256;

    RadarBroadcast::Bomb g_Bomb;
    // One counter for every broadcast message, kept outside the structs so a clear does not reset it along with them.
    std::uint32_t g_MessageSeq;
    std::array<RadarBroadcast::Hostage, kMaxHostages> g_Hostages;
} // namespace

namespace RadarBroadcast
{
    void SetBomb(ncl_math::Vector3 pos, bool planted, float time)
    {
        g_Bomb.active = true;
        g_Bomb.pos = pos;
        g_Bomb.planted = planted;
        g_Bomb.time = time;
        g_Bomb.seq = ++g_MessageSeq;
    }

    void ClearBomb(float time)
    {
        g_Bomb = Bomb{};
        g_Bomb.time = time;
        g_Bomb.seq = ++g_MessageSeq;
    }

    const Bomb& GetBomb()
    {
        return g_Bomb;
    }

    void SetHostage(int index, ncl_math::Vector2 pos, float time)
    {
        if (index < 0 || index >= kMaxHostages)
        {
            return;
        }

        Hostage& h = g_Hostages[index];
        h.active = true;
        h.index = index;
        h.pos = pos;
        h.time = time;
        h.seq = ++g_MessageSeq;
    }

    void KillHostage(int index)
    {
        if (index < 0 || index >= kMaxHostages)
        {
            return;
        }

        g_Hostages[index] = Hostage{};
    }

    int CollectHostages(Hostage* out, int max_out)
    {
        int n = 0;

        for (int i = 0; i < kMaxHostages && n < max_out; i++)
        {
            if (g_Hostages[i].active)
            {
                out[n++] = g_Hostages[i];
            }
        }

        return n;
    }

    void ResetHostages()
    {
        g_Hostages.fill(Hostage{});
    }

    void Reset()
    {
        g_Bomb = Bomb{};
        ResetHostages();
    }
} // namespace RadarBroadcast
