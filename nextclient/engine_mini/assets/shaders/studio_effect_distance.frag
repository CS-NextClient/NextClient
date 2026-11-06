#version 330 compatibility
uniform isampler2D uSeeds;
uniform int uStep;
uniform ivec2 uSeedSize;
uniform ivec2 uSeedOrigin;
layout(location = 0) out ivec2 outSeed;
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    ivec2 size = uSeedSize;
    float best = 1e30;
    outSeed = ivec2(-1);
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            ivec2 q = p + ivec2(x, y) * uStep;
            if (any(lessThan(q, ivec2(0))) || any(greaterThanEqual(q, size)))
            {
                continue;
            }
            ivec2 seed = texelFetch(uSeeds, q, 0).rg;
            float d = dot(vec2(seed - (p + uSeedOrigin)), vec2(seed - (p + uSeedOrigin)));
            if (seed.x >= 0 && d < best)
            {
                best = d;
                outSeed = seed;
            }
        }
    }
}
