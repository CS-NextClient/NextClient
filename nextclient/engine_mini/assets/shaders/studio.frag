#version 330 compatibility

#include "material_coverage.glsl"
#include "material_glitch_surface.glsl"
#include "studio_fog.glsl"

in vec2 vUv;
in vec3 vColor;

uniform sampler2D uTexture;
uniform vec4 uBaseTint;

// Multiplies the texture's alpha exactly as the engine's per vertex alpha does, which
// is also what the masked alpha test then compares against.
uniform float uBlend;

out vec4 oColor;

void main()
{
    bool visible = MaterialCoverage_Evaluate(vUv);

    // Matches the fixed function path: per vertex studio lighting modulating the skin,
    // with the texture alpha left alone for the alpha test on masked textures.
    vec4 texel = MaterialGlitch_Sample(uTexture, vUv) * uBaseTint;

    if (!visible)
    {
        discard;
    }

    oColor = Studio_ApplyFog(vec4(texel.rgb * vColor, texel.a * uBlend));
}
