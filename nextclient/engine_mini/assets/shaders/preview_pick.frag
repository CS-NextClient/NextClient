#version 330 compatibility

#include "material_coverage.glsl"

uniform sampler2D uBase;
uniform int uMasked;
uniform uint uId;
uniform float uAlphaScale;
in vec2 vUv;
layout(location = 0) out uint outId;

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

    outId = uId;
}
