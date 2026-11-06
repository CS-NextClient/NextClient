#include "RadarDraw.h"

#include <algorithm>
#include <array>

#include "main.h"

#include "hud/radar/integration/Performance.h"

#include <glad/glad.h>
#include <ncl_gl/loader.h>
#include <ncl_math/aabb.h>
#include <ncl_math/vec3.h>

#include "hud/radar/render/RadarDrawInternal.h"

namespace
{
    // Multisample resolve must be 1:1; filtering runs on separate half-resolution targets.
    constexpr int kBlurDownscale = 2;

    struct BlurPass
    {
        GLuint target;
        GLuint source;
        ncl_math::Vector2 direction;
    };

} // namespace

void RadarDrawContext::InitEffectsGl(ncl_gl::Program& blur)
{
    // A GL 2.x driver exposing only EXT_framebuffer_object leaves these null under
    // glad, so the blur capture cannot run: fall back to the plain backdrop.
    if (glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D && glBlitFramebuffer)
    {
        glGenTextures(1, &bg_tex_);
        glGenFramebuffers(1, &bg_fbo_);
        glGenTextures(1, &bg_small_);
        glGenFramebuffers(1, &bg_small_fbo_);
        glGenTextures(1, &bg_temp_);
        glGenFramebuffers(1, &bg_temp_fbo_);
        return;
    }

    blur.ok = false;
}

void RadarDrawContext::ResetEffectsGl(bool release)
{
    if (release && ncl_gl::IsLoaded())
    {
        if (bg_tex_)
        {
            glDeleteTextures(1, &bg_tex_);
        }

        if (bg_fbo_)
        {
            glDeleteFramebuffers(1, &bg_fbo_);
        }

        if (bg_small_)
        {
            glDeleteTextures(1, &bg_small_);
        }

        if (bg_small_fbo_)
        {
            glDeleteFramebuffers(1, &bg_small_fbo_);
        }

        if (bg_temp_)
        {
            glDeleteTextures(1, &bg_temp_);
        }

        if (bg_temp_fbo_)
        {
            glDeleteFramebuffers(1, &bg_temp_fbo_);
        }
    }

    bg_tex_ = 0;
    bg_fbo_ = 0;
    bg_small_ = 0;
    bg_small_fbo_ = 0;
    bg_temp_ = 0;
    bg_temp_fbo_ = 0;
    bg_tex_w_ = 0;
    bg_tex_h_ = 0;
}

bool RadarDrawContext::CheckMaskAvailable()
{
    EnsurePrograms();

    return GetMaskProgram().ok;
}

void RadarDrawContext::ClampBoundTexture()
{
    if (!ncl_gl::EnsureLoaded() || GLAD_GL_VERSION_2_0 == 0)
    {
        return;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool RadarDrawContext::DrawBlurBackground(
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
    EnsurePrograms();

    ncl_gl::Program& blur = GetBlurProgram();

    if (!blur.ok)
    {
        return false;
    }

    performance::PerformanceMonitorInterface* monitor = performance_.get_monitor();
    int isize = (int)box_size;

    if (isize < 4)
    {
        return false;
    }

    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);

    // box_* are HUD coordinates (GetScreenInfo space) that the engine's 2D ortho stretches
    // over the whole viewport, so the capture rect is scaled by viewport/HUD. A raw 1:1 copy
    // grabs the wrong region, or an off-screen one that reads back black.
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

    // See FlushBatch: the engine can leave unit 1 current, and its bind cache tracks one.
    GLint prev_unit = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
    glActiveTexture(GL_TEXTURE0);
    GLint prev_tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);

    // Fullscreen and -stretchaspect render the scene into an FBO blitted to the screen
    // later, so the source is GL_DRAW_FRAMEBUFFER_BINDING (0 = default buffer), not GL_BACK.
    GLint prev_read_fb = 0, prev_draw_fb = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fb);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fb);

    // Blit, not glCopyTexImage2D: it reads from an explicit source and resolves multisampling.
    int small_w = std::max(cap_w / kBlurDownscale, 1);
    int small_h = std::max(cap_h / kBlurDownscale, 1);

    if (bg_tex_w_ != cap_w || bg_tex_h_ != cap_h)
    {
        // Never sampled, so no mipmaps.
        glBindTexture(GL_TEXTURE_2D, bg_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cap_w, cap_h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, bg_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg_tex_, 0);

        glBindTexture(GL_TEXTURE_2D, bg_small_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, small_w, small_h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, bg_small_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg_small_, 0);

        glBindTexture(GL_TEXTURE_2D, bg_temp_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, small_w, small_h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, bg_temp_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg_temp_, 0);

        glBindTexture(GL_TEXTURE_2D, 0);
        bg_tex_w_ = cap_w;
        bg_tex_h_ = cap_h;
    }

    GLboolean prev_scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);

    {
        performance::PerformanceGpuScope capture(monitor, performance::PerfStage::RadarBackdropCapture);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_draw_fb);
        GLint src_read_buf = GL_BACK;
        glGetIntegerv(GL_READ_BUFFER, &src_read_buf);
        glReadBuffer(prev_draw_fb == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bg_fbo_);
        glBlitFramebuffer(cap_x, cap_y, cap_x + cap_w, cap_y + cap_h, 0, 0, cap_w, cap_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glReadBuffer(src_read_buf);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, bg_fbo_);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bg_small_fbo_);
        glBlitFramebuffer(0, 0, cap_w, cap_h, 0, 0, small_w, small_h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

    GLint prev_matrix_mode = GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE, &prev_matrix_mode);
    glPushAttrib(GL_VIEWPORT_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, small_w, small_h);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    UseProgram(blur.prog);
    glUniform1f(blur.loc[RadarDrawUniform::kBlurFilterPass], 1.0f);

    const std::array<BlurPass, 2> passes{{
        {bg_temp_fbo_, bg_small_, {1.0f / small_w, 0.0f}},
        {bg_small_fbo_, bg_temp_, {0.0f, 1.0f / small_h}},
    }};

    for (const BlurPass& pass : passes)
    {
        performance::PerformanceGpuScope gpu(monitor, performance::PerfStage::RadarBackdrop);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pass.target);
        glBindTexture(GL_TEXTURE_2D, pass.source);
        glUniform2f(blur.loc[RadarDrawUniform::kBlurDirection], pass.direction.x, pass.direction.y);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(1.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(0.0f, 1.0f);
        glEnd();
    }

    UseProgram(0);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(prev_matrix_mode);
    glPopAttrib();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fb);

    if (prev_scissor)
    {
        glEnable(GL_SCISSOR_TEST);
    }

    glBindTexture(GL_TEXTURE_2D, bg_small_);
    UseProgram(blur.prog);
    glUniform1f(blur.loc[RadarDrawUniform::kBlurFilterPass], 0.0f);
    SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniform2f(blur.loc[RadarDrawUniform::kBlurCenter], box_x + box_size * 0.5f, box_y + box_size * 0.5f);
    glUniform1f(blur.loc[RadarDrawUniform::kBlurRadius], box_size * 0.5f);
    glUniform1f(blur.loc[RadarDrawUniform::kBlurFeather], 1.5f);
    glUniform1f(blur.loc[RadarDrawUniform::kBlurSquare], square ? 1.0f : 0.0f);
    glUniform1f(blur.loc[RadarDrawUniform::kBlurDarken], darken);
    glUniform1f(blur.loc[RadarDrawUniform::kBlurAlpha], alpha);
    ApplyFade(blur.loc[RadarDrawUniform::kBlurFadeColor], blur.loc[RadarDrawUniform::kBlurFadeAmt]);

    {
        performance::PerformanceGpuScope gpu(monitor, performance::PerfStage::RadarBackdrop);
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
    }

    // See FlushBatch: restore the binding for hw.dll's bind cache.
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    glActiveTexture((GLenum)prev_unit);

    return true;
}

