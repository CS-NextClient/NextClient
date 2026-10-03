#include "RadarDraw.h"

#include "RadarDrawInternal.h"

#include <cmath>
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#include <cstddef>

#include "main.h"

#include <ncl_gl/loader.h>
#include <ncl_gl/program.h>
#include <glad/glad.h>
#include <ncl_math/vec3.h>
#include <ncl_math/aabb.h>
#include <Windows.h>
#include <cmrc/cmrc.hpp>
#include <goldsrc/client_api.h>

CMRC_DECLARE(client_mini_rc);

namespace
{
    bool g_ProgramDone;

    constexpr const char* kMaskUniforms[radar_draw::kMaskCount] = {
        "uMode", "uCenter", "uRadius", "uFeather", "uGamma", "uSquare",
        "uRingInner", "uRingColor", "uFadeColor", "uFadeAmt",
    };

    enum
    {
        kPrimClipCenter,
        kPrimClipRadius,
        kPrimClipSquare,
        kPrimFadeColor,
        kPrimFadeAmt,
        kPrimCount,
    };
    constexpr const char* kPrimUniforms[kPrimCount] = {
        "uClipCenter", "uClipRadius", "uClipSquare", "uFadeColor", "uFadeAmt",
    };

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
    constexpr const char* kPrimAttribs[kAttrCount] = {
        "aPos", "aShape", "aCenter", "aColor", "aTri", "aTri2",
    };

    constexpr const char* kBlurUniforms[radar_draw::kBlurCount] = {
        "uCenter",
        "uRadius",
        "uFeather",
        "uSquare",
        "uDirection",
        "uFilterPass",
        "uDarken",
        "uAlpha",
        "uFadeColor",
        "uFadeAmt",
    };

    static_assert(radar_draw::kMaskCount <= ncl_gl::kMaxUniforms && kPrimCount <= ncl_gl::kMaxUniforms &&
        radar_draw::kBlurCount <= ncl_gl::kMaxUniforms);

    ncl_gl::Program g_Mask;
    ncl_gl::Program g_Prim;
    ncl_gl::Program g_Blur;

    // Flashbang wash: primitives blend toward this colour rather than being covered by
    // an overlay fill, which would halo the backdrop. Amount is 0 outside a flash.
    ncl_math::ColorF g_FadeColor;
    float g_FadeAmt;

    // g_ClipR's huge default radius means no clip.
    ncl_math::Vector2 g_ClipCenter;
    float g_ClipR = 1.0e9f;
    bool g_ClipSquare;

    GLuint g_CurrentProgram;
    GLenum g_BlendSrc, g_BlendDst;
    bool g_BlendSet; // false = unknown (the engine may have changed blend state)

    GLint g_PrimAttr[kAttrCount];

    // Deferred quad batch: primitives queue instead of drawing, and the run is flushed
    // when blend or clip changes or another draw intervenes, so call order is preserved.
    struct PrimVertex
    {
        float pos[2];
        float shape[4];  // shape, feather, radius, len
        float center[4]; // center.xy, dir.xy
        float color[4];  // rgba (0..1)
        float tri[4];    // t0.xy, t1.xy
        float tri2[4];   // t2.xy, spread, pad
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
    PrimBatch g_Batch;
    int g_PrimRegion; // >0 defers the unbind to the outermost EndPrims

    RadarDrawStats g_Stats;

    std::string LoadShaderSource(const char* name)
    {
        auto fs = cmrc::client_mini_rc::get_filesystem();
        auto file = fs.open(std::string("assets/shaders/") + name);
        return std::string(file.begin(), file.end());
    }

    bool EnsureGlad()
    {
        return ncl_gl::EnsureLoaded() && GLAD_GL_VERSION_2_0 != 0;
    }

