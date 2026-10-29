#version 330 compatibility
#include "material_coverage.glsl"
uniform sampler2D uBase;
uniform int uMasked;
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
    bool masked_out = false;

    if (uMasked != 0)
    {
        masked_out = texture(uBase, vUv).a * uAlphaScale <= 0.5;
    }

    if (!visible || masked_out)
    {
        discard;
    }
    outFill = uFill;
    outOutline = uOutline;
    outShape = vec4(uShape, 1.0 + uOutlineMode);
}
