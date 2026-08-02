#version 330 compatibility

in vec2 vUv;
in vec3 vColor;

uniform sampler2D uTexture;

// Multiplies the texture's alpha exactly as the engine's per vertex alpha does, which
// is also what the masked alpha test then compares against.
uniform float uBlend;

out vec4 oColor;

void main()
{
    // Matches the fixed function path: per vertex studio lighting modulating the skin,
    // with the texture alpha left alone for the alpha test on masked textures.
    vec4 texel = texture(uTexture, vUv);

    oColor = vec4(texel.rgb * vColor, texel.a * uBlend);
}
