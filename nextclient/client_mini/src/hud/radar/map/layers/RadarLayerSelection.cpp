#include "RadarMapLayers.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr float kConfirmationTime = 0.15f;
    constexpr float kTransitionMargin = 16.0f;
    constexpr float kSupportTolerance = 0.001f;

    const RadarLayerInfo* FindLayer(const RadarMapLayers& map, int id)
    {
        for (const RadarLayerInfo& layer : map.get_layers())
        {
            if (layer.id == id)
            {
                return &layer;
            }
        }

        return nullptr;
    }
} // namespace

int RadarLayerSelection::Update(const RadarMapLayers& map, const RadarLayerObservation& observation)
{
    bool discontinuity = reselect_ || observation.discontinuity ||
                         (has_previous_ && (observation.source != previous_.source || observation.time < previous_.time));

    if (!observation.valid)
    {
        reselect_ = discontinuity;

        return Hold(RadarLayerReason::Invalid);
    }

    if (active_ >= 0 && !discontinuity && !observation.free_camera && observation.grounded && !observation.support)
    {
        return Hold(RadarLayerReason::Invalid);
    }

    bool landing = has_previous_ && !previous_.grounded && !previous_.free_camera && observation.grounded && !observation.free_camera;

    reselect_ = false;
    previous_ = observation;
    has_previous_ = true;

    ncl_math::Vector3 position =
        observation.grounded && !observation.free_camera ? observation.support.value_or(observation.feet) : observation.feet;

    if (active_ < 0 || discontinuity)
    {
        RadarLayerMatch match = map.Locate(position, RadarLayerQuery::Below, active_);
        RadarLayerReason reason = discontinuity ? RadarLayerReason::Discontinuity : RadarLayerReason::Initial;

        if (match.layer < 0)
        {
            match = map.Locate(position, RadarLayerQuery::Nearest, active_);

            if (!discontinuity)
            {
                reason = RadarLayerReason::Nearest;
            }
        }

        return Select(match.layer, reason);
    }

    RadarLayerMatch match =
        map.Locate(position, observation.grounded && !observation.free_camera ? RadarLayerQuery::Nearest : RadarLayerQuery::Below, active_);
    bool nearest = observation.free_camera && match.layer < 0;

    if (nearest)
    {
        match = map.Locate(position, RadarLayerQuery::Nearest, active_);
    }

    RadarLayerReason unchanged = RadarLayerReason::Airborne;

    if (nearest)
    {
        unchanged = RadarLayerReason::Nearest;
    }
    else if (observation.grounded || observation.free_camera)
    {
        unchanged = RadarLayerReason::Unchanged;
    }

    if (match.layer == active_)
    {
        return Hold(unchanged);
    }

    const RadarLayerInfo* active = FindLayer(map, active_);
    const RadarLayerInfo* target = FindLayer(map, match.layer);

    if (!active || !target)
    {
        return Hold(!observation.grounded && !observation.free_camera ? RadarLayerReason::Airborne : RadarLayerReason::Invalid);
    }

    bool exact_support = observation.support && match.contained &&
                         (match.support - *observation.support).LengthSqr() <= kSupportTolerance * kSupportTolerance;

    if (landing && exact_support)
    {
        return Select(match.layer, RadarLayerReason::Grounded);
    }

    RadarLayerMatch active_support = map.Locate(position, RadarLayerQuery::Nearest, active_, active_);
    float active_height = active_support.support.z;
    float target_height = match.support.z;

    if (!observation.grounded && !observation.free_camera && target_height >= active_height)
    {
        return Hold(RadarLayerReason::Airborne);
    }

    float gap = target_height - active_height;
    float margin = std::min(kTransitionMargin, std::abs(gap) * 0.25f);
    float boundary = (target_height + active_height) * 0.5f;
    float transition_distance = std::numeric_limits<float>::max();

    for (const RadarLayerTransition& transition : map.get_analysis().transitions)
    {
        if ((transition.lower != active_ || transition.upper != match.layer) &&
            (transition.upper != active_ || transition.lower != match.layer))
        {
            continue;
        }

        if (std::find(transition.surface_ids.begin(), transition.surface_ids.end(), match.surface_id) == transition.surface_ids.end())
        {
            continue;
        }

        float distance = std::abs(position.z - transition.height);

        if (distance < transition_distance)
        {
            transition_distance = distance;
            boundary = transition.height;

            float span = transition.height_span > 0.0f ? transition.height_span : std::abs(target->height - active->height);
            margin = std::min(kTransitionMargin, span * 0.25f);
        }
    }

    bool crossed = false;

    if (gap == 0.0f)
    {
        crossed = exact_support || observation.free_camera;
    }
    else if (gap > 0.0f)
    {
        crossed = position.z > boundary + margin;
    }
    else
    {
        crossed = position.z < boundary - margin;
    }

    if (!crossed)
    {
        return Hold(unchanged);
    }

    reason_ = RadarLayerReason::Pending;

    if (candidate_ != match.layer)
    {
        candidate_ = match.layer;
        candidate_since_ = observation.time;
    }

    if (observation.time >= candidate_since_ + kConfirmationTime)
    {
        return Select(candidate_, RadarLayerReason::Confirmed);
    }

    return active_;
}

int RadarLayerSelection::Hold(RadarLayerReason reason)
{
    candidate_ = -1;
    candidate_since_ = 0.0f;
    reason_ = reason;

    return active_;
}

int RadarLayerSelection::Select(int layer, RadarLayerReason reason)
{
    active_ = layer;

    return Hold(reason);
}

void RadarLayerSelection::Reset()
{
    previous_ = {};
    has_previous_ = false;
    reselect_ = false;

    Select(-1, RadarLayerReason::Initial);
}

int RadarLayerSelection::get_active() const
{
    return active_;
}

int RadarLayerSelection::get_candidate() const
{
    return candidate_;
}

float RadarLayerSelection::get_candidate_since() const
{
    return candidate_since_;
}

int RadarLayerSelection::get_source() const
{
    return has_previous_ ? previous_.source : -1;
}

RadarLayerReason RadarLayerSelection::get_reason() const
{
    return reason_;
}