bool RadarDrawContext::BeginMask(ncl_math::Vector2 center, float radius, float feather, float gamma, bool square)
{
    if (!CheckMaskAvailable())
    {
        return false;
    }

    ncl_gl::Program& mask = GetMaskProgram();
    UseProgram(mask.prog);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskMode], 0.0f);
    glUniform2f(mask.loc[RadarDrawUniform::kMaskCenter], center.x, center.y);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskRadius], radius);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskFeather], feather);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskGamma], gamma);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskPremultiplied], 0.0f);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskAlphaAa], 0.0f);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskSquare], square ? 1.0f : 0.0f);
    ApplyFade(mask.loc[RadarDrawUniform::kMaskFadeColor], mask.loc[RadarDrawUniform::kMaskFadeAmt]);

    return true;
}

bool RadarDrawContext::BeginMaskRing(ncl_math::Vector2 center, float outer, float inner, float feather, ncl_math::ColorF color, bool square)
{
    if (!CheckMaskAvailable())
    {
        return false;
    }

    ncl_gl::Program& mask = GetMaskProgram();
    UseProgram(mask.prog);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskMode], 1.0f);
    glUniform2f(mask.loc[RadarDrawUniform::kMaskCenter], center.x, center.y);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskRadius], outer);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskRingInner], inner);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskFeather], feather);
    glUniform1f(mask.loc[RadarDrawUniform::kMaskSquare], square ? 1.0f : 0.0f);
    glUniform4f(mask.loc[RadarDrawUniform::kMaskRingColor], color.r, color.g, color.b, color.a);
    ApplyFade(mask.loc[RadarDrawUniform::kMaskFadeColor], mask.loc[RadarDrawUniform::kMaskFadeAmt]);

    return true;
}

void RadarDrawContext::EndMask()
{
    UseProgram(0);
}

void RadarDrawContext::DrawMapTexture(
    unsigned int tex,
    std::span<const float, 8> xy,
    std::span<const float, 8> uv,
    ncl_math::ColorF color,
    ncl_math::Vector2 center,
    float radius,
    bool square,
    bool smoothing
)
{
    if (!BeginMask(center, radius, 1.5f, 0.0f, square))
    {
        return;
    }

    GLint prev_unit = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
    glActiveTexture(GL_TEXTURE0);
    GLint prev_tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    glUniform1f(GetMaskProgram().loc[RadarDrawUniform::kMaskPremultiplied], 1.0f);
    glUniform1f(GetMaskProgram().loc[RadarDrawUniform::kMaskAlphaAa], smoothing ? 1.0f : 0.0f);
    SetBlend(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(color.r * color.a, color.g * color.a, color.b * color.a, color.a);
    glBegin(GL_QUADS);

    for (int i = 0; i < 4; i++)
    {
        glTexCoord2f(uv[i * 2], uv[i * 2 + 1]);
        glVertex2f(xy[i * 2], xy[i * 2 + 1]);
    }

    glEnd();
    SetBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    EndMask();
    // See FlushBatch: restore rather than unbind, for the engine's bind cache.
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    glActiveTexture((GLenum)prev_unit);
    glColor4ub(255, 255, 255, 255);
}
