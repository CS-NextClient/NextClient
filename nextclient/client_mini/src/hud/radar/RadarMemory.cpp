#include "RadarMemory.h"

#include <ncl_math/scalar.h>

void PlayerTracks::Reset()
{
    tracks_ = {};
    reset_pending_ = true;
}

void PlayerTracks::BeginFrame(float time)
{
    if (reset_pending_)
    {
        suppress_until_ = time + kResetDelay;
        reset_pending_ = false;
    }
}

bool PlayerTracks::Suppressed(float time) const
{
    return time < suppress_until_;
}

void PlayerTracks::Clear(int player)
{
    tracks_[player] = Buf{};
}

void PlayerTracks::Sample(int player, ncl_math::Vector2 pos, float time)
{
    Buf& tb = tracks_[player];

    if (time - tb.last_time < kInterval)
    {
        return;
    }

    tb.last_time = time;

    // Drop the trail on a sudden jump (respawn, observer switch, teleport) so it
    // does not draw a long streak across the map.
    if (tb.count > 0)
    {
        int last = (tb.head - 1 + kMaxPoints) % kMaxPoints;

        if ((pos - tb.points[last]).LengthSqr() > kJumpDist * kJumpDist)
        {
            tb.count = 0;
        }
    }

    tb.points[tb.head] = pos;
    tb.head = (tb.head + 1) % kMaxPoints;

    if (tb.count < kMaxPoints)
    {
        tb.count++;
    }
}

int PlayerTracks::CopyPoints(int player, ncl_math::Vector2* out) const
{
    const Buf& tb = tracks_[player];

    for (int k = 0; k < tb.count; k++)
    {
        out[k] = tb.points[(tb.head - tb.count + k + kMaxPoints) % kMaxPoints];
    }

    return tb.count;
}

void DeathMarks::Reset()
{
    for (Mark& m : marks_)
    {
        m.active = false;
    }
}

void DeathMarks::Add(ncl_math::Vector2 world, ncl_math::Color color, float time)
{
    Mark& m = marks_[head_];
    m.pos = world;
    m.color = color;
    m.time = time;
    m.active = true;
    head_ = (head_ + 1) % kMax;
}

int DeathMarks::CollectVisible(Visible* out, int max, float time)
{
    int n = 0;

    for (Mark& m : marks_)
    {
        if (!m.active || n >= max)
        {
            continue;
        }

        float age = time - m.time;

        if (age < 0.0f || age >= kLifetime)
        {
            m.active = false;
            continue;
        }

        float vis = ncl_math::HoldLinear(age, kHold, kLifetime);
        out[n++] = {m.pos, m.color, (int)(vis * 255.0f)};
    }

    return n;
}

void ObjectiveMemory::Reset()
{
    bomb_ = BombSpot{};
    hostage_spots_.fill(HostageSpot{});
}

void ObjectiveMemory::ClearHostageVisibility()
{
    for (HostageSpot& s : hostage_spots_)
    {
        s.visible = false;
    }
}

int ObjectiveMemory::FindOrCreate(int ent_index, ncl_math::Vector2 pos, float time)
{
    for (int j = 0; j < kHostageSlots; j++)
    {
        if (hostage_spots_[j].valid && hostage_spots_[j].ent == ent_index)
        {
            return j;
        }
    }

    for (int j = 0; j < kHostageSlots; j++)
    {
        if (!hostage_spots_[j].valid)
        {
            hostage_spots_[j] = {true, ent_index, pos, time, false};
            return j;
        }
    }

    return -1;
}

void ObjectiveMemory::EaseTo(HostageSpot& s, ncl_math::Vector2 target, float dt) const
{
    s.pos = Ease(s.pos, target, dt);
}

ncl_math::Vector2 ObjectiveMemory::Ease(ncl_math::Vector2 pos, ncl_math::Vector2 target, float dt)
{
    ncl_math::Vector2 d = target - pos;

    if (d.LengthSqr() > kSnapDist * kSnapDist)
    {
        return target;
    }

    return pos + d * ncl_math::SmoothingFactor(kSmoothRate, dt);
}
