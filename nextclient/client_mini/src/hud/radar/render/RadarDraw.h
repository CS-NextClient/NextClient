#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glad/glad.h>
#include <ncl_gl/program.h>
#include <ncl_math/aabb.h>
#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

#include "hud/radar/integration/Performance.h"

struct RadarDrawStats
{
    int batches;
    int quads;
};

struct RadarDrawPlayerMarker
{
    ncl_math::Vector2 pos{};
    ncl_math::Vector2 direction{};
    float opacity = 1.0f;
    float weight = 1.0f;
    bool offscreen{};
};

class RadarDrawContext
{
private:
    enum
    {
        kAttrPos,
        kAttrShape,
        kAttrCenter,
        kAttrColor,
        kAttrTri,
        kAttrTri2,
        kAttrCount,
    };

    struct PrimVertex
    {
        float pos[2];
        float shape[4]; // shape, feather, radius, len
        float center[4]; // center.xy, dir.xy
        float color[4]; // rgba (0..1)
        float tri[4]; // t0.xy, t1.xy
        float tri2[4]; // t2.xy, spread, pad
    };

    struct PrimBatch
    {
        std::vector<PrimVertex> verts;
        bool open;
        GLenum src, dst;
        ncl_math::Vector2 clip_center;
        float clip_radius = 1.0e9f;
        float clip_square;
        GLuint tex; // glyph atlas, 0 when the run draws no glyphs
    };

    struct ShapeParams
    {
        float shape;
        float feather;
        ncl_math::Vector2 center; // shape center / cone apex
        float radius; // disc radius / cross half-extent / ring outer
        float len; // cone length / cross half-width / ring inner
        ncl_math::Vector2 dir; // cone direction (unit)
        float spread; // cone half-angle (radians)
        ncl_math::Color color;
        float alpha; // 0..1
        ncl_math::Vector2 t0, t1, t2;
    };

    radar::integration::Performance& performance_;
    void* owning_context_{};
    void* owning_window_{};
    bool program_done_{};
    ncl_gl::Program mask_{};
    ncl_gl::Program prim_{};
    ncl_gl::Program blur_{};
    ncl_math::ColorF fade_color_{};
    float fade_amt_{};
    ncl_math::Vector2 clip_center_{};
    float clip_r_ = 1.0e9f;
    bool clip_square_{};
    GLuint current_program_{};
    GLenum blend_src_{};
    GLenum blend_dst_{};
    bool blend_set_{};
    GLint prim_attr_[kAttrCount]{};
    // Changes in blend, clip or program flush this queue before later draws.
    PrimBatch batch_{};
    int prim_region_{};
    RadarDrawStats stats_{};
    GLuint bg_tex_{};
    GLuint bg_fbo_{};
    GLuint bg_small_{};
    GLuint bg_small_fbo_{};
    GLuint bg_temp_{};
    GLuint bg_temp_fbo_{};
    int bg_tex_w_{};
    int bg_tex_h_{};

public:
    explicit RadarDrawContext(radar::integration::Performance& performance) :
        performance_(performance)
    {}
    ~RadarDrawContext() = default;
    RadarDrawContext(const RadarDrawContext&) = delete;
    RadarDrawContext& operator=(const RadarDrawContext&) = delete;
    RadarDrawContext(RadarDrawContext&&) = delete;
    RadarDrawContext& operator=(RadarDrawContext&&) = delete;

    bool owns_context(void* context) const
    {
        return owning_context_ && owning_context_ == context;
    }
    bool owns_window(void* window) const
    {
        return owning_window_ && owning_window_ == window;
    }

    bool is_context(void* context, void* window) const
    {
        return owning_context_ && owning_context_ == context && owning_window_ == window;
    }

    void Prepare(void* context, void* window)
    {
        owning_context_ = context;
        owning_window_ = window;
    }

    bool are_prims_ready() const
    {
        return prim_.ok;
    }
    bool CheckMaskAvailable();

    // First call triggers the one-time GL/shader init; the marker shapes have no fixed-function fallback.
    bool CheckPrimsAvailable();

    void ResetGl(bool release);


    void ResetStats();
    RadarDrawStats GetStats();

    bool BeginMask(ncl_math::Vector2 center, float radius, float feather, float gamma, bool square);

    // Ring mode: the caller draws a quad covering the band and the fragment paints the annulus.
    bool BeginMaskRing(ncl_math::Vector2 center, float outer, float inner, float feather, ncl_math::ColorF color, bool square);
    void EndMask();

