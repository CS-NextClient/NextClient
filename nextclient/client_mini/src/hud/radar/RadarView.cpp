#include "RadarView.h"

#include <ncl_math/scalar.h>
#include <algorithm>
#include <cmath>

namespace
{
    // Radar box geometry, expressed as fractions of the screen height.
    constexpr float kRadarHeightFrac = 0.20f; // box diameter relative to screen height
    constexpr float kRadarMarginFrac = 0.02f; // inset from the screen edge
    constexpr float kSpectatorMarginFrac = 0.16f; // pushed down below the spectator letterbox
    constexpr float kRefResolutionH = 1080.0f; // pivot for resolution-independent zoom / marker damping
    constexpr float kHiResDampen = 0.4f; // marker growth factor above the reference resolution

    // Radar box diameter at the reference resolution (1080p, radar scale 1) used to
    // anchor the follow-mode map zoom, so the visible world window no longer scales
    // with screen resolution.
    constexpr float kRefBoxSize = kRefResolutionH * kRadarHeightFrac;

    // Overview projection.
    constexpr float kWorldHalfSpan = 4096.0f; // half the world extent the overview zoom maps across
    constexpr float kOverviewTexelScale = 1024.0f; // overview texels per world unit at zoom 1
    constexpr float kScreenAspect = 4.0f / 3.0f;
    constexpr float kFollowZoomBase = 20.0f; // follow-mode world-window base, divided by map_scale
    constexpr int kOverviewTileCols = 4; // overview sprite tile grid: 4x3 frames per zoom level
    constexpr int kOverviewTileRows = 3;
    constexpr int kOverviewFramesPerLevel = kOverviewTileCols * kOverviewTileRows;
} // namespace

RadarBox RadarView_ComputeBox(float screen_w, float screen_h, float scale, bool spectating)
{
    // In observer mode the spectator UI draws letterbox bars at the top, so the
    // radar is pushed down below the bar.
    float margin_y = spectating ? screen_h * kSpectatorMarginFrac : screen_h * kRadarMarginFrac;

    RadarBox box;
    box.size = screen_h * kRadarHeightFrac * scale;
    box.x = screen_h * kRadarMarginFrac;
    box.y = margin_y;

    // Marker scale: full below the reference resolution, damped above it so
    // dots/crosses/rings keep a sane size on high resolutions instead of growing
    // linearly with the radar.
    float damped = (screen_h <= kRefResolutionH) ? screen_h : kRefResolutionH + (screen_h - kRefResolutionH) * kHiResDampen;
    box.elem = damped * kRadarHeightFrac * scale;
    box.screen_w = screen_w;
    box.screen_h = screen_h;
    return box;
}

RadarTransform RadarView_ComputeTransform(const RadarBox& box, const RadarViewParams& p)
{
    ncl_math::Vector2 center = p.fullmap ? p.ov_origin : p.view_origin;
    // Follow mode: normalise by the actual box size against the reference so the world
    // window depends only on map_scale, not on resolution. Full-map view frames the whole
    // world at map_scale 1 and lets map_scale zoom in from there; rotation does not apply.
    float viewzoom =
        p.fullmap ? (2.0f * kWorldHalfSpan / box.size) / p.map_scale : (kFollowZoomBase / p.map_scale) * (kRefBoxSize / box.size);

    // A round radar clips the corners of the full-map rectangle. Zoom out so its
    // half-diagonal meets the disc radius (factor sqrt(1 + 1/aspect^2)) and the whole
    // map fits inside the circle.
    if (p.fullmap && p.round)
    {
        viewzoom *= std::sqrt(1.0f + 1.0f / (kScreenAspect * kScreenAspect));
    }

    float yaw = (p.rotate && !p.fullmap) ? p.view_yaw : 0.0f;
    float angles = (yaw + 90.0f) * ncl_math::kDeg2Rad;

    float k = p.ov_zoom / kOverviewTexelScale;
    float x_temp, y_temp;

    if (p.ov_rotated)
    {
        angles -= ncl_math::kPi / 2.0f;
        x_temp = 3.0f + k * (p.ov_origin.y - center.y);
        y_temp = 4.0f - k * (p.ov_origin.x - center.x);
    }
    else
    {
        x_temp = 3.0f + k * (p.ov_origin.x - center.x);
        y_temp = 4.0f + k * (p.ov_origin.y - center.y);
    }

    int total_frames = p.sprite_frames > 0 ? p.sprite_frames : kOverviewFramesPerLevel;
    int tiles = (int)std::sqrt((float)(total_frames / kOverviewFramesPerLevel));

    if (tiles < 1)
    {
        tiles = 1;
    }

    int x_tiles = tiles * kOverviewTileCols;
    int y_tiles = tiles * kOverviewTileRows;

    float x_step = (2.0f * kWorldHalfSpan / viewzoom) / x_tiles;
    float y_step = -(2.0f * kWorldHalfSpan / (viewzoom * kScreenAspect)) / y_tiles;

    RadarTransform t;
    t.center = {box.x + box.size * 0.5f, box.y + box.size * 0.5f};
    t.k = k;
    t.world_center = center;
    t.total_frames = total_frames;
    t.x_tiles = x_tiles;
    t.y_tiles = y_tiles;
    t.up = ncl_math::FromAngle(angles).Perp() * y_step;
    t.right = ncl_math::FromAngle(angles) * x_step;
    t.out = t.center - t.right * x_temp - t.up * y_temp;

    // World-to-screen projection: screen = center + basis_x*(k*dx) + basis_y*(k*dy)
    // with dx/dy = world - world_center. The overview "rotated" mode swaps the axes
    // and flips a sign; both variants are folded into the two basis columns here so
    // WorldToScreen/MarkerDirection carry no branch.
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

ncl_math::Vector2 RadarView_ClampToRadar(const RadarBox& box, bool round, int size, ncl_math::Vector2 p)
{
    float cx = box.x + box.size * 0.5f;
    float cy = box.y + box.size * 0.5f;
    float half = box.size * 0.5f - size;

    if (round)
    {
        ncl_math::Vector2 center{cx, cy};
        ncl_math::Vector2 o = p - center;
        float dist = o.Length();

        if (dist > half && dist > 0.0f)
        {
            return center + o / dist * half;
        }

        return p;
    }

    return {std::clamp(p.x, cx - half, cx + half), std::clamp(p.y, cy - half, cy + half)};
}
