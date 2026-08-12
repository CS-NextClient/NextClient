#version 330 compatibility

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexel;
// x the position's bone, y the normal's, z the bone of the normal the glow shell uses.
layout(location = 3) in uvec3 aBones;

// One normal per vertex of the file, shared by every copy of it, which is what the glow
// shell is pushed out along and unwrapped from.
layout(location = 4) in vec3 aShellNormal;

// Three rows per bone, matching the engine's float[3][4] bone transforms, so the array
// is uploaded verbatim. std140 gives a vec4 the 16 byte stride the rows already have.
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

// lightgammatable sampled at the multiples of 4 the engine actually indexes, already
// divided by 1023. Exact rather than an approximation of the curve.
uniform float uLightGamma[256];

// Chrome replaces the texture coordinates with a sphere map built around the view: the
// engine derives it per bone from the vector between the chrome origin and the bone, so
// the reflection slides as the model turns rather than as the camera orbits.
// r_blend: the engine folds it into every vertex colour, and for an additive entity it
// replaces the lighting outright rather than modulating it.
uniform float uBlend;

// Glow shell pushes each vertex out along its shared normal before the bone transform.
uniform float uGlowScale;

// The entity's render colour, which is all the colour the shell has.
uniform vec3 uShellColor;

// The lights the entity carries: the engine picks up to three of cl_elights by
// radius² / distance² and adds them on top of the lighting the model already has.
uniform int uEntityLightCount;
uniform vec4 uEntityLight[3];      // xyz world origin, w radius squared
uniform vec3 uEntityLightColor[3]; // already through the linear table, so 0..1023

// The exponent of the engine's linear and screen gamma tables, and its inverse. Both
// tables are that power exactly, and the pair costs two floats where they cost 2048.
uniform vec2 uGamma;

// Set for the mirrored view model, which the engine compensates for in the lambert.
uniform int uFlipped;

// What the bone rows were grown by, 1 for everything the game itself draws. Only the
// entity light term needs it: that is the one place the rows are read as a rotation to be
// inverted rather than as a transform to be applied.
uniform float uBoneScale;

uniform int uChrome;
uniform vec3 uChromeOrigin;
uniform vec3 uViewRight;


out vec2 vUv;
out vec3 vColor;

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

// R_StudioLighting. The engine rotates the light into each bone's space and dots it with
// the model space normal; dotting the world space normal with the world light vector is
// the same value for orthonormal bone transforms, and the normal is already in hand.
float StudioIllum(vec3 normal)
{
    float illum = uAmbient;

    if (uShadeMode == kShadeFlat)
    {
        return min(illum + uShade * 0.8, 255.0);
    }

    // v_lambert1, a constant in the engine. Its branch for values below 1.0 is therefore
    // unreachable and is not reproduced here.
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
    // The engine's own ceiling: a channel that overflows the table is full brightness.
    return linear_value > 1023.0 ? 1.0 : pow(linear_value / 1023.0, uGamma.y);
}

