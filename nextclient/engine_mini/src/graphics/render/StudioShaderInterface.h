#pragma once

#include "graphics/render/shader_manager.h"

namespace render
{
    enum StudioUniform
    {
        kUniformTexelScale = 0,
        kUniformAmbient,
        kUniformShade,
        kUniformColorMix,
        kUniformLightVec,
        kUniformShadeMode,
        kUniformChrome,
        kUniformChromeOrigin,
        kUniformViewRight,
        kUniformBlend,
        kUniformGlowScale,
        kUniformShellColor,
        kUniformEntityLightCount,
        kUniformEntityLight,
        kUniformEntityLightColor,
        kUniformGamma,
        kUniformFlipped,
        kUniformBoneScale,
        kUniformEffectViewport,
        kUniformEffectTime,
        kUniformFogMode,
        kUniformFogColor,
        kUniformFogParams,
        kUniformCount
    };

    inline constexpr const char* kStudioUniformNames[kUniformCount] = {
        "uTexelScale",       "uAmbient",   "uShade",   "uColorMix",  "uLightVec",       "uShadeMode",        "uChrome",
        "uChromeOrigin",     "uViewRight", "uBlend",   "uGlowScale", "uShellColor",     "uEntityLightCount", "uEntityLight",
        "uEntityLightColor", "uGamma",     "uFlipped", "uBoneScale", "uEffectViewport", "uEffectTime",       "uFogMode",
        "uFogColor",         "uFogParams"
    };

    static_assert(kUniformCount <= ncl_gl::kMaxUniforms, "the program's location array is fixed and is written without a bound");

    inline constexpr int kStudioLightGammaEntries = 256;

    // Both gamma tables hold 1024 entries and are addressed by an 8-bit value.
    inline constexpr int kStudioGammaTableStride = 4;

    // One numeric source feeds both CPU shade selection and the GLSL prologue.
#define NCL_SHADE_LIT 0
#define NCL_SHADE_FLAT 1
#define NCL_SHADE_FULLBRIGHT 2
#define NCL_SHADE_ADDITIVE 3
#define NCL_SHADE_GLOW_SHELL 4
#define NCL_SHADE_STR2(value) #value
#define NCL_SHADE_STR(value) NCL_SHADE_STR2(value)

    inline constexpr int kShadeLit = NCL_SHADE_LIT;
    inline constexpr int kShadeFlat = NCL_SHADE_FLAT;
    inline constexpr int kShadeFullbright = NCL_SHADE_FULLBRIGHT;
    inline constexpr int kShadeAdditive = NCL_SHADE_ADDITIVE;
    inline constexpr int kShadeGlowShell = NCL_SHADE_GLOW_SHELL;

    inline constexpr const char* kStudioShadeDefines = "#define kShadeLit " NCL_SHADE_STR(
        NCL_SHADE_LIT
    ) "\n"
      "#define kShadeFlat " NCL_SHADE_STR(
          NCL_SHADE_FLAT
      ) "\n"
        "#define kShadeFullbright " NCL_SHADE_STR(
            NCL_SHADE_FULLBRIGHT
        ) "\n"
          "#define kShadeAdditive " NCL_SHADE_STR(
              NCL_SHADE_ADDITIVE
          ) "\n"
            "#define kShadeGlowShell " NCL_SHADE_STR(NCL_SHADE_GLOW_SHELL) "\n";

#undef NCL_SHADE_STR
#undef NCL_SHADE_STR2
#undef NCL_SHADE_GLOW_SHELL
#undef NCL_SHADE_ADDITIVE
#undef NCL_SHADE_FULLBRIGHT
#undef NCL_SHADE_FLAT
#undef NCL_SHADE_LIT

    inline constexpr ProgramDesc kStudioBuiltinProgram =
        {"studio.vert", "studio_legacy.frag", kStudioShadeDefines, kStudioUniformNames, kUniformCount, nullptr, 0};

} // namespace render
