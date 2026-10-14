#pragma once

#include <ncl_math/vec3.h>
#include <goldsrc/engine/model.h>

#include "hud/radar/map/Overview.h"
#include "hud/radar/RadarFrame.h"

class SmokeRegistry;

// Screen-space corners of one overview tile, in the winding the sprite expects.
struct TileCorners
{
    ncl_math::Vector2 base;
    ncl_math::Vector2 right;
    ncl_math::Vector2 up;
    ncl_math::Vector2 far_corner;
};

// Everything under the markers: the darkened box, the map, the smoke clouds and the rim.
class BackdropLayer
{
    SmokeRegistry& smoke_;

    RadarOverview overview_;

    // Wrap state persists per texture object, so edge clamp is set once per loaded sprite,
    // not per tile.
    goldsrc::model_t* clamped_sprite_{};

public:
    explicit BackdropLayer(SmokeRegistry& smoke);

    RadarMapStatus EnsureMap(RadarMapMode mode, const RadarMapLayers* layers, bool prepare_only = false);
    RadarMapStatus EnsureLayerImage(const RadarMapLayers& layers, int layer, uint64_t frame);
    RadarMapStatus EnsureTopMap(RadarMapMode mode, bool prepare_only = false, const RadarMapLayers* layers = nullptr);
    void ResetLayerImages();
    void Draw(const RadarFrame& frame);

    void OnVidInit();

    const RadarOverview& get_overview() const
    {
        return overview_;
    }

private:
    void DrawBackground(const RadarFrame& frame);
    void DrawRuntimeMap(const RadarFrame& frame);
    void DrawOverviewTiles(const RadarFrame& frame);
    void EnsureSpriteClamped(goldsrc::model_t* map, int total_frames);
    void DrawOverviewTile(const RadarBox& box, const TileCorners& corners, bool masked);
    void DrawSmoke(const RadarFrame& frame);
    void DrawBorder(const RadarFrame& frame);
};
