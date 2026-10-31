#version 330 compatibility

#include "studio_fog.glsl"

in vec2 vUv;
in vec3 vColor;

uniform sampler2D uTexture;

// Multiplies the texture's alpha exactly as the engine's per vertex alpha does, which
// is also what the masked alpha test then compares against.
#ifdef STUDIO_RIGID_INSTANCING
flat in float vInstanceBlend;
#else
uniform float uBlend;
#endif

out vec4 oColor;

void main()
{
    // Matches the fixed function path: per vertex studio lighting modulating the skin,
    // with the texture alpha left alone for the alpha test on masked textures.
    vec4 texel = texture(uTexture, vUv);

#ifdef STUDIO_RIGID_INSTANCING
    float blend = vInstanceBlend;
#else
    float blend = uBlend;
#endif

    oColor = Studio_ApplyFog(vec4(texel.rgb * vColor, texel.a * blend));
}
