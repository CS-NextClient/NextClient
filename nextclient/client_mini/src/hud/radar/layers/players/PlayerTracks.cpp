#include "PlayerTracks.h"

#include <algorithm>

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

bool PlayerTracks::IsSuppressed(float time) const
{
    // Bounded below as well, so a backwards clock step leaves the window instead of
    // parking in it and wiping every trail each frame.
    return time < suppress_until_ && time >= suppress_until_ - kResetDelay;
}

void PlayerTracks::Clear(int player)
{
    tracks_[player] = Buf{};
}

void PlayerTracks::Sample(int player, ncl_math::Vector2 pos, float time)
{
    Buf& buf = tracks_[player];

    // CL_Parse_Time steps cl.time backwards on a clock reset without a level change, which
    // would otherwise leave last_time in the future and freeze the trail.
    if (time >= buf.last_time && time - buf.last_time < kInterval)
    {
        return;
    }

    buf.last_time = time;

    // Drop the trail on a sudden jump (respawn, observer switch, teleport) so it
    // does not draw a long streak across the map.
    if (buf.count > 0)
    {
        int last = (buf.head - 1 + kMaxPoints) % kMaxPoints;

        if (pos == buf.points[last])
        {
            return;
        }

        if ((pos - buf.points[last]).LengthSqr() > kJumpDist * kJumpDist)
        {
            buf.count = 0;
        }
    }

    buf.points[buf.head] = pos;
    buf.head = (buf.head + 1) % kMaxPoints;

    if (buf.count < kMaxPoints)
    {
        buf.count++;
    }
}

int PlayerTracks::CopyPoints(int player, std::span<ncl_math::Vector2> out) const
{
    const Buf& buf = tracks_[player];
    int count = std::min<int>(buf.count, (int)out.size());

    for (int i = 0; i < count; i++)
    {
        out[i] = buf.points[(buf.head - buf.count + i + kMaxPoints) % kMaxPoints];
    }

    return count;
}
