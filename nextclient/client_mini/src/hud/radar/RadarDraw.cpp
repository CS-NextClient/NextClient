#include "RadarDraw.h"
#include <ncl_gl/loader.h>
#include <ncl_gl/program.h>
#include "../../main.h"

#include <glad/glad.h>
#include <ncl_math/vec3.h>
#include <ncl_math/aabb.h>
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#include <cstddef>
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(client_mini_rc);

namespace
{
    bool g_ProgramDone;

    // Mask program (radar_mask.frag): radar-shape mask for the map, plus the border
    // ring mode.
    enum
    {
        kMaskMode,
        kMaskCenter,
        kMaskRadius,
        kMaskFeather,
        kMaskGamma,
        kMaskSquare,
        kMaskRingInner,
        kMaskRingColor,
        kMaskFadeColor,
        kMaskFadeAmt,
        kMaskCount,
    };
    constexpr const char* kMaskUniforms[kMaskCount] = {
        "uMode", "uCenter", "uRadius", "uFeather", "uGamma", "uSquare",
        "uRingInner", "uRingColor", "uFadeColor", "uFadeAmt",
    };

    // Prim program: analytic anti-aliased disc / cone / triangle / cross / ring, plus
    // solid quads (polyline segments). The per-primitive parameters (shape, geometry,
    // colour) arrive as vertex attributes so a run of primitives draws as one batched
    // glDrawArrays; only the frame-constant clip and flashbang wash stay as uniforms.
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
        kAttrPos,    // vec2 screen position
        kAttrShape,  // vec4 shape, feather, radius, len
        kAttrCenter, // vec4 center.xy, dir.xy
        kAttrColor,  // vec4 rgba (0..1)
        kAttrTri,    // vec4 t0.xy, t1.xy
        kAttrTri2,   // vec4 t2.xy, spread, pad
        kAttrCount,
    };
    constexpr const char* kPrimAttribs[kAttrCount] = {
        "aPos", "aShape", "aCenter", "aColor", "aTri", "aTri2",
    };

    // Backdrop blur program: samples a captured framebuffer region at a high mip.
    enum
    {
        kBlurCenter,
        kBlurRadius,
        kBlurFeather,
        kBlurSquare,
        kBlurLod,
        kBlurStep,
        kBlurDarken,
        kBlurAlpha,
        kBlurFadeColor,
        kBlurFadeAmt,
        kBlurCount,
    };
    constexpr const char* kBlurUniforms[kBlurCount] = {
        "uCenter", "uRadius", "uFeather", "uSquare", "uLod", "uStep",
        "uDarken", "uAlpha", "uFadeColor", "uFadeAmt",
    };

    static_assert(kMaskCount <= ncl_gl::kMaxUniforms && kPrimCount <= ncl_gl::kMaxUniforms && kBlurCount <= ncl_gl::kMaxUniforms);

    ncl_gl::Program g_Mask;
    ncl_gl::Program g_Prim;
    ncl_gl::Program g_Blur;

    // Flashbang wash: every radar primitive blends its colour toward this by the
    // given amount, so the radar (interior, border and the markers that stick out
    // past the rim) is blinded together with the 3D view, with no overlay fill and
    // therefore no halo on the backdrop. Cleared to amount 0 outside a flash.
    ncl_math::ColorF g_FadeColor;
    float g_FadeAmt;

    // Clip shape for batched primitives (radar boundary). Default: effectively no clip.
    ncl_math::Vector2 g_ClipCenter;
    float g_ClipR = 1.0e9f;
    bool g_ClipSquare;

    // Program bind + blend cache (uniform values persist per program object in GL).
    GLuint g_CurrentProgram;
    GLenum g_BlendSrc, g_BlendDst;
    bool g_BlendSet; // false = unknown (the engine may have changed blend state)

    // Attribute locations, resolved with the prim program.
    GLint g_PrimAttr[kAttrCount];

    // Deferred quad batch for the prim program: each primitive appends one quad
    // instead of drawing immediately, and the run is flushed as one glDrawArrays when
    // the blend mode or clip shape changes, a prim region ends, or any non-batched
    // draw intervenes. This collapses a run of primitives into one call while
    // preserving draw order exactly.
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
        bool open; // a run with a fixed blend/clip is being collected
        GLenum src, dst; // blend of the pending run
        ncl_math::Vector2 clip_center; // clip of the pending run
        float clip_radius = 1.0e9f;
        float clip_square;
        GLuint tex; // glyph atlas sampled by the run, 0 when it draws no glyphs
    };
    PrimBatch g_Batch;
    int g_PrimRegion; // >0 defers the unbind to the outermost EndPrims

    RadarDrawStats g_Stats;

    // Blur capture (see DrawBlurBackground): the scene region is resolved 1:1 into
    // g_BgTex (a multisample resolve blit must be 1:1), then filtered-downsampled into
    // the much smaller g_BgSmall, and only g_BgSmall is mipmapped and sampled. This keeps
    // the per-frame mip generation on a fraction of the pixels for the same blur look.
    // The shader LOD drops by log2(downscale) to sample the same spatial extent.
    constexpr int kBlurDownscale = 4;
    constexpr float kBlurSampleLod = 3.0f; // 5.0 (full-size LOD) - log2(kBlurDownscale)
    GLuint g_BgTex;
    GLuint g_BgFbo; // 1:1 resolve target
    GLuint g_BgSmall;
    GLuint g_BgSmallFbo; // downsample target, sampled by the blur shader
    int g_BgTexW, g_BgTexH; // current capture (g_BgTex) size

    // Shader sources are embedded from assets/shaders via CMakeRC (see CMakeLists).
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
        ncl_gl::LoadProgram(g_Mask, vs_src.c_str(), mask_src.c_str(), kMaskUniforms, kMaskCount, nullptr, 0, &error_log);

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
        ncl_gl::LoadProgram(g_Blur, vs_src.c_str(), backdrop_src.c_str(), kBlurUniforms, kBlurCount, nullptr, 0, &error_log);

        if (!g_Blur.ok)
        {
            gEngfuncs.Con_DPrintf("radar backdrop shader failed: %s\n", error_log.c_str());
        }

        if (g_Blur.ok)
        {
            glUseProgram(g_Blur.prog);
            glUniform1i(glGetUniformLocation(g_Blur.prog, "uTex"), 0);
            glUseProgram(0);

            // FBO entry points come from GL 3.0 / ARB_framebuffer_object; a GL 2.x
            // driver exposing only EXT_framebuffer_object leaves them null under glad,
            // so the blur capture (glBlitFramebuffer + glGenerateMipmap) cannot run.
            // Leave the blur disabled in that case and fall back to the plain backdrop.
            if (glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D && glBlitFramebuffer && glGenerateMipmap)
            {
                glGenTextures(1, &g_BgTex);
                glGenFramebuffers(1, &g_BgFbo);
                glGenTextures(1, &g_BgSmall);
                glGenFramebuffers(1, &g_BgSmallFbo);
            }
            else
            {
                g_Blur.ok = false;
            }
        }
    }

    bool PrimReady()
    {
        if (!g_ProgramDone)
        {
            Init();
        }

        return g_Prim.ok;
    }

    // Enables blending with the given factors, skipping the GL calls when they already
    // match the cached state. Safe because UseProgram(0) invalidates the cache whenever
    // control returns to the engine, so a stale cache never survives an engine draw.
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

    // Draws the pending primitive run as one glDrawArrays and clears it. Binds the prim
    // program directly (UseProgram would recurse here); the caller rebinds afterwards.
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
            // The engine's multitexture surface path can leave unit 1 current, and the
            // sampler is bound to unit 0.
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
        // Any hand-off to another program or to the engine must first draw the primitives
        // queued so far, so they land in call order under whatever draws next.
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
            // Handed back to the engine / fixed function, which may change blend state.
            g_BlendSet = false;
        }
    }

    void ApplyFade(GLint color_loc, GLint amt_loc)
    {
        glUniform3f(color_loc, g_FadeColor.r, g_FadeColor.g, g_FadeColor.b);
        glUniform1f(amt_loc, g_FadeAmt);
    }

    // Appends one quad (four explicit corner vertices) to the pending run, flushing
    // first when the blend mode or clip shape differs from the run in progress.
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

    // Shape selectors of the prim program (the vShape.x dispatch in radar_prim.frag).
    constexpr float kShapeDisc = 0.0f;
    constexpr float kShapeCone = 1.0f;
    constexpr float kShapeTriangle = 2.0f;
    constexpr float kShapeCross = 3.0f;
    constexpr float kShapeRing = 4.0f;
    constexpr float kShapeSolid = 5.0f;
    constexpr float kShapeGlyph = 6.0f;

    // Parameters of one analytic shape quad. Only the fields a shape reads need to be
    // set; radius and len are overloaded per shape exactly as in radar_prim.frag.
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
        ncl_math::Vector2 t0, t1, t2; // triangle vertices
    };

    // Appends one analytic shape quad covering the given screen rect. The four corners
    // share every per-shape attribute, so the fragment shader reads them as constants.
    void AddShapeQuad(const ShapeParams& p, GLenum blend_src, GLenum blend_dst, ncl_math::Aabb2 quad)
    {
        if (!PrimReady())
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

    // Minimum polyline segment length (HUD pixels): ClampToRadar can squash distinct
    // trail points into sub-pixel deltas whose direction is noise for the miter.
    constexpr float kMinSegmentLen = ncl_math::kNearZero;

    // Corner offset at the shared point of two polyline segments (unit directions
    // d0, d1): along the perpendicular of the joint bisector, scaled so both segment
    // quads meet edge-to-edge (miter), clamped to twice the half-width so a sharp
    // turn cannot spike.
    ncl_math::Vector2 MiterOffset(ncl_math::Vector2 d0, ncl_math::Vector2 d1, float half_width)
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

bool RadarDraw_Available()
{
    if (!g_ProgramDone)
    {
        Init();
    }

    return g_Mask.ok;
}

bool RadarDraw_PrimsAvailable()
{
    if (!g_ProgramDone)
    {
        Init();
    }

    return g_Prim.ok;
}

void RadarDraw_ResetGl()
{
    // A video mode change can recreate the GL context, leaving the cached program and
    // texture handles dangling. Drop them so the next use re-creates everything against
    // the current context. Deleting a handle from a destroyed context is a no-op, so
    // this is safe whether or not the context actually changed.
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

        if (g_BgTex)
        {
            glDeleteTextures(1, &g_BgTex);
        }

        if (g_BgFbo)
        {
            glDeleteFramebuffers(1, &g_BgFbo);
        }

        if (g_BgSmall)
        {
            glDeleteTextures(1, &g_BgSmall);
        }

        if (g_BgSmallFbo)
        {
            glDeleteFramebuffers(1, &g_BgSmallFbo);
        }
    }

    g_ProgramDone = false;
    g_Mask = ncl_gl::Program{};
    g_Prim = ncl_gl::Program{};
    g_Blur = ncl_gl::Program{};
    g_BgTex = 0;
    g_BgFbo = 0;
    g_BgSmall = 0;
    g_BgSmallFbo = 0;
    g_BgTexW = 0;
    g_BgTexH = 0;
    g_CurrentProgram = 0;
    g_Batch = PrimBatch{};
    g_BlendSet = false;
}

