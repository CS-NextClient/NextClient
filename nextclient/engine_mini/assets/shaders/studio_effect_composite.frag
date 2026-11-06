#version 330 compatibility
#include "material_occlusion.glsl"
uniform sampler2D uFill;
uniform sampler2D uOutline;
uniform sampler2D uShape;
uniform isampler2D uSeeds;
uniform ivec2 uOrigin;
uniform ivec2 uObjectDepthOffset;
uniform ivec2 uMaskSize;
uniform ivec2 uSeedOrigin;
uniform ivec2 uSeedSize;
uniform int uOutlinePath;
uniform ivec2 uOutputOrigin;
uniform float uDepthSign;
out vec4 outColor;
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy) - uOutputOrigin;
    ivec2 local = p - uOrigin;
    ivec2 size = uMaskSize;
    bool inside = all(greaterThanEqual(local, ivec2(0))) && all(lessThan(local, size));
    vec4 shape = inside ? texelFetch(uShape, local, 0) : vec4(0.0);
    outColor = vec4(0.0);
    if (shape.a != 0.0)
    {
        if (MaterialOcclusion_IsHidden(p + uObjectDepthOffset, p, uDepthSign))
        {
            outColor = texelFetch(uFill, local, 0);
        }

        return;
    }

    if (inside && uSamples > 0 && MaterialOcclusion_HasSampleCoverage(p + uObjectDepthOffset, uDepthSign))
    {
        return;
    }

    if (uOutlinePath == 0)
    {
        return;
    }

    ivec2 seed_pixel = p - uSeedOrigin;
    ivec2 seed = ivec2(-1);
    if (uOutlinePath == 1)
    {
        float best = 1e30;
        for (int y = -2; y <= 2; y++)
        {
            for (int x = -2; x <= 2; x++)
            {
                ivec2 q = seed_pixel + ivec2(x, y);
                if (any(lessThan(q, ivec2(0))) || any(greaterThanEqual(q, uSeedSize)))
                {
                    continue;
                }

                ivec2 candidate = texelFetch(uSeeds, q, 0).rg;
                float distance_squared = dot(vec2(candidate - p), vec2(candidate - p));
                if (candidate.x >= 0 && distance_squared < best)
                {
                    best = distance_squared;
                    seed = candidate;
                }
            }
        }
    }
    else
    {
        seed = texelFetch(uSeeds, seed_pixel, 0).rg;
    }

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