    void Init()
    {
        g_ProgramDone = true;

        if (!EnsureGlad())
        {
            return;
        }

        std::string vs_src = LoadShaderSource("radar.vert");
        std::string prim_vs_src = LoadShaderSource("radar_prim.vert");
        std::string mask_src = LoadShaderSource("radar_mask.frag");
        std::string prim_src = LoadShaderSource("radar_prim.frag");
        std::string backdrop_src = LoadShaderSource("radar_backdrop.frag");

        std::string error_log;
        ncl_gl::LoadProgram(g_Mask, vs_src.c_str(), mask_src.c_str(), kMaskUniforms, radar_draw::kMaskCount, nullptr, 0, &error_log);

        if (!g_Mask.ok)
        {
            gEngfuncs.Con_DPrintf("radar mask shader failed: %s\n", error_log.c_str());
        }

        if (g_Mask.ok)
        {
            glUseProgram(g_Mask.prog);
            glUniform1i(glGetUniformLocation(g_Mask.prog, "uTex"), 0);
            glUseProgram(0);
        }

        error_log.clear();
        ncl_gl::LoadProgram(g_Prim, prim_vs_src.c_str(), prim_src.c_str(), kPrimUniforms, kPrimCount, kPrimAttribs, kAttrCount, &error_log);

        if (!g_Prim.ok)
        {
            gEngfuncs.Con_DPrintf("radar prim shader failed: %s\n", error_log.c_str());
        }

        if (g_Prim.ok)
        {
            for (int i = 0; i < kAttrCount; i++)
            {
                g_PrimAttr[i] = glGetAttribLocation(g_Prim.prog, kPrimAttribs[i]);
            }

            glUseProgram(g_Prim.prog);
            glUniform1i(glGetUniformLocation(g_Prim.prog, "uAtlas"), 0);
            glUseProgram(0);
        }

        error_log.clear();
        ncl_gl::LoadProgram(g_Blur, vs_src.c_str(), backdrop_src.c_str(), kBlurUniforms, radar_draw::kBlurCount, nullptr, 0, &error_log);

        if (!g_Blur.ok)
        {
            gEngfuncs.Con_DPrintf("radar backdrop shader failed: %s\n", error_log.c_str());
        }

        if (g_Blur.ok)
        {
            glUseProgram(g_Blur.prog);
            glUniform1i(glGetUniformLocation(g_Blur.prog, "uTex"), 0);
            glUseProgram(0);

            radar_draw::InitEffectsGl(g_Blur);
        }
    }

    // The cache is safe only because UseProgram(0) invalidates it whenever control returns
    // to the engine.
    void SetBlend(GLenum src, GLenum dst)
    {
        if (g_BlendSet && g_BlendSrc == src && g_BlendDst == dst)
        {
            return;
        }

        glEnable(GL_BLEND);
        glBlendFunc(src, dst);
        g_BlendSrc = src;
        g_BlendDst = dst;
        g_BlendSet = true;
    }

