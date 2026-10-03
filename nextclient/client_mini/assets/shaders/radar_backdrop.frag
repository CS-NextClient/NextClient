#version 120
uniform sampler2D uTex;
uniform vec2 uCenter;
uniform float uRadius;
uniform float uFeather;
uniform float uSquare;
uniform vec2 uDirection;
uniform float uFilterPass;
uniform float uDarken;
uniform float uAlpha;
uniform vec3 uFadeColor;
uniform float uFadeAmt;
varying vec2 vScreen;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    if (uFilterPass > 0.5)
    {
        vec3 sum = vec3(0.0);
        float total = 0.0;

        for (int i = -16; i <= 16; i++)
        {
            float offset = float(i);
            float weight = exp(-offset * offset / 72.0);
            sum += texture2D(uTex, uv + uDirection * offset).rgb * weight;
            total += weight;
        }

        gl_FragColor = vec4(sum / total, 1.0);
        return;
    }

    vec3 rgb = texture2D(uTex, uv).rgb * uDarken;

    vec2 dd = abs(vScreen - uCenter);
    float d = (uSquare > 0.5) ? max(dd.x, dd.y) : length(dd);
    float mask = 1.0 - smoothstep(uRadius - uFeather, uRadius, d);

    gl_FragColor = vec4(mix(rgb, uFadeColor, uFadeAmt), mask * uAlpha);
}