void RadarDraw_ResetStats()
{
    g_Stats = RadarDrawStats{};
}

RadarDrawStats RadarDraw_Stats()
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
    // The fade is applied per batch at flush time, so commit any primitives already
    // queued under the current fade before changing it; otherwise the change would
    // recolour them retroactively.
    RadarDraw_FlushPrims();
    g_FadeColor = ncl_math::ColorF{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    g_FadeAmt = std::clamp(color.a / 255.0f, 0.0f, 1.0f);
}

void RadarDraw_ClearFade()
{
    RadarDraw_FlushPrims();
    g_FadeAmt = 0.0f;
}

ncl_math::Color RadarDraw_FadeColor(ncl_math::Color color)
{
    if (g_FadeAmt <= 0.0f)
    {
        return color;
    }

    // Blend toward the fade colour, carrying the source alpha through (Lerp blends all
    // four channels).
    return ncl_math::Lerp(color, g_FadeColor.ToBytes().WithAlpha(color.a), g_FadeAmt);
}

void RadarDraw_ClampBoundTexture()
{
    if (!EnsureGlad())
    {
        return;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void RadarDraw_DrawPolyline(const ncl_math::Vector2* points, const uint8_t* alpha, int count, ncl_math::Color color, float width)
{
    if (count < 2 || !PrimReady())
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

        if (len < kMinSegmentLen) // duplicate point (stationary samples): no segment, break the miter chain
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

        // Both quads at a joint derive the same miter from the same two directions,
        // so adjacent segments share the edge exactly: no gap and no double blend.
        ncl_math::Vector2 start_off = has_prev ? MiterOffset(prev_dir, dir, half) : dir.Perp() * half;
        ncl_math::Vector2 end_off = has_next ? MiterOffset(dir, next_dir, half) : dir.Perp() * half;

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
    // Axis-aligned solid quad: the geometry is the shape, so kShapeSolid samples no distance
    // field. Colour rides per vertex and the flashbang wash applies in the fragment shader.
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
        {.shape = kShapeRing, .feather = 1.1f, .center = center, .radius = outer, .len = inner,
         .color = color, .alpha = color.a / 255.0f},
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
        {{center.x - m, center.y - m}, {center.x + m, center.y + m}}
    );
}

void RadarDraw_DrawBorderRing(ncl_math::Vector2 center, float outer, float inner, float feather, ncl_math::Color color)
{
    if (RadarDraw_BeginRing(center, outer, inner, feather, color.ToFloat(), false))
    {
        SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float m = outer + 2.0f;
        glBegin(GL_QUADS);
        glVertex2f(center.x - m, center.y - m);
        glVertex2f(center.x + m, center.y - m);
        glVertex2f(center.x + m, center.y + m);
        glVertex2f(center.x - m, center.y + m);
        glEnd();
        RadarDraw_End();
        return;
    }

    if (!EnsureGlad())
    {
        return;
    }

    // No mask program: rasterize the circle directly with 1px fixed-function quads
    // (eighth-circle symmetry), the FillRGBABlend way minus the engine dependency.
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
    // Additive blend: the cone is a light wash over the map.
    AddShapeQuad(
        {.shape = kShapeCone, .feather = 1.5f, .center = apex, .len = length, .dir = dir, .spread = spread,
         .color = color, .alpha = peak_alpha},
        GL_SRC_ALPHA, GL_ONE, bb
    );
}

bool RadarDraw_DrawBlurBackground(
    float box_x,
    float box_y,
    float box_size,
    bool square,
    float darken,
    float alpha,
    float hud_w,
    float hud_h
)
{
    if (!g_ProgramDone)
    {
        Init();
    }

    if (!g_Blur.ok)
    {
        return false;
    }

    int isize = (int)box_size;

    if (isize < 4)
    {
        return false;
    }

    std::chrono::steady_clock::time_point blur_start = std::chrono::steady_clock::now();

    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);

    // box_* are HUD coordinates (GetScreenInfo space), which the engine's 2D ortho
    // maps onto the whole GL viewport. When that space differs from the real
    // framebuffer resolution (HUD scaling, windowed/native mismatch) a 1:1 copy grabs
    // the wrong region, or an off-screen one that reads back black. hud_w/hud_h carry
    // that HUD space (GetScreenInfo == the ortho size the engine set), so the captured
    // rect is scaled by viewport/HUD without reading the projection matrix back every
    // frame. Fall back to a 1:1 copy if the HUD size is unavailable.
    if (hud_w <= 0.0f)
    {
        hud_w = (float)vp[2];
    }

    if (hud_h <= 0.0f)
    {
        hud_h = (float)vp[3];
    }

    float sx = (float)vp[2] / hud_w;
    float sy = (float)vp[3] / hud_h;

    int cap_x = vp[0] + (int)(box_x * sx + 0.5f);
    int cap_y = vp[1] + vp[3] - (int)((box_y + box_size) * sy + 0.5f);
    int cap_w = (int)(box_size * sx + 0.5f);
    int cap_h = (int)(box_size * sy + 0.5f);
    cap_x = std::max(cap_x, vp[0]);
    cap_y = std::max(cap_y, vp[1]);
    cap_w = std::max(cap_w, 4);
    cap_h = std::max(cap_h, 4);
    cap_w = std::min(cap_w, vp[0] + vp[2] - cap_x);
    cap_h = std::min(cap_h, vp[1] + vp[3] - cap_y);

    glActiveTexture(GL_TEXTURE0);
    GLint prev_tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);

    // Where is the scene actually being rendered? The reliable, driver-independent
    // answer is GL_DRAW_FRAMEBUFFER_BINDING: 0 means the default framebuffer (the
    // back buffer), any non-zero value is an FBO. Fullscreen and -stretchaspect
    // render the scene into an FBO that is blitted to the screen later, so the
    // scene the radar is drawn over lives in that FBO, not the default buffer.
    GLint prev_read_fb = 0, prev_draw_fb = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fb);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fb);

    // Capture by blitting from that framebuffer into our own. Unlike glCopyTexImage2D
    // this reads from an explicit source and resolves multisampling, so it works for
    // the default buffer, a plain FBO, and a multisampled FBO alike. The resolve blit
    // must be 1:1, so size the destination texture to the captured region.
    int small_w = std::max(cap_w / kBlurDownscale, 1);
    int small_h = std::max(cap_h / kBlurDownscale, 1);

    if (g_BgTexW != cap_w || g_BgTexH != cap_h)
    {
        // 1:1 resolve target: read only (via a blit), never sampled, so no mipmaps.
        glBindTexture(GL_TEXTURE_2D, g_BgTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cap_w, cap_h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, g_BgFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_BgTex, 0);

        // Downsample target: mipmapped and sampled by the blur shader.
        glBindTexture(GL_TEXTURE_2D, g_BgSmall);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, small_w, small_h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, g_BgSmallFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_BgSmall, 0);

        glBindTexture(GL_TEXTURE_2D, 0);
        g_BgTexW = cap_w;
        g_BgTexH = cap_h;
    }

    // 1:1 multisample resolve of the scene region into g_BgTex.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_draw_fb);
    GLint src_read_buf = GL_BACK;
    glGetIntegerv(GL_READ_BUFFER, &src_read_buf);
    glReadBuffer(prev_draw_fb == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_BgFbo);
    glBlitFramebuffer(cap_x, cap_y, cap_x + cap_w, cap_y + cap_h, 0, 0, cap_w, cap_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glReadBuffer(src_read_buf);

    // Filtered downsample of the resolved capture into the small texture; only it is
    // mipmapped and sampled, so the per-frame mip generation runs on far fewer pixels.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_BgFbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_BgSmallFbo);
    glBlitFramebuffer(0, 0, cap_w, cap_h, 0, 0, small_w, small_h, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fb);

    glBindTexture(GL_TEXTURE_2D, g_BgSmall);
    glGenerateMipmap(GL_TEXTURE_2D);

    UseProgram(g_Blur.prog);
    SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniform2f(g_Blur.loc[kBlurCenter], box_x + box_size * 0.5f, box_y + box_size * 0.5f);
    glUniform1f(g_Blur.loc[kBlurRadius], box_size * 0.5f);
    glUniform1f(g_Blur.loc[kBlurFeather], 1.5f);
    glUniform1f(g_Blur.loc[kBlurSquare], square ? 1.0f : 0.0f);
    glUniform1f(g_Blur.loc[kBlurLod], kBlurSampleLod);
    glUniform1f(g_Blur.loc[kBlurStep], 0.09f);
    glUniform1f(g_Blur.loc[kBlurDarken], darken);
    glUniform1f(g_Blur.loc[kBlurAlpha], alpha);
    ApplyFade(g_Blur.loc[kBlurFadeColor], g_Blur.loc[kBlurFadeAmt]);

    float x0 = box_x, y0 = box_y, x1 = box_x + box_size, y1 = box_y + box_size;
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x1, y0);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x1, y1);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x0, y1);
    glEnd();

    UseProgram(0);
    // Restore the engine's binding: hw.dll caches the bound texture name (GL_Bind
    // early-outs on a cache hit), so leaving anything else bound desyncs that cache.
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);

    g_Stats.blur_ms += std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - blur_start).count();

    return true;
}

