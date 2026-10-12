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

void MarkerFeed::ResetMotion(int index)
{
    for (Motion& motion : motions_)
    {
        if (motion.used && motion.index == index)
        {
            motion = Motion{};
            return;
        }
    }
}

void MarkerFeed::ResetMotion()
{
    motions_.fill(Motion{});
    marker_count_ = 0;
    last_time_ = 0.0f;
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

MarkerFeed::Motion* MarkerFeed::AcquireMotion(int index, ncl_math::Vector3 pos, std::uint32_t seq, float time)
{
    for (Motion& motion : motions_)
    {
        if (motion.used && motion.index == index)
        {
            return &motion;
        }
    }

    for (Motion& slot : motions_)
    {
        if (!slot.used)
        {
            slot = {
                .used = true,
                .index = index,
                .seq = seq,
                .from = pos,
                .to = pos,
                .start = time,
                .duration = params_.max_interval,
                .shown = pos,
            };
            return &slot;
        }
    }

    return nullptr;
}

float MarkerFeed::ComputeCorrectionSpeed(const MarkerIdentity::Observation& report) const
{
    float distance = ncl_math::Distance(report.previous_pos, report.pos);
    if (report.interval <= 0.0f || distance == 0.0f || distance > params_.snap_dist)
    {
        return params_.fallback_speed;
    }

    return distance / report.interval;
}

ncl_math::Vector3 MarkerFeed::UpdateSnapshot(ncl_math::Vector3 pos, const MarkerIdentity::Observation& report, float time)
{
    Motion* motion = AcquireMotion(report.index, pos, report.seq, time);

    if (!motion)
    {
        return pos;
    }

    if (!motion->snapshot)
    {
        motion->snapshot_offset = motion->shown - pos;
        if (motion->snapshot_offset.LengthSqr() > params_.snap_dist * params_.snap_dist)
        {
            motion->snapshot_offset = {};
        }
    }
    else if (ncl_math::DistanceSqr(motion->snapshot_pos, pos) > params_.snap_dist * params_.snap_dist)
    {
        motion->snapshot_offset = {};
    }
    else
    {
        float distance = motion->snapshot_offset.Length();
        float step = motion->snapshot_speed * (time - motion->snapshot_time);
        motion->snapshot_offset = distance <= step ? ncl_math::Vector3{} : motion->snapshot_offset * (1.0f - step / distance);
    }

    motion->shown = pos + motion->snapshot_offset;
    motion->snapshot = true;
    motion->snapshot_pos = pos;
    motion->snapshot_time = time;
    // Apply a newly received speed to subsequent frames, without changing distance already covered.
    motion->snapshot_speed = ComputeCorrectionSpeed(report);

    // A report taken here is older than the snapshot, so it only times the segment the next one starts.
    if (motion->seq != report.seq)
    {
        motion->seq = report.seq;
        motion->start = time;
    }

    // Leaving mid-correction must hold the drawn position until a newer report arrives.
    motion->from = motion->shown;
    motion->to = motion->shown;
    return motion->shown;
}

ncl_math::Vector3 MarkerFeed::Interpolate(int index, ncl_math::Vector3 pos, std::uint32_t seq, float time)
{
    Motion* motion = AcquireMotion(index, pos, seq, time);

    if (!motion)
    {
        return pos;
    }

    if (motion->seq != seq)
    {
        float played = ncl_math::Saturate((time - motion->start) / motion->duration);
        ncl_math::Vector3 shown = ncl_math::Lerp(motion->from, motion->to, played);
        bool jumped = ncl_math::DistanceSqr(shown, pos) > params_.snap_dist * params_.snap_dist;

        motion->seq = seq;
        motion->from = jumped ? pos : shown;
        motion->to = pos;
        motion->duration = std::clamp(time - motion->start, params_.min_interval, params_.max_interval);
        motion->start = time;
    }

    float t = ncl_math::Saturate((time - motion->start) / motion->duration);
    motion->shown = ncl_math::Lerp(motion->from, motion->to, t);
    motion->snapshot = false;
    return motion->shown;
}

void MarkerFeed::Update(
    std::span<const MarkerIdentity::Entity> entities,
    std::span<const MarkerIdentity::Observation> observations,
    float time
)
{
    if (time < last_time_)
    {
        ResetMotion();
    }
    last_time_ = time;

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

        int index = get_index(entity.ent);

        if (entity.discontinuity)
        {
            ResetMotion(index);
        }

        auto report = std::ranges::find_if(observations, [index](const MarkerIdentity::Observation& observation) {
            return observation.index == index;
        });
        ncl_math::Vector3 pos = report != observations.end() ? UpdateSnapshot(entity.pos, *report, time) : entity.pos;
        markers_[marker_count_++] = {pos, entity.ent, index};
    }

    for (const MarkerIdentity::Observation& observation : observations)
    {
        int ent = get_entity(observation.index);
        auto carrier =
            std::ranges::find_if(entities, [ent](const MarkerIdentity::Entity& entity) { return ent >= 0 && entity.ent == ent; });

        if (carrier != entities.end())
        {
            continue;
        }

        ncl_math::Vector3 pos = Interpolate(observation.index, observation.pos, observation.seq, time);

        if (marker_count_ >= (int)markers_.size())
        {
            continue;
        }

        markers_[marker_count_++] = {pos, -1, observation.index};
    }
}
