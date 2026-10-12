#include "PlayerMarkerFade.h"

#include <ncl_math/scalar.h>

#include "hud/radar/layers/players/DeathMarks.h"

PlayerMarkerFade::Visible PlayerMarkerFade::Update(
    ncl_math::Vector3 pos,
    float time,
    bool snapshot,
    std::uint32_t seq,
    float move_tolerance
)
{
    if (time < last_time_ || time - last_time_ >= DeathMarks::kLifetime)
    {
        Reset();
    }

    // A cached position cannot replace a confirmed one without a newer report.
    if (initialized_ && !snapshot && seq == seq_)
    {
        pos = pos_;
    }

    ncl_math::Vector2 move{pos.x - pos_.x, pos.y - pos_.y};
    bool changed = initialized_ && move.LengthSqr() > move_tolerance * move_tolerance && (!snapshot || !snapshot_);

    if ((!initialized_ && !snapshot) || (initialized_ && !visible_) || changed)
    {
        Visible previous = CollectVisible(time);
        previous_pos_ = previous.previous_alpha > previous.alpha ? previous.previous_pos : previous.pos;
        previous_alpha_ = visible_ ? ncl_math::Saturate(previous.alpha + previous.previous_alpha) : 0.0f;
        start_ = time;
        fading_ = true;
    }

    pos_ = pos;
    seq_ = seq;
    last_time_ = time;
    initialized_ = true;
    visible_ = true;
    snapshot_ = snapshot;

    return CollectVisible(time);
}

PlayerMarkerFade::Visible PlayerMarkerFade::CollectVisible(float time) const
{
    float progress = fading_ ? ncl_math::Saturate((time - start_) / kDuration) : 1.0f;
    return {pos_, progress, previous_pos_, previous_alpha_ * (1.0f - progress)};
}

void PlayerMarkerFade::Lose()
{
    visible_ = false;
    fading_ = false;
}

void PlayerMarkerFade::Reset()
{
    *this = PlayerMarkerFade{};
}
