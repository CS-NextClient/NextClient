#pragma once

#include <ncl_math/vec3.h>

// Pure view math for the advanced radar: where the box sits on screen and how the
// overview/world projects into it. Free of engine, cvar and view-global access so
// the projection is unit-testable; the caller gathers the inputs.

struct RadarBox
{
    float x, y; // top-left in screen pixels
    float size; // width == height
    float elem; // size used to scale markers: like size but damped on high
                // resolutions so dots/crosses/rings do not balloon
    float screen_w, screen_h; // HUD coordinate space (GetScreenInfo), cached for reuse
};

// Affine placement of the overview onto the screen for the current frame. The
// same basis is used to lay out the map tiles and to position player markers, so
// markers stay aligned with the map in every rotation/zoom mode.
struct RadarTransform
{
    ncl_math::Vector2 out; // screen position of the tile grid origin
    ncl_math::Vector2 right; // per-tile right edge (screen px)
    ncl_math::Vector2 up; // per-tile up edge (screen px)
    ncl_math::Vector2 center; // radar centre (screen px)
    ncl_math::Vector2 world_center; // world point mapped to the radar centre
    ncl_math::Transform2 projection; // (world - world_center) * k -> screen; rotated basis folded in
    float k; // overview texels per world unit (zoom / 1024)
    int total_frames; // overview sprite frame count (tile grid layout)
    int x_tiles, y_tiles; // tiles across/down, derived from total_frames

    ncl_math::Vector2 WorldToScreen(ncl_math::Vector2 world) const;
    // On-screen heading of a marker facing the given world yaw (degrees).
    ncl_math::Vector2 MarkerDirection(float facing_yaw) const;
};

struct RadarViewParams
{
    float ov_zoom; // overview_t fields (Overview.h)
    ncl_math::Vector2 ov_origin;
    bool ov_rotated;
    float map_scale; // clamped hud_radar_map_scale
    bool rotate;
    bool fullmap;
    bool round;
    ncl_math::Vector2 view_origin; // follow-mode world centre (local/observed player)
    float view_yaw; // degrees
    int sprite_frames; // overview sprite frame count; 0 with no sprite (runtime map)
};

RadarBox RadarView_ComputeBox(float screen_w, float screen_h, float scale, bool spectating);
RadarTransform RadarView_ComputeTransform(const RadarBox& box, const RadarViewParams& p);

// Clamps a marker position into the radar shape (round disc or square box), keeping
// a margin of `size` pixels so the marker body stays inside the rim.
ncl_math::Vector2 RadarView_ClampToRadar(const RadarBox& box, bool round, int size, ncl_math::Vector2 p);
