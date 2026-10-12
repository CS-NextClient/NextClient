#include "RadarLayerMarker.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace
{
    constexpr float kOtherFloorOpacity = 0.4f;
    constexpr float kIndicatorGapPx = 1.0f;

    RadarLayerStyle StyleForLayer(const RadarLayerView& view, int layer, float height)
    {
        if (layer < 0 || layer == view.active)
        {
            return {};
        }

        float difference = height - *view.active_support_z;
        int vertical = 0;

        if (difference > 0.0f)
        {
            vertical = 1;
        }
        else if (difference < 0.0f)
        {
            vertical = -1;
        }

        return {kOtherFloorOpacity, vertical};
    }

    bool IsLayerViewValid(const RadarLayerView& view)
    {
        return view.enabled && view.map && view.active_support_z.has_value();
    }

    RadarLayerPlacement PlaceMarker(
        const RadarShape& shape,
        ncl_math::Vector2 raw,
        int size,
        const RadarLayerStyle& style,
        std::optional<int> indicator_size,
        std::optional<float> marker_half_height,
        bool offscreen
    )
    {
        int arrow_size = indicator_size.value_or(size);
        float offset = 0.0f;

        if (style.vertical != 0)
        {
            offset = marker_half_height.value_or((float)size) + arrow_size * 0.5f + kIndicatorGapPx;
        }

        float inset = style.vertical == 0 ? (float)size : std::max((float)size, offset + arrow_size);
        ncl_math::Vector2 marker = shape.Clamp((int)std::ceil(inset), raw);

        return {marker, marker + ncl_math::Vector2{0.0f, -style.vertical * offset}, offscreen};
    }

} // namespace

RadarLayerStyle RadarLayerMarker_ComputeStyle(const RadarLayerView& view, std::optional<ncl_math::Vector3> position)
{
    if (!IsLayerViewValid(view) || !position)
    {
        return {};
    }

    RadarLayerMatch match = view.map->Locate(*position, RadarLayerQuery::Below, view.active);

    if (match.layer < 0)
    {
        match = view.map->Locate(*position, RadarLayerQuery::Nearest, view.active);
    }

    return StyleForLayer(view, match.layer, match.support.z);
}

void RadarSiteLayerCache::Reset()
{
    map_ = nullptr;
    volumes_.clear();
    active_ = -1;
}

void RadarSiteLayerCache::Prepare(const RadarMapLayers& map, std::span<const ncl_math::Aabb3> volumes, int active)
{
    bool rebuild = map_ != &map || volumes_.size() != volumes.size() ||
                   !std::equal(volumes_.begin(), volumes_.end(), volumes.begin(), [](const Volume& cached, const ncl_math::Aabb3& volume) {
                       return cached.bounds.min == volume.min && cached.bounds.max == volume.max;
                   });
    if (!rebuild && active_ == active)
    {
        return;
    }

    if (rebuild)
    {
        volumes_.clear();
        volumes_.reserve(volumes.size());
        std::vector<int> assigned;
        for (const ncl_math::Aabb3& bounds : volumes)
        {
            map.Intersect(bounds, assigned);
            Volume volume{bounds, {}, assigned.empty()};
            for (int layer : assigned)
            {
                RadarLayerMatch support = map.Locate(bounds.Center(), RadarLayerQuery::Nearest, layer, layer);
                volume.supports.emplace_back(layer, support.support.z);
            }

            volumes_.push_back(std::move(volume));
        }

        map_ = &map;
    }

    for (Volume& volume : volumes_)
    {
        if (!volume.fallback)
        {
            continue;
        }

        volume.supports.clear();
        RadarLayerMatch match = map.Locate(volume.bounds.Center(), RadarLayerQuery::Below, active);
        if (match.layer < 0)
        {
            match = map.Locate(volume.bounds.Center(), RadarLayerQuery::Nearest, active);
        }

        if (match.layer >= 0)
        {
            RadarLayerMatch support = map.Locate(volume.bounds.Center(), RadarLayerQuery::Nearest, match.layer, match.layer);
            volume.supports.emplace_back(match.layer, support.support.z);
        }
    }

    active_ = active;
}

RadarLayerStyle RadarSiteLayerCache::ComputeStyle(const RadarLayerView& view) const
{
    if (!IsLayerViewValid(view) || view.map != map_)
    {
        return {};
    }

    int nearest = -1;
    float nearest_height = 0.0f;
    float distance = std::numeric_limits<float>::max();
    for (const Volume& volume : volumes_)
    {
        for (const auto& [layer, height] : volume.supports)
        {
            if (layer == view.active)
            {
                return {};
            }

            float candidate = std::abs(height - *view.active_support_z);
            if (std::tuple{candidate, layer} < std::tuple{distance, nearest})
            {
                nearest = layer;
                nearest_height = height;
                distance = candidate;
            }
        }
    }

    return StyleForLayer(view, nearest, nearest_height);
}

RadarLayerStyle RadarLayerMarker_ComputeVolumeStyle(const RadarLayerView& view, std::span<const ncl_math::Aabb3> volumes)
{
    if (!IsLayerViewValid(view))
    {
        return {};
    }

    RadarSiteLayerCache cache;
    cache.Prepare(*view.map, volumes, view.active);
    return cache.ComputeStyle(view);
}

RadarLayerPlacement RadarLayerMarker_Place(
    const RadarShape& shape,
    ncl_math::Vector2 raw,
    int size,
    const RadarLayerStyle& style,
    std::optional<int> indicator_size,
    std::optional<float> marker_half_height
)
{
    return PlaceMarker(
        shape,
        raw,
        size,
        style,
        indicator_size,
        marker_half_height,
        !shape.Contains(raw)
    );
}

RadarPlayerPlacement RadarLayerMarker_PlacePlayer(
    const RadarShape& shape,
    ncl_math::Vector2 raw,
    ncl_math::Vector2 direction,
    int size,
    int extent,
    const RadarLayerStyle& style,
    std::optional<int> indicator_size,
    std::optional<float> marker_half_height
)
{
    bool offscreen = !shape.Contains(raw);
    ncl_math::Vector2 out_dir{};

    if (offscreen)
    {
        ncl_math::TryNormalize(raw - shape.center, &out_dir);
    }

    if (!marker_half_height)
    {
        marker_half_height = RadarLayerMarker_PlayerHalfHeight(
            offscreen ? out_dir : direction,
            size,
            style.vertical,
            offscreen
        );
    }

    return {
        PlaceMarker(
            shape,
            raw,
            extent,
            style,
            indicator_size.value_or(size),
            marker_half_height,
            offscreen
        ),
        out_dir
    };
}

float RadarLayerMarker_PlayerHalfHeight(ncl_math::Vector2 direction, int size, int vertical, bool offscreen)
{
    ncl_math::Vector2 normalized{};

    if (!ncl_math::TryNormalize(direction, &normalized))
    {
        return (float)size;
    }

    if (offscreen)
    {
        float tip = -vertical * normalized.y * RadarPlayerMarkerGeometry::kTipScale;
        float base = vertical * normalized.y * RadarPlayerMarkerGeometry::kBaseScale +
                     std::abs(normalized.x) * RadarPlayerMarkerGeometry::kHalfWidthScale;

        return size * std::max(tip, base);
    }

    return size * std::max(1.0f, -vertical * normalized.y * 2.0f);
}

bool RadarLayerMarker_IsTrailSegmentVisible(const RadarLayerView& view, int first, int second)
{
    return !view.enabled || (first == view.active && second == view.active);
}
