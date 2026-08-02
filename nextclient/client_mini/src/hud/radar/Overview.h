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

// Source used to draw the radar map. Values match the hud_radar_map_mode cvar.
// Overview* prefer the shipped overview sprite and fall back to a runtime BSP
// render; Runtime*/Slice* always render the BSP. Textured keeps world textures,
// Flat is flat-shaded; Slice keeps only the play-level floor.
enum class RadarMapMode
{
    None = 0, // nothing loaded yet
    OverviewTextured = 1,
    OverviewFlat = 2,
    RuntimeTextured = 3,
    RuntimeFlat = 4,
    SliceTextured = 5,
    SliceFlat = 6,
};

// Clamps a raw hud_radar_map_mode cvar value into a valid mode (never None).
RadarMapMode RadarMapMode_FromCvar(float value);

// Parses overviews/<map>.txt and loads the overview bitmap as a tiled map sprite.
// On maps that ship no overview a top-down image of the BSP is generated at runtime
// and used instead. Reloads automatically when the level changes.
class RadarOverview
{
private:
    overview_t data_{};
    model_t* map_sprite_ = nullptr;
    RuntimeMap runtime_{};
    char loaded_map_[64]{};
    RadarMapMode loaded_mode_ = RadarMapMode::None;

public:
    void EnsureLoaded(RadarMapMode mode);

    // Drops the loaded map/runtime image so the next EnsureLoaded rebuilds it. Call
    // on level (re)load: the engine rebuilds its textures/lightmaps then, which
    // leaves a previously generated runtime texture stale even on the same map.
    void Invalidate();

    bool valid() const
    {
        bool result = map_sprite_ != nullptr || runtime_.valid;
        return result;
    }
    const overview_t& data() const
    {
        return data_;
    }
    model_t* sprite() const
    {
        return map_sprite_;
    }

    bool runtime() const
    {
        bool result = map_sprite_ == nullptr && runtime_.valid;
        return result;
    }
    const RuntimeMap& runtime_map() const
    {
        return runtime_;
    }

private:
    bool ParseInfo(const char* file_name);
};
