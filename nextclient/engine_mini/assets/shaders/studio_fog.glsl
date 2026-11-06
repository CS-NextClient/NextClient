in float vFogDepth;

uniform int uFogMode;
uniform vec3 uFogColor;
// Density, start and end from the captured fixed-function state.
uniform vec3 uFogParams;

const int kFogLinear = 0x2601;
const int kFogExp = 0x0800;

vec4 Studio_ApplyFog(vec4 color)
{
    if (uFogMode == 0)
    {
        return color;
    }

    float factor;
    if (uFogMode == kFogLinear)
    {
        factor = (uFogParams.z - vFogDepth) / (uFogParams.z - uFogParams.y);
    }
    else
    {
        float distance = uFogParams.x * vFogDepth;
        factor = exp(uFogMode == kFogExp ? -distance : -distance * distance);
    }

    return vec4(mix(uFogColor, color.rgb, clamp(factor, 0.0, 1.0)), color.a);
}
