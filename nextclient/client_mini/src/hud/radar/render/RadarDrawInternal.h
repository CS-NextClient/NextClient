#pragma once

#include <glad/glad.h>
#include <ncl_gl/program.h>

// Shared by the RadarDraw translation units: the compiled programs, the GL state cache and the
// fade both the batch and the effects drive. Not part of the module's API.
namespace RadarDrawUniform
{
    enum
    {
        kMaskMode,
        kMaskCenter,
        kMaskRadius,
        kMaskFeather,
        kMaskGamma,
        kMaskSquare,
        kMaskPremultiplied,
        kMaskAlphaAa,
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
} // namespace RadarDrawUniform

bool RadarDrawInternal_EnsureGlad();

// Compiles the programs on the first call.
void RadarDrawInternal_EnsurePrograms();

ncl_gl::Program& RadarDrawInternal_GetMaskProgram();
ncl_gl::Program& RadarDrawInternal_GetBlurProgram();

// Hands the batch off: queued primitives are drawn before another program takes over.
void RadarDrawInternal_UseProgram(GLuint p);
void RadarDrawInternal_SetBlend(GLenum src, GLenum dst);

// Writes the current wash into a program's two fade uniforms.
void RadarDrawInternal_ApplyFade(GLint color_loc, GLint amt_loc);

void RadarDrawInternal_AddBlurMs(float ms);

// Creates the capture targets the blur needs; clears blur.ok when the driver has no FBOs.
void RadarDrawInternal_InitEffectsGl(ncl_gl::Program& blur);

// Deletes the owned effect textures and framebuffers.
void RadarDrawInternal_ResetEffectsGl();
