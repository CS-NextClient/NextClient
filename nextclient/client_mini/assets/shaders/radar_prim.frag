#version 120
// Clip shape and flashbang wash are frame-constant, so they stay uniforms uploaded
// once per batch; the per-primitive parameters arrive as varyings (see radar_prim.vert).
uniform vec2 uClipCenter;
uniform float uClipRadius;
uniform float uClipSquare;
uniform vec3 uFadeColor; // flashbang wash applied to every primitive
uniform float uFadeAmt;
uniform sampler2D uAtlas; // glyph cell: alpha = coverage, luminance = fill (a baked drop shadow reads as black)

varying vec2 vScreen;
varying vec4 vShape;  // shape, feather, radius, len
varying vec4 vCenter; // center.xy, dir.xy
varying vec4 vColor;
varying vec4 vTri;    // t0.xy, t1.xy
varying vec4 vTri2;   // t2.xy, spread, pad

float cross2(vec2 u, vec2 v)
{
    return u.x * v.y - u.y * v.x;
}

float segDist(vec2 p, vec2 a, vec2 b)
{
    vec2 e = b - a; vec2 w = p - a;
    float t = clamp(dot(w, e) / max(dot(e, e), 1e-5), 0.0, 1.0);
    return length(w - e * t);
}

void main()
{
    float uShape = vShape.x; // 0 disc, 1 cone, 2 triangle, 3 cross, 4 ring, 5 solid, 6 glyph
    float uFeather = vShape.y;
    float uRadius = vShape.z;
    float uLen = vShape.w;
    vec2 uCenter = vCenter.xy;
    vec2 uDir = vCenter.zw;
    vec4 uColor = vColor;
    vec2 uT0 = vTri.xy;
    vec2 uT1 = vTri.zw;
    vec2 uT2 = vTri2.xy;
    float uSpread = vTri2.z;

    vec2 p = vScreen;
    float a;
    vec3 rgb = uColor.rgb;
    if (uShape < 0.5)
    {
        a = 1.0 - smoothstep(uRadius - uFeather, uRadius, distance(p, uCenter));
    }
    else if (uShape < 1.5)
    {
        vec2 rel = p - uCenter;
        vec2 perp = vec2(-uDir.y, uDir.x);
        float t = dot(rel, uDir);

        if (t <= 0.0)
        {
            discard;
        }

        float ang = atan(abs(dot(rel, perp)) / t);
        float edgeAng = 1.0 - smoothstep(uSpread - 0.10, uSpread, ang);
        float edgeLen = 1.0 - smoothstep(uLen - uFeather, uLen, t);

        a = exp(-3.5 * (t / uLen)) * edgeAng * edgeLen;
    }
    else if (uShape < 2.5)
    {
        float d0 = cross2(uT1 - uT0, p - uT0);
        float d1 = cross2(uT2 - uT1, p - uT1);
        float d2 = cross2(uT0 - uT2, p - uT2);
        bool inside = (d0 >= 0.0 && d1 >= 0.0 && d2 >= 0.0) || (d0 <= 0.0 && d1 <= 0.0 && d2 <= 0.0);
        float dist = min(segDist(p, uT0, uT1), min(segDist(p, uT1, uT2), segDist(p, uT2, uT0)));
        float sd = inside ? -dist : dist;

        a = 1.0 - smoothstep(-uFeather, uFeather, sd);
    }
    else if (uShape < 3.5)
    {
        vec2 c = uCenter; // cross: two diagonals as one shape (uRadius extent, uLen half-width)
        float dl = min(segDist(p, c + vec2(-uRadius, -uRadius), c + vec2(uRadius, uRadius)),
                       segDist(p, c + vec2(-uRadius,  uRadius), c + vec2(uRadius, -uRadius)));

        a = 1.0 - smoothstep(uLen - uFeather, uLen, dl);
    }
    else if (uShape < 4.5)
    {
        float dd = distance(p, uCenter); // ring: uRadius outer, uLen inner

        a = (1.0 - smoothstep(uRadius - uFeather, uRadius, dd)) * smoothstep(uLen - uFeather, uLen, dd);
    }
    else if (uShape < 5.5)
    {
        a = 1.0; // solid: the quad geometry itself is the shape (polyline segments)
    }
    else
    {
        // glyph: LUMINANCE_ALPHA atlas cell with alpha = coverage (the fill plus the baked drop
        // shadow) and luminance = the glyph fill. Modulate the caller colour by luminance, so the
        // body takes the colour while the baked drop shadow (luminance 0) stays black, matching the
        // engine's own scheme text.
        vec4 g = texture2D(uAtlas, uT0);
        a = g.a;
        rgb = uColor.rgb * g.r;
    }

    vec2 cd = abs(vScreen - uClipCenter);
    float cdist = (uClipSquare > 0.5) ? max(cd.x, cd.y) : length(cd);

    a *= 1.0 - smoothstep(uClipRadius - 1.0, uClipRadius, cdist);

    gl_FragColor = vec4(mix(rgb, uFadeColor, uFadeAmt), uColor.a * a);
}