    // Binds the prim program directly: UseProgram would recurse here.
    void FlushBatch()
    {
        if (g_Batch.verts.empty())
        {
            g_Batch.open = false;
            return;
        }

        if (g_CurrentProgram != g_Prim.prog)
        {
            glUseProgram(g_Prim.prog);
            g_CurrentProgram = g_Prim.prog;
        }

        SetBlend(g_Batch.src, g_Batch.dst);
        glUniform2f(g_Prim.loc[kPrimClipCenter], g_Batch.clip_center.x, g_Batch.clip_center.y);
        glUniform1f(g_Prim.loc[kPrimClipRadius], g_Batch.clip_radius);
        glUniform1f(g_Prim.loc[kPrimClipSquare], g_Batch.clip_square);
        glUniform3f(g_Prim.loc[kPrimFadeColor], g_FadeColor.r, g_FadeColor.g, g_FadeColor.b);
        glUniform1f(g_Prim.loc[kPrimFadeAmt], g_FadeAmt);

        // Restore rather than unbind afterwards: hw.dll caches the bound texture name and
        // early-outs on a hit, so leaving a foreign texture bound desyncs that cache.
        GLint prev_tex = 0;
        GLint prev_unit = GL_TEXTURE0;

        if (g_Batch.tex != 0)
        {
            // The engine's multitexture path can leave unit 1 current; the sampler is on unit 0.
            glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
            glBindTexture(GL_TEXTURE_2D, g_Batch.tex);
        }

        // Client-side vertex arrays, so make sure no engine VBO hijacks the pointers.
        GLint prev_vbo = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        const char* base = reinterpret_cast<const char*>(g_Batch.verts.data());
        GLsizei stride = sizeof(PrimVertex);
        glVertexAttribPointer(g_PrimAttr[kAttrPos], 2, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, pos));
        glVertexAttribPointer(g_PrimAttr[kAttrShape], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, shape));
        glVertexAttribPointer(g_PrimAttr[kAttrCenter], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, center));
        glVertexAttribPointer(g_PrimAttr[kAttrColor], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, color));
        glVertexAttribPointer(g_PrimAttr[kAttrTri], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, tri));
        glVertexAttribPointer(g_PrimAttr[kAttrTri2], 4, GL_FLOAT, GL_FALSE, stride, base + offsetof(PrimVertex, tri2));

        for (int i = 0; i < kAttrCount; i++)
        {
            glEnableVertexAttribArray(g_PrimAttr[i]);
        }

        glDrawArrays(GL_QUADS, 0, (GLsizei)g_Batch.verts.size());
        g_Stats.batches++;

        for (int i = 0; i < kAttrCount; i++)
        {
            glDisableVertexAttribArray(g_PrimAttr[i]);
        }

        glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);

        if (g_Batch.tex != 0)
        {
            glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
            glActiveTexture((GLenum)prev_unit);
        }

        g_Batch.verts.clear();
        g_Batch.open = false;
        g_Batch.tex = 0;
    }

    void UseProgram(GLuint p)
    {
        // Hand-off must draw the queued primitives first, so they land in call order.
        if (p != g_Prim.prog && g_Batch.open)
        {
            FlushBatch();
        }

        if (p != g_CurrentProgram)
        {
            glUseProgram(p);
            g_CurrentProgram = p;
        }

        if (p == 0)
        {
            g_BlendSet = false;
        }
    }

    void ApplyFade(GLint color_loc, GLint amt_loc)
    {
        glUniform3f(color_loc, g_FadeColor.r, g_FadeColor.g, g_FadeColor.b);
        glUniform1f(amt_loc, g_FadeAmt);
    }

    void AddQuad(GLenum blend_src, GLenum blend_dst, const PrimVertex (&corners)[4], GLuint tex = 0)
    {
        float clip_sq = g_ClipSquare ? 1.0f : 0.0f;

        if (g_Batch.open &&
            (g_Batch.src != blend_src || g_Batch.dst != blend_dst || g_Batch.clip_center != g_ClipCenter ||
             g_Batch.clip_radius != g_ClipR || g_Batch.clip_square != clip_sq ||
             (tex != 0 && g_Batch.tex != 0 && g_Batch.tex != tex)))
        {
            FlushBatch();
        }

        g_Batch.open = true;
        g_Batch.src = blend_src;
        g_Batch.dst = blend_dst;
        g_Batch.clip_center = g_ClipCenter;
        g_Batch.clip_radius = g_ClipR;
        g_Batch.clip_square = clip_sq;

        if (tex != 0)
        {
            g_Batch.tex = tex;
        }

        for (int c = 0; c < 4; c++)
        {
            g_Batch.verts.push_back(corners[c]);
        }
    }

    // Must match the vShape.x dispatch in radar_prim.frag.
    constexpr float kShapeDisc = 0.0f;
    constexpr float kShapeCone = 1.0f;
    constexpr float kShapeTriangle = 2.0f;
    constexpr float kShapeCross = 3.0f;
    constexpr float kShapeRing = 4.0f;
    constexpr float kShapeSolid = 5.0f;
    constexpr float kShapeGlyph = 6.0f;

    struct ShapeParams
    {
        float shape;
        float feather;
        ncl_math::Vector2 center;     // shape centre / cone apex
        float radius;                 // disc radius / cross half-extent / ring outer
        float len;                    // cone length / cross half-width / ring inner
        ncl_math::Vector2 dir;        // cone direction (unit)
        float spread;                 // cone half-angle (radians)
        ncl_math::Color color;
        float alpha;                  // 0..1
        ncl_math::Vector2 t0, t1, t2;
    };

    void AddShapeQuad(const ShapeParams& p, GLenum blend_src, GLenum blend_dst, ncl_math::Aabb2 quad)
    {
        if (!RadarDraw_CheckPrimsAvailable())
        {
            return;
        }

        PrimVertex v;
        v.shape[0] = p.shape;
        v.shape[1] = p.feather;
        v.shape[2] = p.radius;
        v.shape[3] = p.len;
        v.center[0] = p.center.x;
        v.center[1] = p.center.y;
        v.center[2] = p.dir.x;
        v.center[3] = p.dir.y;
        ncl_math::ColorF cf = p.color.ToFloat();
        v.color[0] = cf.r;
        v.color[1] = cf.g;
        v.color[2] = cf.b;
        v.color[3] = p.alpha;
        v.tri[0] = p.t0.x;
        v.tri[1] = p.t0.y;
        v.tri[2] = p.t1.x;
        v.tri[3] = p.t1.y;
        v.tri2[0] = p.t2.x;
        v.tri2[1] = p.t2.y;
        v.tri2[2] = p.spread;
        v.tri2[3] = 0.0f;

        const ncl_math::Vector2 corner_pos[4] = {
            {quad.min.x, quad.min.y}, {quad.max.x, quad.min.y}, {quad.max.x, quad.max.y}, {quad.min.x, quad.max.y}
        };
        PrimVertex corners[4] = {v, v, v, v};

        for (int c = 0; c < 4; c++)
        {
            corners[c].pos[0] = corner_pos[c].x;
            corners[c].pos[1] = corner_pos[c].y;
        }

        AddQuad(blend_src, blend_dst, corners);
    }

    // ClampToRadar can squash trail points into sub-pixel deltas whose direction is noise.
    constexpr float kMinSegmentLen = ncl_math::kNearZero;

    // d0, d1 are unit; the miter is clamped to twice the half-width so a sharp turn
    // cannot spike the corner.
    ncl_math::Vector2 ComputeMiterOffset(ncl_math::Vector2 d0, ncl_math::Vector2 d1, float half_width)
    {
        ncl_math::Vector2 sum = d0 + d1;
        float len = sum.Length();

        if (len < ncl_math::kNearZero)
        {
            return d1.Perp() * half_width; // 180 degree reversal
        }

        ncl_math::Vector2 miter = (sum / len).Perp();
        float align = std::max(miter.Dot(d1.Perp()), 0.5f);

        return miter * (half_width / align);
    }
} // namespace

