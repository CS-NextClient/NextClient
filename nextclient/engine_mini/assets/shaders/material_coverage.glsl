uniform sampler2D uDissolveMap;
uniform vec4 uDissolveTransform;
uniform float uDissolve;
uniform float uDissolveSpace;
uniform float uNoise;
uniform float uNoiseSpace;
uniform float uNoiseScale;
uniform float uNoiseSeed;
uniform float uNoiseSpeed;
#include "material_effect_clock.glsl"

vec2 MaterialCoverage_Coordinates(vec2 uv, float space)
{
    return space > 0.5 ? (gl_FragCoord.xy - uEffectViewport.xy) / max(uEffectViewport.w, 1.0) : uv;
}

uint MaterialCoverage_Hash(uvec3 cell)
{
    uint value = cell.x * 0x9e3779b9u ^ cell.y * 0x85ebca6bu ^ cell.z * 0xc2b2ae35u;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

// Coverage evaluation keeps helper fragments alive for subsequent texture gradients and derivatives.
bool MaterialCoverage_Evaluate(vec2 uv)
{
    bool visible = true;

    if (uDissolve > 0.0)
    {
        if (uDissolve >= 1.0)
        {
            visible = false;
        }
        else
        {
            vec2 coordinate = MaterialCoverage_Coordinates(uv, uDissolveSpace);
            float threshold = texture(uDissolveMap, coordinate * uDissolveTransform.xy + uDissolveTransform.zw).r;
            visible = !(threshold <= uDissolve);
        }
    }

    if (uNoise > 0.0)
    {
        if (uNoise >= 1.0)
        {
            visible = false;
        }
        else
        {
            vec2 coordinate = MaterialCoverage_Coordinates(uv, uNoiseSpace);
            uvec2 cell = uvec2(ivec2(floor(coordinate * max(uNoiseScale, 1.0))));
            uint seed = uint(uNoiseSeed) + uint(floor(max(uEffectTime, 0.0) * uNoiseSpeed));
            float threshold = float(MaterialCoverage_Hash(uvec3(cell, seed))) / 4294967295.0;
            visible = visible && !(threshold < uNoise);
        }
    }

    return visible;
}
