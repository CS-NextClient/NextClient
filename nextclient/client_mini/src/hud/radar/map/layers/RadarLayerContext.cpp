#include "RadarLayerContext.h"

#include <algorithm>
#include <cmath>

#include <goldsrc/event_api.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/pmove.h>

#include "hud/radar/map/Overview.h"
#include "hud/radar/render/RadarView.h"

namespace
{
    constexpr float kTeleportPad = 24.0f;
    constexpr float kMinimumSampleInterval = 0.005f;
    constexpr float kMaximumSampleInterval = 0.1f;
    constexpr float kSupportTraceStart = 2.0f;
    constexpr float kSupportTraceDepth = 64.0f;
    constexpr int kPointHull = 2;
    constexpr float kContactPrecision = 0.001f;

    bool IsFinite(ncl_math::Vector3 point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }
} // namespace

RadarLayerView RadarLayerContext_ComputeView(const RadarMapLayers& map, int active, ncl_math::Vector3 origin, bool enabled)
{
    RadarLayerView view{&map, active, origin, enabled};
    if (enabled && active >= 0 && (size_t)active < map.get_layers().size())
    {
        RadarLayerMatch support = map.Locate(origin, RadarLayerQuery::Nearest, active, active);
        if (support.layer >= 0)
        {
            view.active_support_z = support.support.z;
        }
    }
    return view;
}

RadarLayerSource RadarLayerContext_ChooseSource(int observer_mode, int local_index, int observed_index, bool spectating)
{
    if (!spectating || observer_mode == goldsrc::OBS_NONE)
    {
        return {false, local_index};
    }

    if (RadarView_IsFreeCamera(observer_mode))
    {
        return {true, 0};
    }

    return {false, observed_index};
}

bool RadarLayerContext_IsLayeredMode(RadarMapMode mode)
{
    return mode == RadarMapMode::SliceTextured || mode == RadarMapMode::SliceFlat;
}

RadarMapMode RadarLayerContext_TopMode(RadarMapMode mode)
{
    if (mode == RadarMapMode::SliceTextured)
    {
        return RadarMapMode::RuntimeTextured;
    }

    if (mode == RadarMapMode::SliceFlat)
    {
        return RadarMapMode::RuntimeFlat;
    }

    return mode;
}

ncl_math::Vector3 RadarLayerContext_Feet(ncl_math::Vector3 origin, ncl_math::Vector3 hull_min)
{
    origin.z += hull_min.z;

    return origin;
}

std::optional<RadarLayerContact> RadarLayerContext_TraceSupport(
    goldsrc::event_api_s& events,
    goldsrc::playermove_t& movement,
    ncl_math::Vector3 feet,
    const ncl_math::Aabb3& player_bounds
)
{
    int previous_hull = movement.usehull;

    events.EV_SetUpPlayerPrediction(false, true);
    events.EV_PushPMStates();
    events.EV_SetTraceHull(kPointHull);

    ncl_math::Vector3 start = feet + ncl_math::Vector3{0, 0, kSupportTraceStart};
    ncl_math::Vector3 end = feet - ncl_math::Vector3{0, 0, kSupportTraceDepth};
    goldsrc::pmtrace_t trace{};

    events.EV_PlayerTrace(start.data(), end.data(), goldsrc::PM_STUDIO_IGNORE, -1, &trace);

    std::optional<RadarLayerContact> contact;
    ncl_math::Vector3 normal = ncl_math::Vector3::Ref(trace.plane.normal);

    if (!trace.startsolid && !trace.allsolid && trace.fraction < 1.0f && IsFinite(normal) && normal.z >= kMapSupportNormalZ)
    {
        const goldsrc::physent_t* hit = events.EV_GetPhysent(trace.ent);

        if (hit)
        {
            ncl_math::Vector3 offset = ncl_math::Vector3::Ref(hit->origin);

            if (hit->model)
            {
                offset = offset + ncl_math::Vector3::Ref(hit->model->hulls[0].clip_mins) -
                         ncl_math::Vector3::Ref(movement._player_mins[kPointHull]);
            }

            // PM_PlayerTrace rotates the normal but keeps plane.dist in the hit hull's local space.
            float distance = trace.plane.dist + normal.Dot(offset);
            ncl_math::Vector3 stopped = ncl_math::Vector3::Ref(trace.endpos);
            ncl_math::Vector3 support{stopped.x, stopped.y, (distance - normal.x * stopped.x - normal.y * stopped.y) / normal.z};

            float corner_x = normal.x >= 0 ? player_bounds.min.x : player_bounds.max.x;
            float corner_y = normal.y >= 0 ? player_bounds.min.y : player_bounds.max.y;
            float gap = normal.Dot(feet - support) + normal.x * corner_x + normal.y * corner_y;
            float backoff = normal.Dot(stopped - support);

            if (IsFinite(support) && std::isfinite(gap) && std::isfinite(backoff))
            {
                contact = RadarLayerContact{support, gap >= -kContactPrecision && gap <= backoff + kContactPrecision};
            }
        }
    }

    events.EV_PopPMStates();
    events.EV_SetTraceHull(previous_hull);

    return contact;
}

