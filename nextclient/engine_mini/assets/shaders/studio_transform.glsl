#include "material_glitch.glsl"
invariant gl_Position;

vec3 Transform(vec3 v, uint bone, float translate)
{
#ifdef STUDIO_RIGID_INSTANCING
    vec4 r0 = aInstanceRow0;
    vec4 r1 = aInstanceRow1;
    vec4 r2 = aInstanceRow2;
#else
    vec4 r0 = uBones[bone * 3u + 0u];
    vec4 r1 = uBones[bone * 3u + 1u];
    vec4 r2 = uBones[bone * 3u + 2u];
#endif

    return vec3(
        dot(r0.xyz, v) + r0.w * translate,
        dot(r1.xyz, v) + r1.w * translate,
        dot(r2.xyz, v) + r2.w * translate
    );
}

vec4 Studio_Project(vec3 world_position)
{
    return MaterialGlitch_Position(gl_ModelViewProjectionMatrix * vec4(world_position, 1.0));
}
