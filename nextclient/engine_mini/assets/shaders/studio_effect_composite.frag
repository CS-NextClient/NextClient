#version 330 compatibility
#include "material_occlusion.glsl"
uniform sampler2D uFill;
uniform sampler2D uOutline;
uniform sampler2D uShape;
uniform isampler2D uSeeds;
uniform ivec2 uOrigin;
uniform ivec2 uOutputOrigin;
uniform float uDepthSign;
out vec4 outColor;
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy) - uOutputOrigin;
    ivec2 local = p - uOrigin;
    ivec2 size = textureSize(uShape, 0);
    bool inside = all(greaterThanEqual(local, ivec2(0))) && all(lessThan(local, size));
    vec4 shape = inside ? texelFetch(uShape, local, 0) : vec4(0.0);
    outColor = vec4(0.0);
    if (shape.a != 0.0)
    {
        if (MaterialOcclusion_IsHidden(local, p, uDepthSign))
        {
            outColor = texelFetch(uFill, local, 0);
        }
        return;
    }
    if (inside && uSamples > 0 && MaterialOcclusion_HasSampleCoverage(local, uDepthSign))
    {
        return;
    }
    ivec2 seed = texelFetch(uSeeds, p, 0).rg;
    if (seed.x < 0)
    {
        return;
    }
    vec3 style = texelFetch(uShape, seed - uOrigin, 0).rgb;
    vec4 color = texelFetch(uOutline, seed - uOrigin, 0);
    float distance = length(vec2(seed - p));
    float edge = 1.0 - smoothstep(style.x, style.x + 1.0, distance);
    float glow = style.y > 0.0 ? pow(max(0.0, 1.0 - max(distance - style.x, 0.0) / style.y), 2.0) * style.z : 0.0;
    outColor = vec4(color.rgb * (1.0 + glow), color.a * clamp(edge + glow, 0.0, 1.0));
}
