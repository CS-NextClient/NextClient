#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <goldsrc/fwd.h>
#include <next_client_mini/radar_cvars.h>

#include "hud/radar/map/MapRender.h"

struct overview_t
{
    float zoom;
    float origin[3];
    int rotated;
    char image[64];
    char map[64];
};

class RadarOverview
{
private:
    overview_t data_{};
    goldsrc::model_t* map_sprite_{};
    RuntimeMap runtime_{};

    MapLayerRenderer layer_renderer_;
    std::vector<MapLayerMaterial> layer_materials_;
    const RuntimeMap* layer_image_{};
    bool layer_layout_ready_{};
    bool layer_gl_failed_{};
    std::string layer_waiting_reason_;

    bool runtime_layout_ready_{};
    bool runtime_failed_{};
    bool runtime_layers_ready_{};

    char loaded_map_[64]{};
    RadarMapMode loaded_mode_ = RadarMapMode::None;

public:
    ~RadarOverview();

    RadarMapStatus EnsureLoaded(RadarMapMode mode, const RadarMapLayers* layers, bool prepare_only);
    RadarMapStatus EnsureLayerImage(const RadarMapLayers& layers, int layer, uint64_t frame);
    RadarMapStatus EnsureTopImage(RadarMapMode mode, bool prepare_only, const RadarMapLayers* layers = nullptr);

    void ResetLayerImages();

    // Call on level (re)load: the engine rebuilds its textures/lightmaps then, which
    // leaves a previously generated runtime texture stale even on the same map.
    void Invalidate();

    bool is_valid() const
    {
        bool result = map_sprite_ != nullptr || runtime_.valid || layer_layout_ready_ || runtime_layout_ready_;

        return result;
    }

    const MapLayerCache& get_layer_cache() const
    {
        return layer_renderer_.get_cache();
    }

    std::string_view get_layer_failure_reason() const
    {
        if (layer_gl_failed_)
        {
            return "OpenGL initialization failed";
        }

        return layer_renderer_.get_failure_reason();
    }

    const std::string& get_layer_waiting_reason() const
    {
        return layer_waiting_reason_;
    }

    const overview_t& get_data() const
    {
        return data_;
    }

    goldsrc::model_t* get_sprite() const
    {
        return map_sprite_;
    }

    bool is_runtime() const
    {
        bool result = map_sprite_ == nullptr && (runtime_.valid || layer_image_ != nullptr);

        return result;
    }

    const RuntimeMap& get_runtime_map() const
    {
        return layer_image_ ? *layer_image_ : runtime_;
    }

private:
    bool ParseInfo(const char* file_name);

    // Return the position after their block, or null when the file is malformed. data_
    // keeps whatever was read before a failure, so a half-read file must not look loaded.
    char* ParseGlobalBlock(char* parse_pos);
    char* ParseLayerBlock(char* parse_pos);
    bool ParseOverviewBody(char* parse_pos);
};
