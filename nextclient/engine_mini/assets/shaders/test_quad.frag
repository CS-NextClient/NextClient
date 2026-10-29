#version 330 compatibility

in vec2 vUv;

uniform float uTime;

out vec4 oColor;

void main()
{
    vec2 cell = floor(vUv * 8.0);
    float checker = mod(cell.x + cell.y, 2.0);

    vec3 color = mix(vec3(0.05, 0.35, 0.85), vec3(0.95, 0.55, 0.10), checker);
    float alpha = 0.25 + 0.10 * sin(uTime * 2.0);

    oColor = vec4(color, alpha);
}
