#version 330 compatibility
#include "material_coverage.glsl"
uniform sampler2D uBase;
uniform int uMasked;
uniform int uBlendBaseAlpha;
uniform float uAlphaScale;
uniform vec4 uFill;
uniform vec4 uOutline;
uniform vec3 uShape;
uniform float uOutlineMode;
in vec2 vUv;
layout(location = 0) out vec4 outFill;
layout(location = 1) out vec4 outOutline;
layout(location = 2) out vec4 outShape;
void main()
{
    bool visible = MaterialCoverage_Evaluate(vUv);
    bool masked = uMasked != 0;
    bool blend_base_alpha = uBlendBaseAlpha != 0;
    float base_alpha = (masked || blend_base_alpha) ? texture(uBase, vUv).a : 1.0;
    float alpha = base_alpha * uAlphaScale;

    if (!visible || (masked && alpha <= 0.5) || (blend_base_alpha && alpha <= 0.0))
    {
        discard;
    }

    float effect_alpha = blend_base_alpha ? base_alpha : 1.0;
    outFill = vec4(uFill.rgb, uFill.a * effect_alpha);
    outOutline = vec4(uOutline.rgb, uOutline.a * effect_alpha);
    outShape = vec4(uShape, 1.0 + uOutlineMode);
}
