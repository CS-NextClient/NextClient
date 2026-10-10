#pragma once

#include <optional>

#include "hud/radar/map/layers/RadarMapLayers.h"
#include "hud/radar/render/RadarView.h"

struct RadarLayerStyle
{
    float opacity = 1.0f;
    int vertical{};
};

struct RadarLayerPlacement
{
    ncl_math::Vector2 marker{};
    ncl_math::Vector2 indicator{};
    bool offscreen{};
};

struct RadarPlayerPlacement
{
    RadarLayerPlacement placement{};
    ncl_math::Vector2 out_dir{};
};

namespace RadarPlayerMarkerGeometry
{
    inline constexpr float kTipScale = 1.7f;
    inline constexpr float kBaseScale = 0.8f;
    inline constexpr float kHalfWidthScale = 1.2f;
} // namespace RadarPlayerMarkerGeometry

class RadarSiteLayerCache
{
private:
    struct Volume
    {
        ncl_math::Aabb3 bounds;
        std::vector<std::pair<int, float>> supports;
        bool fallback{};
    };

    const RadarMapLayers* map_{};
    std::vector<Volume> volumes_;
    int active_ = -1;

public:
    void Reset();
    void Prepare(const RadarMapLayers& map, std::span<const ncl_math::Aabb3> volumes, int active);
    RadarLayerStyle ComputeStyle(const RadarLayerView& view) const;
};

RadarLayerStyle RadarLayerMarker_ComputeStyle(const RadarLayerView& view, std::optional<ncl_math::Vector3> position);
RadarLayerStyle RadarLayerMarker_ComputeVolumeStyle(const RadarLayerView& view, std::span<const ncl_math::Aabb3> volumes);

RadarLayerPlacement RadarLayerMarker_Place(
    const RadarShape& shape,
    ncl_math::Vector2 raw,
    int size,
    const RadarLayerStyle& style,
    std::optional<int> indicator_size = {},
    std::optional<float> marker_half_height = {}
);

RadarPlayerPlacement RadarLayerMarker_PlacePlayer(
    const RadarShape& shape,
    ncl_math::Vector2 raw,
    ncl_math::Vector2 direction,
    int size,
    int extent,
    const RadarLayerStyle& style,
    std::optional<int> indicator_size = {},
    std::optional<float> marker_half_height = {}
);

float RadarLayerMarker_PlayerHalfHeight(ncl_math::Vector2 direction, int size, int vertical, bool offscreen);

bool RadarLayerMarker_IsTrailSegmentVisible(const RadarLayerView& view, int first, int second);