bool RadarDraw_Begin(ncl_math::Vector2 center, float radius, float feather, float gamma, bool square)
{
    if (!RadarDraw_Available())
    {
        return false;
    }

    UseProgram(g_Mask.prog);
    glUniform1f(g_Mask.loc[kMaskMode], 0.0f);
    glUniform2f(g_Mask.loc[kMaskCenter], center.x, center.y);
    glUniform1f(g_Mask.loc[kMaskRadius], radius);
    glUniform1f(g_Mask.loc[kMaskFeather], feather);
    glUniform1f(g_Mask.loc[kMaskGamma], gamma);
    glUniform1f(g_Mask.loc[kMaskSquare], square ? 1.0f : 0.0f);
    ApplyFade(g_Mask.loc[kMaskFadeColor], g_Mask.loc[kMaskFadeAmt]);

    return true;
}

bool RadarDraw_BeginRing(
    ncl_math::Vector2 center,
    float outer,
    float inner,
    float feather,
    ncl_math::ColorF color,
    bool square
)
{
    if (!RadarDraw_Available())
    {
        return false;
    }

    UseProgram(g_Mask.prog);
    glUniform1f(g_Mask.loc[kMaskMode], 1.0f);
    glUniform2f(g_Mask.loc[kMaskCenter], center.x, center.y);
    glUniform1f(g_Mask.loc[kMaskRadius], outer);
    glUniform1f(g_Mask.loc[kMaskRingInner], inner);
    glUniform1f(g_Mask.loc[kMaskFeather], feather);
    glUniform1f(g_Mask.loc[kMaskSquare], square ? 1.0f : 0.0f);
    glUniform4f(g_Mask.loc[kMaskRingColor], color.r, color.g, color.b, color.a);
    ApplyFade(g_Mask.loc[kMaskFadeColor], g_Mask.loc[kMaskFadeAmt]);

    return true;
}