bool RadarDraw_CheckPrimsAvailable()
{
    if (!g_ProgramDone)
    {
        Init();
    }

    return g_Prim.ok;
}

void RadarDraw_ResetGl()
{
    radar_draw::ResetEffectsGl();

    // A video mode change can recreate the GL context and dangle these handles. Deleting a
    // handle from a destroyed context is a no-op, so this runs either way.
    if (ncl_gl::IsLoaded())
    {
        if (g_Mask.prog)
        {
            glDeleteProgram(g_Mask.prog);
        }

        if (g_Prim.prog)
        {
            glDeleteProgram(g_Prim.prog);
        }

        if (g_Blur.prog)
        {
            glDeleteProgram(g_Blur.prog);
        }

    }

    g_ProgramDone = false;
    g_Mask = ncl_gl::Program{};
    g_Prim = ncl_gl::Program{};
    g_Blur = ncl_gl::Program{};
    g_CurrentProgram = 0;
    g_Batch = PrimBatch{};
    g_BlendSet = false;
}

void RadarDraw_ResetStats()
{
    g_Stats = RadarDrawStats{};
}

RadarDrawStats RadarDraw_GetStats()
{
    return g_Stats;
}

void RadarDraw_SetPrimClip(ncl_math::Vector2 center, float radius, bool square)
{
    g_ClipCenter = center;
    g_ClipR = radius;
    g_ClipSquare = square;
}

