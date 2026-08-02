#include "RadarBroadcast.h"

void RadarBroadcast::SetBomb(const ncl_math::Vector3& pos, bool planted, float time)
{
    bomb_.active = true;
    bomb_.pos = pos;
    bomb_.planted = planted;
    bomb_.time = time;
    bomb_.seq = ++seq_;
}

void RadarBroadcast::ClearBomb(float time)
{
    bomb_ = Bomb{};
    bomb_.time = time;
    bomb_.seq = ++seq_;
}

void RadarBroadcast::SetHostage(int index, ncl_math::Vector2 pos, float time)
{
    if (index < 0 || index >= kHostageIndexRange)
    {
        return;
    }

    Hostage& hostage = hostages_[index];
    hostage.active = true;
    hostage.index = index;
    hostage.pos = pos;
    hostage.time = time;
    hostage.seq = ++seq_;
}

void RadarBroadcast::KillHostage(int index)
{
    if (index < 0 || index >= kHostageIndexRange)
    {
        return;
    }

    hostages_[index] = Hostage{};
}

int RadarBroadcast::CollectHostages(std::span<Hostage> out) const
{
    int n = 0;

    for (int i = 0; i < kHostageIndexRange && n < (int)out.size(); i++)
    {
        if (hostages_[i].active)
        {
            out[n++] = hostages_[i];
        }
    }

    return n;
}

void RadarBroadcast::ResetHostages()
{
    hostages_ = {};
}

void RadarBroadcast::Reset(float time)
{
    ClearBomb(time);
    ResetHostages();
}
