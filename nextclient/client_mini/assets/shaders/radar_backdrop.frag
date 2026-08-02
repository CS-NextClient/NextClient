#version 120
uniform sampler2D uTex;
uniform vec2 uCenter;
uniform float uRadius;
uniform float uFeather;
uniform float uSquare;
uniform float uLod;
uniform float uStep;
uniform float uDarken;
uniform float uAlpha;
uniform vec3 uFadeColor;
uniform float uFadeAmt;
varying vec2 vScreen;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec3 rgb = vec3(0.0);

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            rgb += texture2D(uTex, uv + vec2(float(x), float(y)) * uStep, uLod).rgb;
        }
    }
    rgb = rgb / 9.0 * uDarken;

    vec2 dd = abs(vScreen - uCenter);
    float d = (uSquare > 0.5) ? max(dd.x, dd.y) : length(dd);
    float mask = 1.0 - smoothstep(uRadius - uFeather, uRadius, d);

    gl_FragColor = vec4(mix(rgb, uFadeColor, uFadeAmt), mask * uAlpha);
}