    // xy/uv are four screen-space corners and their texture coordinates.
    void DrawMapTexture(
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
    void DrawPolyline(std::span<const ncl_math::Vector2> points, std::span<const uint8_t> alpha, ncl_math::Color color, float width);

    void DrawDisc(ncl_math::Vector2 center, float radius, ncl_math::Color color);


    void DrawPlayerMarkerFade(
        const RadarDrawPlayerMarker& current,
        const RadarDrawPlayerMarker& previous,
        float radius,
        ncl_math::Color color
    );

    // LUMINANCE_ALPHA atlas cell: alpha = coverage, luminance = fill modulated by the color, so a baked
    // drop shadow (luminance 0) stays black. One atlas per batch; a different one flushes.
    void DrawGlyph(unsigned int atlas, const ncl_math::Aabb2& rect, const ncl_math::Aabb2& uv, ncl_math::Color fill);

    void DrawBomb(ncl_math::Vector2 center, float half_extent, ncl_math::Color color);

    void DrawDisconnectedSignal(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color);

    void SetPrimClip(ncl_math::Vector2 center, float radius, bool square);
    void ClearPrimClip();

    // While open the batched draws keep their GL program bound, so wrap only regions with no
    // fixed-function or engine 2D draws (those need program 0). Calls nest; the outermost End unbinds.
    void BeginPrims();
    void EndPrims();

    // Call before an engine/TriAPI draw that must appear over the queued primitives: those
    // draws bypass the program bind that would otherwise flush.
    void FlushPrims();

    // Flashbang wash: every radar primitive blends its color toward (r,g,b) by alpha/255,
    // instead of an overlay fill, which haloed the blurred backdrop. Changing the fade flushes
    // any pending marker batch, so queued markers keep the fade they were drawn under.
    void SetFade(ncl_math::Color color);
    void ClearFade();

    // Closes the radar's frame: commits the primitives still queued and drops the fade, so the
    // debug overlay and the rest of the HUD draw through the engine's own state.
    void EndFrame();

    // For the non-shader paths, which cannot read the fade uniform.
    ncl_math::Color ApplyFade(ncl_math::Color color);

    void DrawTriangle(ncl_math::Vector2 p0, ncl_math::Vector2 p1, ncl_math::Vector2 p2, ncl_math::Color color);

    void DrawRect(ncl_math::Vector2 min, ncl_math::Vector2 max, ncl_math::Color color);

    // One shape, not two diagonals: overlapping arms would double-blend at the center.
    void DrawCross(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color);

    void DrawQuestionMark(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color);

    void DrawRing(ncl_math::Vector2 center, float outer, float inner, ncl_math::Color color);

    void DrawBorderRing(ncl_math::Vector2 center, float outer, float inner, float feather, ncl_math::Color color);

    void DrawCone(ncl_math::Vector2 apex, ncl_math::Vector2 dir, float length, float spread, ncl_math::Color color, float peak_alpha);

    // Captures the framebuffer, so it runs before the map and the markers. hud_w/hud_h are the HUD
    // coordinate space (GetScreenInfo) the captured rect is scaled to the framebuffer through: taking
    // that scale from the projection matrix instead gives a black backdrop under fullscreen and
    // -stretchaspect, which render into an FBO.
    bool DrawBlurBackground(float box_x, float box_y, float box_size, bool square, float darken, float alpha, float hud_w, float hud_h);

    // GL_CLAMP_TO_EDGE on the currently bound 2D texture: without it GL_LINEAR samples past the
    // tile edge and leaves transparent seams between adjacent overview tiles.
    void ClampBoundTexture();

    void DrawLayerIndicator(ncl_math::Vector2 screen, int vertical, int size, ncl_math::Color color);

private:
    void Init();
    void SetBlend(GLenum src, GLenum dst);
    void FlushBatch();
    void UseProgram(GLuint p);
    void ApplyFade(GLint color_loc, GLint amt_loc);
    void AddQuad(GLenum blend_src, GLenum blend_dst, const PrimVertex (&corners)[4], GLuint tex = 0);
    void AddShapeQuad(const ShapeParams& p, GLenum blend_src, GLenum blend_dst, ncl_math::Aabb2 quad);
    void EnsurePrograms();
    ncl_gl::Program& GetMaskProgram();
    ncl_gl::Program& GetBlurProgram();
    void InitEffectsGl(ncl_gl::Program& blur);
    void ResetEffectsGl(bool release);
};
