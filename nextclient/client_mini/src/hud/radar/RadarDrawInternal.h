#pragma once

#include <glad/glad.h>
#include <ncl_gl/program.h>

// Shared by the RadarDraw translation units: the compiled programs, the GL state cache and the
// fade both the batch and the effects drive. Not part of the module's API.
namespace radar_draw
{
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

    enum
    {
        kBlurCenter,
        kBlurRadius,
        kBlurFeather,
        kBlurSquare,
        kBlurDirection,
        kBlurFilterPass,
        kBlurDarken,
        kBlurAlpha,
        kBlurFadeColor,
        kBlurFadeAmt,
        kBlurCount,
    };

    bool EnsureGlad();

    // Compiles the programs on the first call; every availability check goes through it.
    void EnsurePrograms();

    ncl_gl::Program& GetMaskProgram();
    ncl_gl::Program& GetBlurProgram();

    // Hands the batch off: queued primitives are drawn before another program takes over.
    void UseProgram(GLuint p);
    void SetBlend(GLenum src, GLenum dst);

    // Writes the current wash into a program's two fade uniforms.
    void ApplyFade(GLint color_loc, GLint amt_loc);

    void AddBlurMs(float ms);

    // Creates the capture targets the blur needs; clears blur.ok when the driver has no FBOs.
    void InitEffectsGl(ncl_gl::Program& blur);

    // Deletes the textures and framebuffers the effects own, for RadarDraw_ResetGl.
    void ResetEffectsGl();
} // namespace radar_draw
