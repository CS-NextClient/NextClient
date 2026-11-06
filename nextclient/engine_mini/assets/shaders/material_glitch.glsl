#include "material_effect_clock.glsl"
uniform float uGlitchAmount;
uniform float uGlitchDisplacement;
uniform float uGlitchBands;
uniform float uGlitchRgbSplit;
uniform float uGlitchSpeed;
uniform float uGlitchSeed;

uint MaterialGlitch_Hash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

float MaterialGlitch_Offset(float height)
{
    uint band = uint(int(floor(height * max(uGlitchBands, 1.0))));
    uint frame = uint(floor(max(uEffectTime, 0.0) * max(uGlitchSpeed, 0.0)));
    uint value = MaterialGlitch_Hash(band ^ (frame + uint(uGlitchSeed)) * 0x9e3779b9u);
    float gate = float(value & 65535u) / 65536.0;
    float shift = float(MaterialGlitch_Hash(value) & 65535u) / 32767.5 - 1.0;
    return gate < uGlitchAmount ? shift * uGlitchAmount : 0.0;
}

vec4 MaterialGlitch_Position(vec4 clip)
{
    if (uGlitchAmount > 0.0 && clip.w > 0.0)
    {
        float shift = MaterialGlitch_Offset(clip.y / clip.w * 0.5 + 0.5);
        clip.x += shift * uGlitchDisplacement * 2.0 / max(uEffectViewport.z, 1.0) * clip.w;
    }
    return clip;
}
