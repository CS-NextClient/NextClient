#version 120
uniform sampler2D uTex;
uniform vec2 uCenter;
uniform float uRadius;
uniform float uFeather;
uniform float uGamma;
uniform float uSquare;
uniform float uMode; // 0 = map mask, 1 = ring
uniform float uRingInner;
uniform vec4 uRingColor;
uniform vec3 uFadeColor; // flashbang wash applied to every radar pixel
uniform float uFadeAmt;

varying vec2 vScreen;

void main()
{
    vec2 dd = abs(vScreen - uCenter);
    float d = (uSquare > 0.5) ? max(dd.x, dd.y) : length(dd);
    if (uMode > 0.5)
    {
        float a = smoothstep(uRingInner - uFeather, uRingInner + uFeather, d)
                * (1.0 - smoothstep(uRadius - uFeather, uRadius + uFeather, d));

        gl_FragColor = vec4(mix(uRingColor.rgb, uFadeColor, uFadeAmt), uRingColor.a * a);
        return;
    }

    vec4 c = texture2D(uTex, gl_TexCoord[0].xy) * gl_Color;
    c.rgb *= mix(1.15, 0.575, clamp(d / uRadius, 0.0, 1.0)); // radial: bright centre, dark edges (+15% brightness)
    c.a *= 1.0 - smoothstep(uRadius - uFeather, uRadius, d);

    if (uGamma > 0.0)
    {
        c.rgb = pow(c.rgb, vec3(uGamma));
    }

    c.rgb = mix(c.rgb, uFadeColor, uFadeAmt);

    gl_FragColor = c;
}
