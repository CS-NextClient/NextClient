#pragma once

#include "MapRender.h"

typedef struct model_s model_t;

struct overview_t
{
    float zoom;
    float origin[3];
    int rotated;
    char image[64];
    char map[64];
};

// Values match the hud_radar_map_mode cvar. Overview* fall back to the runtime BSP
// render when the map ships no overview sprite.
enum class RadarMapMode
{
    None = 0,
    OverviewTextured = 1,
    OverviewFlat = 2,
    RuntimeTextured = 3,
    RuntimeFlat = 4,
    SliceTextured = 5,
    SliceFlat = 6,
};

// Never returns None.
RadarMapMode RadarMapMode_FromCvar(float value);

class RadarOverview
{
private:
    overview_t data_{};
    model_t* map_sprite_{};
    RuntimeMap runtime_{};
    char loaded_map_[64]{};
    RadarMapMode loaded_mode_ = RadarMapMode::None;

public:
    void EnsureLoaded(RadarMapMode mode);

    // Call on level (re)load: the engine rebuilds its textures/lightmaps then, which
    // leaves a previously generated runtime texture stale even on the same map.
    void Invalidate();

    bool is_valid() const
    {
        bool result = map_sprite_ != nullptr || runtime_.valid;
        return result;
    }
    const overview_t& get_data() const
    {
        return data_;
    }
    model_t* get_sprite() const
    {
        return map_sprite_;
    }

    bool is_runtime() const
    {
        bool result = map_sprite_ == nullptr && runtime_.valid;
        return result;
    }
    const RuntimeMap& get_runtime_map() const
    {
        return runtime_;
    }

private:
    bool ParseInfo(const char* file_name);

    // Return the position after their block, or null when the file is malformed. data_
    // keeps whatever was read before a failure, so a half-read file must not look loaded.
    char* ParseGlobalBlock(char* parse_pos);
    char* ParseLayerBlock(char* parse_pos);
    bool ParseOverviewBody(char* parse_pos);
};