void RadarDraw_ClearPrimClip()
{
    g_ClipCenter = {};
    g_ClipR = 1.0e9f;
    g_ClipSquare = false;
}

void RadarDraw_BeginPrims()
{
    g_PrimRegion++;
}

void RadarDraw_EndPrims()
{
    if (g_PrimRegion > 0 && --g_PrimRegion == 0)
    {
        UseProgram(0);
    }
}

void RadarDraw_FlushPrims()
{
    if (g_Batch.open)
    {
        FlushBatch();
        UseProgram(0);
    }
}

void RadarDraw_SetFade(ncl_math::Color color)
{
    // The fade applies per batch at flush time; queued primitives must be committed first
    // or the change recolours them retroactively.
    RadarDraw_FlushPrims();
    g_FadeColor = ncl_math::ColorF{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    g_FadeAmt = std::clamp(color.a / 255.0f, 0.0f, 1.0f);
}

void RadarDraw_ClearFade()
{
    RadarDraw_FlushPrims();
    g_FadeAmt = 0.0f;
}

void RadarDraw_EndFrame()
{
    RadarDraw_ClearFade();
}

ncl_math::Color RadarDraw_ApplyFade(ncl_math::Color color)
{
    if (g_FadeAmt <= 0.0f)
    {
        return color;
    }

    // Lerp blends all four channels, so the source alpha is put back explicitly.
    return ncl_math::Lerp(color, g_FadeColor.ToBytes().WithAlpha(color.a), g_FadeAmt);
}

void RadarDraw_DrawPolyline(
    std::span<const ncl_math::Vector2> points,
    std::span<const uint8_t> alpha,
    ncl_math::Color color,
    float width
)
{
    int count = (int)points.size();

    if (count < 2 || !RadarDraw_CheckPrimsAvailable())
    {
        return;
    }

    float half = width * 0.5f;
    ncl_math::ColorF cf = color.ToFloat();

    PrimVertex v{};
    v.shape[0] = kShapeSolid;
    v.color[0] = cf.r;
    v.color[1] = cf.g;
    v.color[2] = cf.b;

    ncl_math::Vector2 prev_dir;
    bool has_prev = false;

    for (int i = 0; i + 1 < count; i++)
    {
        ncl_math::Vector2 dir = points[i + 1] - points[i];
        float len = dir.Length();

        if (len < kMinSegmentLen) // stationary samples: break the miter chain
        {
            has_prev = false;
            continue;
        }

        dir = dir / len;

        ncl_math::Vector2 next_dir;
        bool has_next = false;

        if (i + 2 < count)
        {
            next_dir = points[i + 2] - points[i + 1];
            float next_len = next_dir.Length();

            if (next_len >= kMinSegmentLen)
            {
                next_dir = next_dir / next_len;
                has_next = true;
            }
        }

        // Both quads at a joint derive the same miter, so the edges meet exactly: no seam.
        ncl_math::Vector2 start_off = has_prev ? ComputeMiterOffset(prev_dir, dir, half) : dir.Perp() * half;
        ncl_math::Vector2 end_off = has_next ? ComputeMiterOffset(dir, next_dir, half) : dir.Perp() * half;

        PrimVertex corners[4] = {v, v, v, v};
        corners[0].pos[0] = points[i].x + start_off.x;
        corners[0].pos[1] = points[i].y + start_off.y;
        corners[1].pos[0] = points[i].x - start_off.x;
        corners[1].pos[1] = points[i].y - start_off.y;
        corners[2].pos[0] = points[i + 1].x - end_off.x;
        corners[2].pos[1] = points[i + 1].y - end_off.y;
        corners[3].pos[0] = points[i + 1].x + end_off.x;
        corners[3].pos[1] = points[i + 1].y + end_off.y;
        corners[0].color[3] = alpha[i] / 255.0f;
        corners[1].color[3] = alpha[i] / 255.0f;
        corners[2].color[3] = alpha[i + 1] / 255.0f;
        corners[3].color[3] = alpha[i + 1] / 255.0f;

        AddQuad(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, corners);

        prev_dir = dir;
        has_prev = true;
    }
}

void RadarDraw_DrawDisc(ncl_math::Vector2 center, float radius, ncl_math::Color color)
{
    float e = radius + 2.0f;
    AddShapeQuad(
        {.shape = kShapeDisc, .feather = 1.2f, .center = center, .radius = radius, .color = color, .alpha = color.a / 255.0f},
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
        {{center.x - e, center.y - e}, {center.x + e, center.y + e}}
    );
}

void RadarDraw_DrawTriangle(ncl_math::Vector2 p0, ncl_math::Vector2 p1, ncl_math::Vector2 p2, ncl_math::Color color)
{
    ncl_math::Aabb2 bb = ncl_math::Aabb2::Empty();
    bb.Include(p0);
    bb.Include(p1);
    bb.Include(p2);
    bb = bb.Expanded(2.0f);
    AddShapeQuad(
        {.shape = kShapeTriangle, .feather = 1.1f, .color = color, .alpha = color.a / 255.0f, .t0 = p0, .t1 = p1, .t2 = p2},
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, bb
    );
}

void RadarDraw_DrawRect(ncl_math::Vector2 min, ncl_math::Vector2 max, ncl_math::Color color)
{
    ncl_math::ColorF cf = color.ToFloat();

    PrimVertex v{};
    v.shape[0] = kShapeSolid;
    v.color[0] = cf.r;
    v.color[1] = cf.g;
    v.color[2] = cf.b;
    v.color[3] = color.a / 255.0f;

    PrimVertex corners[4] = {v, v, v, v};
    const ncl_math::Vector2 pos[4] = {{min.x, min.y}, {max.x, min.y}, {max.x, max.y}, {min.x, max.y}};

    for (int c = 0; c < 4; c++)
    {
        corners[c].pos[0] = pos[c].x;
        corners[c].pos[1] = pos[c].y;
    }

    AddQuad(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, corners);
}

void RadarDraw_DrawGlyph(
    unsigned int atlas,
    const ncl_math::Aabb2& rect,
    const ncl_math::Aabb2& uv,
    ncl_math::Color fill
)
{
    PrimVertex v{};
    v.shape[0] = kShapeGlyph;
    v.color[0] = fill.r / 255.0f;
    v.color[1] = fill.g / 255.0f;
    v.color[2] = fill.b / 255.0f;
    v.color[3] = fill.a / 255.0f;

    PrimVertex corners[4] = {v, v, v, v};
    const ncl_math::Vector2 pos[4] = {
        {rect.min.x, rect.min.y}, {rect.max.x, rect.min.y}, {rect.max.x, rect.max.y}, {rect.min.x, rect.max.y}
    };
    const ncl_math::Vector2 tex[4] = {
        {uv.min.x, uv.min.y}, {uv.max.x, uv.min.y}, {uv.max.x, uv.max.y}, {uv.min.x, uv.max.y}
    };

    for (int c = 0; c < 4; c++)
    {
        corners[c].pos[0] = pos[c].x;
        corners[c].pos[1] = pos[c].y;
        corners[c].tri[0] = tex[c].x;
        corners[c].tri[1] = tex[c].y;
    }

    AddQuad(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, corners, (GLuint)atlas);
}

void RadarDraw_DrawCross(ncl_math::Vector2 center, float half_extent, float half_width, ncl_math::Color color)
{
    float m = half_extent + half_width + 2.0f;
    AddShapeQuad(
        {.shape = kShapeCross, .feather = 1.1f, .center = center, .radius = half_extent, .len = half_width,
         .color = color, .alpha = color.a / 255.0f},
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
        {{center.x - m, center.y - m}, {center.x + m, center.y + m}}
    );
}

void RadarDraw_DrawRing(ncl_math::Vector2 center, float outer, float inner, ncl_math::Color color)
{
    float m = outer + 2.0f;
    AddShapeQuad(
        {.shape = kShapeRing, .feather = 1.1f, .center = center, .radius = outer, .len = inner, .color = color, .alpha = color.a / 255.0f},
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA,
        {{center.x - m, center.y - m}, {center.x + m, center.y + m}}
    );
}

void RadarDraw_DrawBorderRing(ncl_math::Vector2 center, float outer, float inner, float feather, ncl_math::Color color)
{
    if (RadarDraw_BeginMaskRing(center, outer, inner, feather, color.ToFloat(), false))
    {
        SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float m = outer + 2.0f;
        glBegin(GL_QUADS);
        glVertex2f(center.x - m, center.y - m);
        glVertex2f(center.x + m, center.y - m);
        glVertex2f(center.x + m, center.y + m);
        glVertex2f(center.x - m, center.y + m);
        glEnd();
        RadarDraw_EndMask();
        return;
    }

    if (!EnsureGlad())
    {
        return;
    }

    // No mask program: rasterize with 1px quads (eighth-circle symmetry), FillRGBABlend-style.
    UseProgram(0);
    glDisable(GL_TEXTURE_2D);
    SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);

    auto px = [](int x, int y) {
        glVertex2f((float)x, (float)y);
        glVertex2f((float)(x + 1), (float)y);
        glVertex2f((float)(x + 1), (float)(y + 1));
        glVertex2f((float)x, (float)(y + 1));
    };

    int icx = (int)center.x, icy = (int)center.y, radius = (int)outer;
    int thickness = (int)(outer - inner);

    for (int t = 0; t < thickness; t++)
    {
        int rr = radius - t;

        if (rr <= 0)
        {
            break;
        }

        int count = (int)(0.70710678f * rr + 0.5f);

        for (int i = 0; i <= count; i++)
        {
            int s = (int)(std::sqrt((float)(rr * rr - i * i)) + 0.5f);
            px(icx + i, icy + s);
            px(icx + i, icy - s);
            px(icx - i, icy + s);
            px(icx - i, icy - s);
            px(icx + s, icy + i);
            px(icx + s, icy - i);
            px(icx - s, icy + i);
            px(icx - s, icy - i);
        }
    }

    glEnd();
    glEnable(GL_TEXTURE_2D);
    glColor4ub(255, 255, 255, 255);
}

