#version 330 compatibility

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexel;
layout(location = 3) in uvec3 aBones;
layout(std140) uniform Bones
{
    vec4 uBones[384];
};
#include "studio_transform.glsl"

uniform vec2 uTexelScale;
uniform int uChrome;
uniform vec3 uChromeOrigin;
uniform vec3 uViewRight;
out vec2 vUv;

void main()
{
    vec3 transformed = Transform(aPosition, aBones.x, 1.0);
    gl_Position = Studio_Project(transformed);
    vUv = aTexel * uTexelScale;

    if (uChrome != 0)
    {
        uint normal_bone = aBones.y * 3u;
        vec3 normal = normalize(vec3(
            dot(uBones[normal_bone].xyz, aNormal),
            dot(uBones[normal_bone + 1u].xyz, aNormal),
            dot(uBones[normal_bone + 2u].xyz, aNormal)
        ));
        vec3 origin = vec3(uBones[normal_bone].w, uBones[normal_bone + 1u].w, uBones[normal_bone + 2u].w);
        vec3 to_bone = normalize(origin - uChromeOrigin);
        vec3 up = normalize(cross(to_bone, uViewRight));
        vec3 right = normalize(cross(up, to_bone));
        vUv = vec2((dot(normal, right) + 1.0) * 32.0, (dot(normal, up) + 1.0) * 32.0) * uTexelScale;
    }
}
