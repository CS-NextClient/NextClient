#pragma once

#include <cstdint>
#include <span>

#include <ncl_math/aabb.h>
#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

bool RadarDraw_CheckMaskAvailable();

// First call triggers the one-time GL/shader init; the marker shapes have no fixed-function fallback.
bool RadarDraw_CheckPrimsAvailable();

// Call on VidInit: a mode change can recreate the GL context, invalidating the cached programs.
void RadarDraw_ResetGl();

struct RadarDrawStats
{
    int batches;
    int quads;
    float blur_ms;
};
void RadarDraw_ResetStats();
RadarDrawStats RadarDraw_GetStats();

bool RadarDraw_BeginMask(ncl_math::Vector2 center, float radius, float feather, float gamma, bool square);

// Ring mode: the caller draws a quad covering the band and the fragment paints the annulus.
bool RadarDraw_BeginMaskRing(
    ncl_math::Vector2 center,
    float outer,
    float inner,
    float feather,
    ncl_math::ColorF color,
    bool square
);
void RadarDraw_EndMask();

// xy/uv are four screen-space corners and their texture coordinates.
void RadarDraw_DrawMapTexture(
    unsigned int tex,
    std::span<const float, 8> xy,
    std::span<const float, 8> uv,
    ncl_math::ColorF color,
    ncl_math::Vector2 center,
    float radius,
    bool square,
    bool smoothing
);

// points and alpha are one per vertex and the same length.
void RadarDraw_DrawPolyline(
    std::span<const ncl_math::Vector2> points,
    std::span<const uint8_t> alpha,
    ncl_math::Color color,
    float width
);

void RadarDraw_DrawDisc(ncl_math::Vector2 center, float radius, ncl_math::Color color);

struct RadarDrawPlayerMarker
{
    ncl_math::Vector2 pos{};
    ncl_math::Vector2 direction{};
    float opacity = 1.0f;
    float weight = 1.0f;
    bool offscreen{};
};

void RadarDraw_DrawPlayerMarkerFade(
    const RadarDrawPlayerMarker& current,
    const RadarDrawPlayerMarker& previous,
    float radius,
    ncl_math::Color color
);

// LUMINANCE_ALPHA atlas cell: alpha = coverage, luminance = fill modulated by the color, so a baked
// drop shadow (luminance 0) stays black. One atlas per batch; a different one flushes.
void RadarDraw_DrawGlyph(
    unsigned int atlas,
    const ncl_math::Aabb2& rect,
    const ncl_math::Aabb2& uv,
    ncl_math::Color fill
);

void RadarDraw_DrawBomb(ncl_math::Vector2 center, float half_extent, ncl_math::Color color);

void RadarDraw_DrawDisconnectedSignal(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color);

void RadarDraw_SetPrimClip(ncl_math::Vector2 center, float radius, bool square);
void RadarDraw_ClearPrimClip();

// While open the batched draws keep their GL program bound, so wrap only regions with no
// fixed-function or engine 2D draws (those need program 0). Calls nest; the outermost End unbinds.
void RadarDraw_BeginPrims();
void RadarDraw_EndPrims();

// Call before an engine/TriAPI draw that must appear over the queued primitives: those
// draws bypass the program bind that would otherwise flush.
void RadarDraw_FlushPrims();

// Flashbang wash: every radar primitive blends its color toward (r,g,b) by alpha/255,
// instead of an overlay fill, which haloed the blurred backdrop. Changing the fade flushes
// any pending marker batch, so queued markers keep the fade they were drawn under.
void RadarDraw_SetFade(ncl_math::Color color);
void RadarDraw_ClearFade();

// Closes the radar's frame: commits the primitives still queued and drops the fade, so the
// debug overlay and the rest of the HUD draw through the engine's own state.
void RadarDraw_EndFrame();

// For the non-shader paths, which cannot read the fade uniform.
ncl_math::Color RadarDraw_ApplyFade(ncl_math::Color color);

void RadarDraw_DrawTriangle(ncl_math::Vector2 p0, ncl_math::Vector2 p1, ncl_math::Vector2 p2, ncl_math::Color color);

void RadarDraw_DrawRect(ncl_math::Vector2 min, ncl_math::Vector2 max, ncl_math::Color color);

// One shape, not two diagonals: overlapping arms would double-blend at the center.
void RadarDraw_DrawCross(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color);

void RadarDraw_DrawQuestionMark(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color);

void RadarDraw_DrawRing(ncl_math::Vector2 center, float outer, float inner, ncl_math::Color color);

void RadarDraw_DrawBorderRing(ncl_math::Vector2 center, float outer, float inner, float feather, ncl_math::Color color);

void RadarDraw_DrawCone(ncl_math::Vector2 apex, ncl_math::Vector2 dir, float length, float spread, ncl_math::Color color, float peak_alpha);

// Captures the framebuffer, so it runs before the map and the markers. hud_w/hud_h are the HUD
// coordinate space (GetScreenInfo) the captured rect is scaled to the framebuffer through: taking
// that scale from the projection matrix instead gives a black backdrop under fullscreen and
// -stretchaspect, which render into an FBO.
bool RadarDraw_DrawBlurBackground(
    float box_x,
    float box_y,
    float box_size,
    bool square,
    float darken,
    float alpha,
    float hud_w,
    float hud_h
);

// GL_CLAMP_TO_EDGE on the currently bound 2D texture: without it GL_LINEAR samples past the
// tile edge and leaves transparent seams between adjacent overview tiles.
void RadarDraw_ClampBoundTexture();

void RadarDraw_DrawLayerIndicator(ncl_math::Vector2 screen, int vertical, int size, ncl_math::Color color);
