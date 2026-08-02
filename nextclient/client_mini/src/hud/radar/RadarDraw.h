#pragma once

#include <cstdint>

#include <ncl_math/aabb.h>
#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

// Screen-space circular mask applied to the radar map via a minimal GLSL program.
// Begin installs the program and uploads the circle (HUD pixel space); the map is
// then drawn with the regular TriAPI calls and clipped to the circle; End restores
// fixed-function rendering. Returns false (and draws nothing special) when the GL
// pipeline is unavailable, letting the caller fall back to an unclipped draw.

bool RadarDraw_Available();

// True once the prim program is linked (requires GL 2.0). When false the advanced
// radar cannot draw player/objective markers, so the caller falls back to the classic
// radar. Triggers the one-time GL/shader init on first call.
bool RadarDraw_PrimsAvailable();

// Drops the cached GL programs/textures so they are re-created on next use. Call on
// VidInit: a video mode change can recreate the GL context and invalidate the handles.
void RadarDraw_ResetGl();

// Per-frame draw statistics for the diagnostics overlay: quad-batch flushes
// (glDrawArrays calls) and CPU time spent submitting the blur backdrop. The caller
// resets them at the start of each radar frame.
struct RadarDrawStats
{
    int batches;
    float blur_ms;
};
void RadarDraw_ResetStats();
RadarDrawStats RadarDraw_Stats();

bool RadarDraw_Begin(ncl_math::Vector2 center, float radius, float feather, float gamma, bool square);

// Installs the program in ring mode. The caller draws a quad covering the band;
// the fragment paints an anti-aliased annulus (inner..outer) in the given color.
bool RadarDraw_BeginRing(
    ncl_math::Vector2 center,
    float outer,
    float inner,
    float feather,
    ncl_math::ColorF color,
    bool square
);
void RadarDraw_End();

// Draws a single textured quad (a runtime-generated map image) masked to the radar
// shape. xy/uv are four screen-space corners and their texture coords; the texture
// is sampled and tinted by (r,g,b,a). Used when no overview sprite is available.
void RadarDraw_DrawMapTexture(
    unsigned int tex,
    const float* xy,
    const float* uv,
    ncl_math::ColorF color,
    ncl_math::Vector2 center,
    float radius,
    bool square
);

// Polyline in HUD pixel space with a per-point alpha, drawn as mitered segment
// quads through the prim batch. Used for movement trails.
void RadarDraw_DrawPolyline(const ncl_math::Vector2* points, const uint8_t* alpha, int count, ncl_math::Color color, float width);

void RadarDraw_DrawDisc(ncl_math::Vector2 center, float radius, ncl_math::Color color);

// One glyph cell from a LUMINANCE_ALPHA atlas (alpha = coverage, luminance = fill), batched with the
// analytic shapes. The caller colour modulates the luminance, so the glyph body takes the colour and
// a baked drop shadow (luminance 0) stays black. Only one atlas may be sampled per batch; a different
// one flushes.
void RadarDraw_DrawGlyph(
    unsigned int atlas,
    const ncl_math::Aabb2& rect,
    const ncl_math::Aabb2& uv,
    ncl_math::Color fill
);

// Clips subsequent batched draws to the radar shape (round or square). Pass to keep a
// large primitive (e.g. a smoke cloud) from spilling past the radar; clear afterwards.
void RadarDraw_SetPrimClip(ncl_math::Vector2 center, float radius, bool square);
void RadarDraw_ClearPrimClip();

// Prim batch region. While open, the batched draws below keep their GL program bound
// instead of unbinding after every primitive, so a run of primitives costs one program
// bind instead of one per shape. Only wrap regions that contain no fixed-function or
// engine 2D draws (those need program 0). Calls nest; the outermost End unbinds.
void RadarDraw_BeginPrims();
void RadarDraw_EndPrims();

// Draws any primitives queued so far and hands back to fixed function. Call before an
// engine/TriAPI draw that must appear over them (e.g. the map tiles over the queued
// background disc): those draws bypass the program bind that would otherwise flush.
void RadarDraw_FlushPrims();

// Flashbang wash. While set, every radar primitive (map, border, markers, blurred
// backdrop, trails) blends its colour toward (r,g,b) by alpha/255, so the whole
// radar including the parts protruding past the rim is blinded with the view, with
// no overlay fill and thus no halo on the backdrop. Clear it after drawing. Changing
// the fade flushes any pending marker batch, so queued markers keep the fade they
// were drawn under.
void RadarDraw_SetFade(ncl_math::Color color);
void RadarDraw_ClearFade();

// Returns the given colour blended toward the current fade (unchanged when no fade
// is set). For non-shader paths such as trails or text labels.
ncl_math::Color RadarDraw_FadeColor(ncl_math::Color color);

void RadarDraw_DrawTriangle(ncl_math::Vector2 p0, ncl_math::Vector2 p1, ncl_math::Vector2 p2, ncl_math::Color color);

// Axis-aligned solid rectangle in HUD pixel space, batched with the other prims. Replaces the
// engine's immediate-mode FillRGBABlend for the radar's own fills so they join the batch and take
// the radar clip and flashbang wash.
void RadarDraw_DrawRect(ncl_math::Vector2 min, ncl_math::Vector2 max, ncl_math::Color color);

// Anti-aliased X cross centred at (cx,cy): two diagonals of the given half-extent
// and line half-width, drawn as a single shape so the centre keeps the same
// opacity as the arms (no double blend where they overlap).
void RadarDraw_DrawCross(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color);

// Anti-aliased ring (annulus) between inner and outer radius. Used to mark the
// bomb carrier with a coloured halo around their dot.
void RadarDraw_DrawRing(ncl_math::Vector2 center, float outer, float inner, ncl_math::Color color);

// Round radar border: an annulus drawn with the mask program's ring mode; when
// that program is unavailable it falls back to rasterizing the circle with pixel
// quads, so the border never disappears.
void RadarDraw_DrawBorderRing(ncl_math::Vector2 center, float outer, float inner, float feather, ncl_math::Color color);

// Additive FOV cone from (ax,ay) along the unit direction (fx,fy), widening by
// the half-angle spread, with an exponential alpha falloff toward the far edge.
void RadarDraw_DrawCone(ncl_math::Vector2 apex, ncl_math::Vector2 dir, float length, float spread, ncl_math::Color color, float peak_alpha);

// Captures the framebuffer region under the radar box and draws it back blurred
// (mipmap downsample) as the radar backdrop, masked to the radar shape and
// darkened for readability. Returns false when unavailable (caller falls back to
// a solid fill). Must be called before the map/markers are drawn. hud_w/hud_h are
// the HUD coordinate space size (GetScreenInfo), used to scale the captured rect to
// the framebuffer without reading the projection matrix back each frame.
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

// Sets GL_CLAMP_TO_EDGE on the currently bound 2D texture, so GL_LINEAR does not
// sample past the tile edge, which leaves transparent seams (the "grid" artifact)
// between adjacent overview tiles.
void RadarDraw_ClampBoundTexture();
