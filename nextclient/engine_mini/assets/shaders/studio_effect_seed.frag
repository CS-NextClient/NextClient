#version 330 compatibility
#include "material_occlusion.glsl"
uniform sampler2D uShape;
uniform ivec2 uOrigin;
uniform float uDepthSign;
layout(location = 0) out ivec2 outSeed;
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    ivec2 local = p - uOrigin;
    ivec2 size = textureSize(uShape, 0);
    outSeed = ivec2(-1);
    if (any(lessThan(local, ivec2(0))) || any(greaterThanEqual(local, size)))
    {
        return;
    }
    vec4 shape = texelFetch(uShape, local, 0);
    if (shape.a == 0.0 || shape.x <= 0.0)
    {
        return;
    }
    bool occluded_only = shape.a < 1.5;
    bool visible_only = shape.a > 2.5;
    if (occluded_only || visible_only)
    {
        bool hidden = MaterialOcclusion_IsHidden(local, p, uDepthSign);
        if ((occluded_only && !hidden) || (visible_only && hidden))
        {
            return;
        }
    }
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            ivec2 q = local + ivec2(x, y);
            if (any(lessThan(q, ivec2(0))) || any(greaterThanEqual(q, size)) || texelFetch(uShape, q, 0).a == 0.0)
            {
                outSeed = p;
                return;
            }
        }
    }
}
