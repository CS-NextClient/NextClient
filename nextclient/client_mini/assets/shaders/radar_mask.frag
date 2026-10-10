#version 120
uniform sampler2D uTex;
uniform vec2 uCenter;
uniform float uRadius;
uniform float uFeather;
uniform float uGamma;
uniform float uSquare;
uniform float uPremultiplied;
uniform float uAlphaAa;
uniform float uMode; // 0 = map mask, 1 = ring
uniform float uRingInner;
uniform vec4 uRingColor;
uniform vec3 uFadeColor; // flashbang wash applied to every radar pixel
uniform float uFadeAmt;

varying vec2 vScreen;

vec4 SampleMap(vec2 uv)
{
    vec4 center = texture2D(uTex, uv);
    if (uAlphaAa < 0.5)
    {
        return center;
    }

    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    vec4 s0 = texture2D(uTex, uv - 0.125 * dx - 0.375 * dy);
    vec4 s1 = texture2D(uTex, uv + 0.375 * dx - 0.125 * dy);
    vec4 s2 = texture2D(uTex, uv - 0.375 * dx + 0.125 * dy);
    vec4 s3 = texture2D(uTex, uv + 0.125 * dx + 0.375 * dy);
    float alpha = 0.25 * (s0.a + s1.a + s2.a + s3.a);

    // Newly covered pixels need a color from the opaque side of the contour.
    vec4 source = center;
    if (source.a == 0.0)
    {
        source = s0;
        if (s1.a > source.a) { source = s1; }
        if (s2.a > source.a) { source = s2; }
        if (s3.a > source.a) { source = s3; }
    }

    return source.a > 0.0 ? vec4(source.rgb * (alpha / source.a), alpha) : vec4(0.0);
}

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

    vec4 c = SampleMap(gl_TexCoord[0].xy) * gl_Color;
    c.rgb *= mix(1.15, 0.575, clamp(d / uRadius, 0.0, 1.0)); // radial: bright centre, dark edges (+15% brightness)
    float coverage = 1.0 - smoothstep(uRadius - uFeather, uRadius, d);
    c.a *= coverage;

    if (uPremultiplied > 0.5)
    {
        c.rgb *= coverage;
    }

    if (uGamma > 0.0)
    {
        c.rgb = pow(c.rgb, vec3(uGamma));
    }

    c.rgb = mix(c.rgb, uFadeColor * (uPremultiplied > 0.5 ? c.a : 1.0), uFadeAmt);

    gl_FragColor = c;
}
