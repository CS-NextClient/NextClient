#version 330 compatibility

// Same vertex stage as the built-in studio program: a material program adds to the
// pipeline rather than reimplementing skinning and lighting, so it receives every
// built-in uniform and only has to declare what it uses.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexel;
layout(location = 3) in uvec2 aBones;

layout(std140) uniform Bones
{
    vec4 uBones[384];
};

uniform vec2 uTexelScale;

uniform float uAmbient;
uniform float uShade;
uniform vec3 uColorMix;
uniform vec3 uLightVec;
// The kShade* values are injected as defines by the program build, so this file and the
// C++ side cannot drift apart.
uniform int uShadeMode;
uniform float uLightGamma[256];

// Chrome replaces the texture coordinates with a sphere map built around the view: the
// engine derives it per bone from the vector between the chrome origin and the bone, so
// the reflection slides as the model turns rather than as the camera orbits.
// r_blend: the engine folds it into every vertex colour, and for an additive entity it
// replaces the lighting outright rather than modulating it.
uniform float uBlend;

// Glow shell pushes each vertex out along its own normal before the bone transform.
uniform float uGlowScale;

uniform int uChrome;
uniform vec3 uChromeOrigin;
uniform vec3 uViewRight;

// The lights the entity carries. Declared here as well as in the built-in program,
// because a material shades the same vertices and the engine adds these to every mesh of
// the model: leaving them out lights half a model and not the other half.
uniform int uEntityLightCount;
uniform vec4 uEntityLight[3];
uniform vec3 uEntityLightColor[3];
uniform vec2 uGamma;
uniform int uFlipped;


out vec2 vUv;
out vec3 vColor;

// Always computed, not only for chrome meshes: the finish uses the same sphere map for
// its metallic response, and whether the model's own texture asked for chrome is a
// separate question from whether the material wants a reflection.
out vec2 vSphereUv;

// What a highlight needs and the sphere map cannot give: where the surface faces and where
// the viewer is. The chrome origin is the view origin, which is what the engine hands over
// for the sphere map in the first place.
out vec3 vNormal;
out vec3 vToView;

// How much light actually reaches this entity, which is what tells a weapon in the sun from
// the same weapon in a doorway. x is the direct part, which is what a glint answers to;
// y is all of it, which is what a reflection of the surroundings answers to. Both taken
// through the same gamma table the diffuse shading uses, so they are on its scale.
out vec2 vLightLevel;

vec3 Transform(vec3 v, uint bone, float translate)
{
    vec4 r0 = uBones[bone * 3u + 0u];
    vec4 r1 = uBones[bone * 3u + 1u];
    vec4 r2 = uBones[bone * 3u + 2u];

    return vec3(
        dot(r0.xyz, v) + r0.w * translate,
        dot(r1.xyz, v) + r1.w * translate,
        dot(r2.xyz, v) + r2.w * translate
    );
}

float StudioIllum(vec3 normal)
{
    float illum = uAmbient;

    if (uShadeMode == kShadeFlat)
    {
        return min(illum + uShade * 0.8, 255.0);
    }

    const float lambert = 1.4953241;

    float lightcos = min(dot(normal, uLightVec), 1.0);

    illum += uShade;
    lightcos = (lightcos + (lambert - 1.0)) / lambert;

    if (lightcos > 0.0)
    {
        illum -= uShade * lightcos;
    }

    return min(max(illum, 0.0), 255.0);
}

float ScreenGamma(float linear_value)
{
    return linear_value > 1023.0 ? 1.0 : pow(linear_value / 1023.0, uGamma.y);
}

