#include "MarkerFeed.h"

#include <algorithm>

MarkerFeed::MarkerFeed(const Params& params) :
    params_(params),
    identity_(params.identity)
{
}

void MarkerFeed::Reset()
{
    identity_.Reset();
    ResetMotion();
}

void MarkerFeed::ResetMotion()
{
    motions_.fill(Motion{});
    marker_count_ = 0;
}

int MarkerFeed::get_entity(int index) const
{
    if (params_.index_is_entity)
    {
        return index;
    }

    return identity_.get_entity(index);
}

int MarkerFeed::get_index(int ent) const
{
    if (params_.index_is_entity)
    {
        return ent;
    }

    return identity_.get_index(ent);
}

ncl_math::Vector2 MarkerFeed::Interpolate(int index, ncl_math::Vector2 pos, std::uint32_t seq, float time)
{
    Motion* motion = nullptr;

    for (Motion& candidate : motions_)
    {
        if (candidate.used && candidate.index == index)
        {
            motion = &candidate;
            break;
        }
    }

    if (!motion)
    {
        for (Motion& slot : motions_)
        {
            if (!slot.used)
            {
                slot = {true, index, seq, pos, pos, time, params_.max_interval};
                break;
            }
        }

        return pos;
    }

    if (motion->seq != seq)
    {
        float played = std::clamp((time - motion->start) / motion->duration, 0.0f, 1.0f);
        ncl_math::Vector2 shown = ncl_math::Lerp(motion->from, motion->to, played);
        bool jumped = ncl_math::DistanceSqr(shown, pos) > params_.snap_dist * params_.snap_dist;

        motion->seq = seq;
        motion->from = jumped ? pos : shown;
        motion->to = pos;
        motion->duration = std::clamp(time - motion->start, params_.min_interval, params_.max_interval);
        motion->start = time;
    }

    float t = std::clamp((time - motion->start) / motion->duration, 0.0f, 1.0f);
    return ncl_math::Lerp(motion->from, motion->to, t);
}

void MarkerFeed::Update(
    std::span<const MarkerIdentity::Entity> entities,
    std::span<const MarkerIdentity::Observation> observations,
    float time
)
{
    if (!params_.index_is_entity)
    {
        identity_.Update(entities, observations);
    }

    marker_count_ = 0;

    for (const MarkerIdentity::Entity& entity : entities)
    {
        if (marker_count_ >= (int)markers_.size())
        {
            break;
        }

        markers_[marker_count_++] = {entity.pos, entity.ent, get_index(entity.ent)};
    }

    for (const MarkerIdentity::Observation& observation : observations)
    {
        // Played on even while the snapshot covers the thing, so a marker that comes back resumes
        // from where it was last reported.
        ncl_math::Vector2 pos = Interpolate(observation.index, observation.pos, observation.seq, time);

        int ent = get_entity(observation.index);
        bool carried = std::ranges::any_of(entities, [ent](const MarkerIdentity::Entity& entity) {
            return ent >= 0 && entity.ent == ent;
        });

        if (carried || marker_count_ >= (int)markers_.size())
        {
            continue;
        }

        markers_[marker_count_++] = {pos, -1, observation.index};
    }
}
