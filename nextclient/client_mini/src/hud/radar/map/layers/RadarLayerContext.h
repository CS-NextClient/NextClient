#pragma once

#include <string>
#include <string_view>

#include "hud/radar/map/layers/RadarMapLayers.h"

enum class RadarMapMode;

namespace goldsrc
{
    struct event_api_s;
    struct playermove_t;
} // namespace goldsrc

struct RadarLayerSource
{
    bool camera{};
    int entity{};
};

RadarLayerView RadarLayerContext_ComputeView(const RadarMapLayers& map, int active, ncl_math::Vector3 origin, bool enabled);

RadarLayerSource RadarLayerContext_ChooseSource(int observer_mode, int local_index, int observed_index, bool spectating);
bool RadarLayerContext_IsLayeredMode(RadarMapMode mode);
RadarMapMode RadarLayerContext_TopMode(RadarMapMode mode);

ncl_math::Vector3 RadarLayerContext_Feet(ncl_math::Vector3 origin, ncl_math::Vector3 hull_min);

struct RadarLayerContact
{
    ncl_math::Vector3 support{};
    bool grounded{};
};

std::optional<RadarLayerContact> RadarLayerContext_TraceSupport(
    goldsrc::event_api_s& events,
    goldsrc::playermove_t& movement,
    ncl_math::Vector3 feet,
    const ncl_math::Aabb3& player_bounds
);

class RadarLayerContext
{
private:
    RadarLayerSelection selection_;
    RadarLayerView view_{};
    RadarLayerObservation previous_{};
    float previous_sample_time_{};
    float previous_speed_{};
    bool has_previous_{};

public:
    void Update(const RadarMapLayers& map, RadarLayerObservation observation, float sample_time, float speed);
    void Reset();

    const RadarLayerView& get_view() const;
    const RadarLayerSelection& get_selection() const;
};

class RadarLayerLifecycle
{
private:
    RadarMapStatus geometry_ = RadarMapStatus::Waiting;
    RadarMapStatus image_ = RadarMapStatus::Waiting;
    std::string geometry_failure_reason_;
    std::string image_failure_reason_;

public:
    void Reset();
    void ResetImage();

    // Status updates return true only when a failure is first latched.
    bool SetGeometryStatus(RadarMapStatus status, std::string_view failure_reason = {});
    bool SetImageStatus(RadarMapStatus status, std::string_view failure_reason = {});

    RadarMapStatus get_geometry_status() const;
    RadarMapStatus get_image_status() const;
    std::string_view get_failure_reason() const;

    bool is_layout_ready() const;
    bool is_image_ready() const;

    bool should_build_geometry() const;
    bool should_render_image() const;
    bool should_use_fallback() const;
};
