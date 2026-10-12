#pragma once

#include <cmath>
#include <optional>

#include <ncl_math/aabb.h>

#include <ncl_math/vec3.h>

// Free of engine, cvar and view-global access so the projection stays unit-testable;
// the caller gathers the inputs.

struct RadarBox
{
    float x, y; // top-left, screen px
    float size; // width == height
    float marker_px;
    float screen_w, screen_h; // HUD coordinate space (GetScreenInfo)

    ncl_math::Vector2 center() const { return {x + size * 0.5f, y + size * 0.5f}; }
    float radius() const { return size * 0.5f; }
};

struct RadarShape
{
    ncl_math::Vector2 center;
    float radius;
    bool round;

    ncl_math::Vector2 Clamp(int margin, ncl_math::Vector2 p) const;

    bool Contains(ncl_math::Vector2 p) const
    {
        ncl_math::Vector2 o = p - center;

        return round ? o.LengthSqr() <= radius * radius : (std::abs(o.x) <= radius && std::abs(o.y) <= radius);
    }
};

inline RadarShape RadarView_ComputeShape(const RadarBox& box, bool round)
{
    return {box.center(), box.radius(), round};
}

struct RadarTransform
{
    ncl_math::Vector2 tile_origin; // tile grid origin, screen px
    ncl_math::Vector2 right; // per-tile right edge, screen px
    ncl_math::Vector2 up; // per-tile up edge, screen px
    ncl_math::Vector2 center; // radar center, screen px
    ncl_math::Vector2 world_center; // world point mapped to the radar center
    ncl_math::Transform2 projection; // (world - world_center) * k -> screen px
    float k; // overview texels per world unit
    int total_frames; // overview sprite frame count
    int x_tiles, y_tiles;

    ncl_math::Vector2 WorldToScreen(ncl_math::Vector2 world) const;
    // facing_yaw in degrees; the result is a screen-space heading.
    ncl_math::Vector2 MarkerDirection(float facing_yaw) const;
};

struct RadarViewParams
{
    float ov_zoom = 1.0f; // overview_t fields (Overview.h)
    ncl_math::Vector2 ov_origin{};
    bool ov_rotated{};
    float map_scale = 1.0f; // already-clamped hud_radar_map_scale
    bool map_shift{};
    bool rotate{};
    bool fullmap_enabled{};
    bool scoreboard_visible{};
    bool round{};
    std::optional<ncl_math::Aabb2> map_bounds{};
    ncl_math::Vector2 view_origin{}; // follow-mode world center
    float view_yaw{}; // degrees
    int sprite_frames{}; // 0 with no sprite (runtime map)
};

bool RadarView_IsFreeCamera(int observer_mode);
bool RadarView_ShouldHighlightPlayer(int observer_mode);

RadarBox RadarView_ComputeBox(float screen_w, float screen_h, float scale, bool spectating, bool fullmap = false);
RadarTransform RadarView_ComputeTransform(const RadarBox& box, const RadarViewParams& p);
