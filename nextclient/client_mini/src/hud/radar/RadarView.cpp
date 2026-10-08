#include "RadarView.h"

#include <goldsrc/pmove.h>
#include <ncl_math/scalar.h>
#include <algorithm>
#include <cmath>

namespace
{
    // Fractions of screen height.
    constexpr float kRadarHeightFrac = 0.20f;
    constexpr float kRadarMarginFrac = 0.02f;
    constexpr float kSpectatorMarginFrac = 0.16f;

    constexpr float kRefResolutionH = 1080.0f;
    constexpr float kHiResDampen = 0.4f; // marker growth above the reference resolution

    // Anchors the follow-mode zoom, so the visible world window does not scale with
    // screen resolution.
    constexpr float kRefBoxSize = kRefResolutionH * kRadarHeightFrac;

    constexpr float kWorldSpan = 8192.0f; // GoldSrc world extent (-4096..4096)
    constexpr float kOverviewTexelScale = 1024.0f; // overview texels per world unit at zoom 1
    constexpr float kScreenAspect = 4.0f / 3.0f;
    constexpr float kFollowZoomBase = 20.0f;
    constexpr int kOverviewTileCols = 4; // overview sprite tile grid, 4x3 frames per zoom level
    constexpr int kOverviewTileRows = 3;
    constexpr int kOverviewFramesPerLevel = kOverviewTileCols * kOverviewTileRows;

    // A round radar clips the corners of the full-map rectangle; the half-diagonal
    // ratio zooms out just enough to fit the whole map inside the disc.
    const float kRoundFullmapZoomOut = std::sqrt(1.0f + 1.0f / (kScreenAspect * kScreenAspect));

    // Radar centre in tile steps from the overview grid origin.
    constexpr ncl_math::Vector2 kCenterInTiles{3.0f, 4.0f};
} // namespace

bool RadarView_IsFreeCamera(int observer_mode)
{
    return observer_mode == goldsrc::OBS_ROAMING;
}

bool RadarView_ShouldHighlightPlayer(int observer_mode)
{
    return observer_mode == goldsrc::OBS_NONE || observer_mode == goldsrc::OBS_IN_EYE;
}

RadarBox RadarView_ComputeBox(float screen_w, float screen_h, float scale, bool spectating)
{
    // The spectator letterbox height is a stock resource value (resource/UI/Spectator.res,
    // and czero ships a different one), so reading it belongs to the caller; this module
    // stays engine-free.
    float margin_y = spectating ? screen_h * kSpectatorMarginFrac : screen_h * kRadarMarginFrac;

    RadarBox box;
    box.size = screen_h * kRadarHeightFrac * scale;
    box.x = screen_h * kRadarMarginFrac;
    box.y = margin_y;

    float damped = (screen_h <= kRefResolutionH) ? screen_h : ncl_math::Lerp(kRefResolutionH, screen_h, kHiResDampen);
    box.marker_px = damped * kRadarHeightFrac * scale;
    box.screen_w = screen_w;
    box.screen_h = screen_h;
    return box;
}

RadarTransform RadarView_ComputeTransform(const RadarBox& box, const RadarViewParams& p)
{
    ncl_math::Vector2 center = p.fullmap ? p.ov_origin : p.view_origin;
    float viewzoom = p.fullmap 
        ? (kWorldSpan / box.size) / p.map_scale
        : (kFollowZoomBase / p.map_scale) * (kRefBoxSize / box.size);

    if (p.fullmap && p.round)
    {
        viewzoom *= kRoundFullmapZoomOut;
    }

    float yaw = (p.rotate && !p.fullmap) ? p.view_yaw : 0.0f;
    float angles = (yaw + 90.0f) * ncl_math::kDeg2Rad;

    float k = p.ov_zoom / kOverviewTexelScale;
    ncl_math::Vector2 offset = (p.ov_origin - center) * k;

    if (p.ov_rotated)
    {
        angles -= ncl_math::kPi / 2.0f;
        offset = -offset.Perp();
    }

    ncl_math::Vector2 center_tile = kCenterInTiles + offset;

    int total_frames = p.sprite_frames > 0 ? p.sprite_frames : kOverviewFramesPerLevel;
    int tiles = (int)std::sqrt((float)(total_frames / kOverviewFramesPerLevel));

    if (tiles < 1)
    {
        tiles = 1;
    }

    int x_tiles = tiles * kOverviewTileCols;
    int y_tiles = tiles * kOverviewTileRows;

    float x_step = (kWorldSpan / viewzoom) / x_tiles;
    float y_step = -(kWorldSpan / (viewzoom * kScreenAspect)) / y_tiles;

    RadarTransform t;
    t.center = box.center();
    t.k = k;
    t.world_center = center;
    t.total_frames = total_frames;
    t.x_tiles = x_tiles;
    t.y_tiles = y_tiles;
    ncl_math::Vector2 basis = ncl_math::FromAngle(angles);
    t.up = basis.Perp() * y_step;
    t.right = basis * x_step;
    t.tile_origin = t.center - t.right * center_tile.x - t.up * center_tile.y;

    // The overview "rotated" mode swaps the axes and flips a sign; folding both variants
    // into the basis columns here keeps WorldToScreen/MarkerDirection branch-free.
    t.projection.origin = t.center;

    if (p.ov_rotated)
    {
        t.projection.basis_x = t.up;
        t.projection.basis_y = -t.right;
    }
    else
    {
        t.projection.basis_x = -t.right;
        t.projection.basis_y = -t.up;
    }

    return t;
}

ncl_math::Vector2 RadarTransform::WorldToScreen(ncl_math::Vector2 world) const
{
    return projection.Apply((world - world_center) * k);
}

ncl_math::Vector2 RadarTransform::MarkerDirection(float facing_yaw) const
{
    ncl_math::Vector2 fwd = ncl_math::FromAngle(facing_yaw * ncl_math::kDeg2Rad);

    return projection.ApplyVector(fwd * k);
}

ncl_math::Vector2 RadarShape::Clamp(int margin, ncl_math::Vector2 p) const
{
    float half = radius - margin;

    if (round)
    {
        ncl_math::Vector2 o = p - center;
        float dist = o.Length();

        if (dist > half && dist > 0.0f)
        {
            return center + o / dist * half;
        }

        return p;
    }

    return {std::clamp(p.x, center.x - half, center.x + half), std::clamp(p.y, center.y - half, center.y + half)};
}