// R_LightStrength and R_LightLambert, as in the built-in program: the light is rotated
// into the bone's space while the vertex and the normal are the ones stored in the file.
vec3 EntityLight(vec3 base)
{
    uint bone = aBones.x * 3u;
    vec4 r0 = uBones[bone];
    vec4 r1 = uBones[bone + 1u];
    vec4 r2 = uBones[bone + 2u];

    vec3 adjust = vec3(0.0);
    vec3 normal = uFlipped != 0 && uChrome == 0 ? vec3(aNormal.x, -aNormal.y, aNormal.z) : aNormal;

    for (int i = 0; i < uEntityLightCount; i++)
    {
        vec3 to_light = uEntityLight[i].xyz - vec3(r0.w, r1.w, r2.w);
        vec3 local = vec3(
            dot(to_light, vec3(r0.x, r1.x, r2.x)),
            dot(to_light, vec3(r0.y, r1.y, r2.y)),
            dot(to_light, vec3(r0.z, r1.z, r2.z))
        );

        vec3 offset = aPosition - local;
        float lambert = -dot(offset, normal);

        if (lambert <= 0.0)
        {
            continue;
        }

        float square = dot(offset, offset);
        float falloff = square <= 0.0 ? 1.0 : uEntityLight[i].w / (sqrt(square) * square);

        adjust += lambert * falloff * uEntityLightColor[i];
    }

    if (adjust == vec3(0.0))
    {
        return base;
    }

    vec3 sum = adjust + pow(base, vec3(uGamma.x)) * 1023.0;

    return vec3(ScreenGamma(sum.r), ScreenGamma(sum.g), ScreenGamma(sum.b));
}

vec2 ChromeTexel(vec3 bone_translation, vec3 world_normal)
{
    vec3 to_bone = normalize(bone_translation - uChromeOrigin);
    vec3 up = normalize(cross(to_bone, uViewRight));
    vec3 right = normalize(cross(up, to_bone));

    // The engine rotates these into the bone's space and dots them with the model space
    // normal; dotting the world normal with them directly is the same number and the
    // world normal is already in hand.
    return vec2((dot(world_normal, right) + 1.0) * 32.0, (dot(world_normal, up) + 1.0) * 32.0);
}

void main()
{
    vec3 world_position = Transform(aPosition + aNormal * uGlowScale, aBones.x, 1.0);
    vec3 world_normal = normalize(Transform(aNormal, aBones.y, 0.0));

    if (uShadeMode == kShadeAdditive)
    {
        // Additive entity: no lighting and no colour mix, the engine writes r_blend into
        // all three channels and lets the texture carry the rest.
        vColor = vec3(uBlend);
    }
    else if (uShadeMode == kShadeFullbright)
    {
        vColor = vec3(uLightGamma[255]);
    }
    else
    {
        vColor = uColorMix * uLightGamma[int(StudioIllum(world_normal))];
    }

    if (uEntityLightCount > 0)
    {
        vColor = EntityLight(vColor);
    }

    // The normal's bone, matching the engine: chrome is computed per normal.
    uint bone = aBones.y * 3u;
    vec3 bone_translation = vec3(uBones[bone].w, uBones[bone + 1u].w, uBones[bone + 2u].w);

    vec2 sphere = ChromeTexel(bone_translation, world_normal);

    // Normalised by the chrome convention's own range, not by uTexelScale: those texel
    // coordinates span 0..64 by construction, so dividing by the model skin's size
    // sampled a corner of the sphere map and the reflection came out almost flat. The
    // engine divides by the texture size only because there the chrome texture is the
    // model's own; a material's sphere map is its own image at its own resolution.
    vSphereUv = sphere * (1.0 / 64.0);

    vNormal = world_normal;
    vToView = uChromeOrigin - world_position;

    vLightLevel = vec2(
        uLightGamma[int(clamp(uShade, 0.0, 255.0))],
        uLightGamma[int(clamp(uAmbient + uShade, 0.0, 255.0))]
    );

    vUv = (uChrome != 0 ? sphere : aTexel) * uTexelScale;

    gl_Position = gl_ModelViewProjectionMatrix * vec4(world_position, 1.0);
}
