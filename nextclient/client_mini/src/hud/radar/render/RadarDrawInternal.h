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