void RadarDraw_End()
{
    UseProgram(0);
}

void RadarDraw_DrawMapTexture(
    unsigned int tex,
    const float* xy,
    const float* uv,
    ncl_math::ColorF color,
    ncl_math::Vector2 center,
    float radius,
    bool square
)
{
    if (!RadarDraw_Available())
    {
        return;
    }

    UseProgram(g_Mask.prog);
    glUniform1f(g_Mask.loc[kMaskMode], 0.0f);
    glUniform2f(g_Mask.loc[kMaskCenter], center.x, center.y);
    glUniform1f(g_Mask.loc[kMaskRadius], radius);
    glUniform1f(g_Mask.loc[kMaskFeather], 1.5f);
    glUniform1f(g_Mask.loc[kMaskGamma], 0.0f);
    glUniform1f(g_Mask.loc[kMaskSquare], square ? 1.0f : 0.0f);
    ApplyFade(g_Mask.loc[kMaskFadeColor], g_Mask.loc[kMaskFadeAmt]);

    glActiveTexture(GL_TEXTURE0);
    GLint prev_tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);

    for (int i = 0; i < 4; i++)
    {
        glTexCoord2f(uv[i * 2], uv[i * 2 + 1]);
        glVertex2f(xy[i * 2], xy[i * 2 + 1]);
    }

    glEnd();

    UseProgram(0);
    // See DrawBlurBackground: restore rather than unbind, for the engine's bind cache.
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    glColor4ub(255, 255, 255, 255);
}
