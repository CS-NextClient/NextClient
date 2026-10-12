#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include <ncl_math/aabb.h>
#include <goldsrc/fwd.h>

#include "hud/radar/map/layers/MapLayerCache.h"
#include "hud/radar/map/layers/RadarMapLayers.h"

inline constexpr int kMapImageSize = 1024;

inline constexpr float kMapFlatMinBrightness = 0.16f;
inline constexpr float kMapFlatMaxBrightness = 0.55f;

struct RuntimeMap
{
    unsigned int texture{};
    ncl_math::Aabb2 bounds; // world XY covered by the texture
    bool valid{};
};

RuntimeMap MapRender_Generate(
    goldsrc::model_t* world,
    bool textured,
    bool slice,
    RadarMapStatus* status = nullptr,
    std::span<const int> excluded_surfaces = std::span<const int>{}
);
void MapRender_Free(RuntimeMap& map);

struct MapLayerMaterial
{
    unsigned int base_texture{};
    unsigned int lightmap_texture{};
    bool needs_lightmap{};
};

class MapLayerRenderer
{
private:
    MapLayerCache cache_;
    std::array<RuntimeMap, kMapLayerImageLimit> maps_{};

    unsigned int framebuffer_{};
    unsigned int depth_buffer_{};

    const RadarMapLayers* layers_{};
    bool textured_{};
    bool failed_{};
    std::string failure_reason_;

public:
    MapLayerRenderer() = default;
    ~MapLayerRenderer();

    MapLayerRenderer(const MapLayerRenderer&) = delete;
    MapLayerRenderer& operator=(const MapLayerRenderer&) = delete;

    RadarMapStatus Ensure(
        const RadarMapLayers& layers,
        std::span<const MapLayerMaterial> materials,
        int layer,
        bool textured,
        uint64_t frame,
        std::string* waiting_reason = nullptr
    );

    bool is_cached(const RadarMapLayers& layers, int layer, bool textured) const;
    const RuntimeMap* get_map(int layer) const;
    const MapLayerCache& get_cache() const;
    const std::string& get_failure_reason() const;

    void Reset();

private:
    RadarMapStatus Fail(const RadarMapLayers& layers, bool textured, std::string reason);
};