void RadarLayerContext::Update(const RadarMapLayers& map, RadarLayerObservation observation, float sample_time, float speed)
{
    observation.valid = observation.valid && IsFinite(observation.feet) && std::isfinite(sample_time) && std::isfinite(observation.time);

    if (observation.support && !IsFinite(*observation.support))
    {
        observation.support.reset();
    }

    if (has_previous_)
    {
        if (observation.source == previous_.source && sample_time == previous_sample_time_ && previous_.discontinuity)
        {
            observation.discontinuity = false;
        }

        observation.discontinuity = observation.discontinuity || observation.source != previous_.source ||
                                    observation.time < previous_.time || sample_time < previous_sample_time_;

        if (observation.valid && sample_time > previous_sample_time_ && observation.source == previous_.source)
        {
            float elapsed = std::clamp(sample_time - previous_sample_time_, kMinimumSampleInterval, kMaximumSampleInterval);
            float distance = std::max(speed, previous_speed_) * elapsed + kTeleportPad;

            observation.discontinuity = observation.discontinuity || (observation.feet - previous_.feet).LengthSqr() > distance * distance;
        }
    }

    selection_.Update(map, observation);
    view_ = RadarLayerContext_ComputeView(map, selection_.get_active(), observation.feet, selection_.get_active() >= 0);

    if (observation.valid && (!has_previous_ || sample_time != previous_sample_time_ || observation.source != previous_.source))
    {
        previous_ = observation;
        previous_sample_time_ = sample_time;
        previous_speed_ = speed;
        has_previous_ = true;
    }
}

void RadarLayerContext::Reset()
{
    selection_.Reset();
    view_ = {};

    previous_ = {};
    previous_sample_time_ = 0;
    previous_speed_ = 0;
    has_previous_ = false;
}

const RadarLayerView& RadarLayerContext::get_view() const
{
    return view_;
}

const RadarLayerSelection& RadarLayerContext::get_selection() const
{
    return selection_;
}

void RadarLayerLifecycle::Reset()
{
    geometry_ = RadarMapStatus::Waiting;
    geometry_failure_reason_.clear();

    ResetImage();
}

void RadarLayerLifecycle::ResetImage()
{
    image_ = RadarMapStatus::Waiting;
    image_failure_reason_.clear();
}

bool RadarLayerLifecycle::SetGeometryStatus(RadarMapStatus status, std::string_view failure_reason)
{
    if (geometry_ == RadarMapStatus::Failed)
    {
        return false;
    }

    geometry_ = status;

    if (status == RadarMapStatus::Failed)
    {
        geometry_failure_reason_ = failure_reason.empty() ? "analysis produced no usable support layers" : failure_reason;
    }

    return status == RadarMapStatus::Failed;
}

bool RadarLayerLifecycle::SetImageStatus(RadarMapStatus status, std::string_view failure_reason)
{
    if (image_ == RadarMapStatus::Failed)
    {
        return false;
    }

    image_ = status;

    if (status == RadarMapStatus::Failed)
    {
        image_failure_reason_ = failure_reason.empty() ? "layer image generation failed" : failure_reason;

        return true;
    }

    return false;
}

RadarMapStatus RadarLayerLifecycle::get_geometry_status() const
{
    return geometry_;
}

RadarMapStatus RadarLayerLifecycle::get_image_status() const
{
    return image_;
}

std::string_view RadarLayerLifecycle::get_failure_reason() const
{
    if (geometry_ == RadarMapStatus::Failed)
    {
        return geometry_failure_reason_;
    }

    return image_failure_reason_;
}

bool RadarLayerLifecycle::is_layout_ready() const
{
    return geometry_ == RadarMapStatus::Ready;
}

bool RadarLayerLifecycle::is_image_ready() const
{
    return is_layout_ready() && image_ == RadarMapStatus::Ready;
}

bool RadarLayerLifecycle::should_build_geometry() const
{
    return geometry_ == RadarMapStatus::Waiting;
}

bool RadarLayerLifecycle::should_render_image() const
{
    return is_layout_ready() && image_ != RadarMapStatus::Failed;
}

bool RadarLayerLifecycle::should_use_fallback() const
{
    return geometry_ == RadarMapStatus::Failed || image_ == RadarMapStatus::Failed;
}
