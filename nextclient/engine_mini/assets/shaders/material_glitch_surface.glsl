#include "material_glitch.glsl"
vec4 MaterialGlitch_Sample(sampler2D source, vec2 uv)
{
    vec4 color = texture(source, uv);
    if (uGlitchAmount > 0.0 && uGlitchRgbSplit > 0.0)
    {
        float shift = MaterialGlitch_Offset((gl_FragCoord.y - uEffectViewport.y) / max(uEffectViewport.w, 1.0));
        vec2 dx = dFdx(uv);
        vec2 dy = dFdy(uv);
        vec2 offset = dx * shift * uGlitchRgbSplit;
        color.r = textureGrad(source, uv + offset, dx, dy).r;
        color.b = textureGrad(source, uv - offset, dx, dy).b;
    }
    return color;
}