void RadarDraw_DrawCone(ncl_math::Vector2 apex, ncl_math::Vector2 dir, float length, float spread, ncl_math::Color color, float peak_alpha)
{
    ncl_math::Vector2 perp = dir.Perp();
    float hw = length * tanf(spread);
    ncl_math::Vector2 tip = apex + dir * length;
    ncl_math::Aabb2 bb = ncl_math::Aabb2::Empty();
    bb.Include(apex);
    bb.Include(tip + perp * hw);
    bb.Include(tip - perp * hw);
    bb = bb.Expanded(2.0f);
    AddShapeQuad(
        {.shape = kShapeCone, .feather = 1.5f, .center = apex, .len = length, .dir = dir, .spread = spread,
         .color = color, .alpha = peak_alpha},
        GL_SRC_ALPHA, GL_ONE, bb
    );
}

namespace radar_draw
{
    bool EnsureGlad()
    {
        return ::EnsureGlad();
    }

    void EnsurePrograms()
    {
        if (!g_ProgramDone)
        {
            Init();
        }
    }

    ncl_gl::Program& GetMaskProgram()
    {
        return g_Mask;
    }

    ncl_gl::Program& GetBlurProgram()
    {
        return g_Blur;
    }

    void UseProgram(GLuint p)
    {
        ::UseProgram(p);
    }

    void SetBlend(GLenum src, GLenum dst)
    {
        ::SetBlend(src, dst);
    }

    void ApplyFade(GLint color_loc, GLint amt_loc)
    {
        ::ApplyFade(color_loc, amt_loc);
    }

    void AddBlurMs(float ms)
    {
        g_Stats.blur_ms += ms;
    }
} // namespace radar_draw