// R_LightStrength and R_LightLambert. The engine does this in the bone's own space: the
// light is rotated into it, while the vertex and the normal are the ones stored in the
// file, untransformed. The falloff is radius² / distance³, and the distance the lambert
// term carries cancels one of those powers.
vec3 EntityLight(vec3 base)
{
    uint bone = aBones.x * 3u;
    vec4 r0 = uBones[bone];
    vec4 r1 = uBones[bone + 1u];
    vec4 r2 = uBones[bone + 2u];

    vec3 adjust = vec3(0.0);

    // A mirrored view model carries the mirror in its bone transforms, and the engine
    // answers that by negating the normal's Y for this term alone. It does so only on the
    // branch that emits ordinary texture coordinates, not on the chrome one.
    vec3 normal = uFlipped != 0 && uChrome == 0 ? vec3(aNormal.x, -aNormal.y, aNormal.z) : aNormal;

    // A scaled entity carries the factor in its bone rows, and the three dots below are the
    // transpose of a rotation, which is its inverse only while the rows are unit length.
    // Divided out here, and the result divided by the square of it: the whole term is a
    // length over a length cubed, and every one of those lengths grew by the factor.
    float inverse_scale = 1.0 / uBoneScale;

    for (int i = 0; i < uEntityLightCount; i++)
    {
        vec3 to_light = uEntityLight[i].xyz - vec3(r0.w, r1.w, r2.w);
        vec3 local = vec3(
            dot(to_light, vec3(r0.x, r1.x, r2.x)),
            dot(to_light, vec3(r0.y, r1.y, r2.y)),
            dot(to_light, vec3(r0.z, r1.z, r2.z))
        ) * inverse_scale * inverse_scale;

        vec3 offset = aPosition - local;
        float lambert = -dot(offset, normal);

        if (lambert <= 0.0)
        {
            continue;
        }

        float square = dot(offset, offset);
        float falloff = square <= 0.0 ? 1.0 : uEntityLight[i].w / (sqrt(square) * square);

        adjust += lambert * falloff * uEntityLightColor[i] * inverse_scale * inverse_scale;
    }

    if (adjust == vec3(0.0))
    {
        return base;
    }

    // Added in the linear domain, exactly where the engine adds it, and brought back
    // through the screen table afterwards.
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
    // Bone transforms take the model into world space; the engine's modelview holds the
    // camera, exactly as the fixed function path expects after its CPU skinning.
    vec3 world_position = Transform(aPosition + aShellNormal * uGlowScale, aBones.x, 1.0);
    vec3 world_normal = normalize(Transform(aNormal, aBones.y, 0.0));

    if (uShadeMode == kShadeGlowShell)
    {
        // Glow shell: the engine's forced chrome branch emits no vertex colour, so the
        // whole shell keeps the flat colour the pass opened with and the sprite bound over
        // the model carries everything else.
        vColor = uShellColor;
    }
    else if (uShadeMode == kShadeAdditive)
    {
        // Additive entity: no lighting and no colour mix, the engine writes r_blend into
        // all three channels and lets the texture carry the rest.
        vColor = vec3(uBlend);
    }
    else if (uShadeMode == kShadeFullbright)
    {
        // The stock engine never reads STUDIO_NF_FULLBRIGHT, so honouring it is a
        // deliberate addition rather than a reproduction: the texture is shown at the
        // brightness ceiling of the same gamma pipeline, and the scene light colour is
        // not applied, since a self lit surface is not tinted by what lights the room.
        vColor = vec3(uLightGamma[255]);
    }
    else
    {
        vColor = uColorMix * uLightGamma[int(StudioIllum(world_normal))];
    }

    // Every mode but the shell: the engine runs the entity's lights over whatever colour
    // the vertex ended up with, including the flat one an additive entity gets, and the
    // forced chrome pass is the one place it emits no colour at all.
    if (uShadeMode != kShadeGlowShell && uEntityLightCount > 0)
    {
        vColor = EntityLight(vColor);
    }

    vec2 texel = aTexel;

    if (uChrome != 0)
    {
        // The normal's bone, matching the engine: chrome is computed per normal. The shell
        // is unwrapped from the same normal it was pushed out along, not from the one this
        // copy of the vertex carries for lighting, so that its sphere map is continuous
        // across the hard edges the extrusion is already welded across.
        bool shell = uShadeMode == kShadeGlowShell;
        uint normal_bone = shell ? aBones.z : aBones.y;
        vec3 chrome_normal = shell ? normalize(Transform(aShellNormal, aBones.z, 0.0)) : world_normal;

        uint bone = normal_bone * 3u;
        vec3 bone_translation = vec3(uBones[bone].w, uBones[bone + 1u].w, uBones[bone + 2u].w);

        texel = ChromeTexel(bone_translation, chrome_normal);
    }

    vUv = texel * uTexelScale;

    gl_Position = gl_ModelViewProjectionMatrix * vec4(world_position, 1.0);
}
