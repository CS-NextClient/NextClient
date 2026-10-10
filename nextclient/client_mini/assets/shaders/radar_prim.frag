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

float roundedBoxDist(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + vec2(radius);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float signalArcDist(vec2 p, float radius)
{
    p.x = abs(p.x);
    if (p.y <= -p.x)
    {
        return abs(length(p) - radius);
    }
    return distance(p, vec2(0.70710678, -0.70710678) * radius);
}

float TriangleCoverage(vec2 p, vec2 t0, vec2 t1, vec2 t2, float feather)
{
    float d0 = cross2(t1 - t0, p - t0);
    float d1 = cross2(t2 - t1, p - t1);
    float d2 = cross2(t0 - t2, p - t2);
    bool inside = (d0 >= 0.0 && d1 >= 0.0 && d2 >= 0.0) || (d0 <= 0.0 && d1 <= 0.0 && d2 <= 0.0);
    float dist = min(segDist(p, t0, t1), min(segDist(p, t1, t2), segDist(p, t2, t0)));
    return 1.0 - smoothstep(-feather, feather, inside ? -dist : dist);
}

vec4 PlayerMarker(vec2 p, vec2 center, vec2 direction, bool offscreen, float radius, vec4 color)
{
    bool directed = dot(direction, direction) > 0.5;
    vec2 perpendicular = vec2(-direction.y, direction.x);
    if (offscreen && directed)
    {
        vec2 base = center - direction * (radius * 0.8);
        float coverage = TriangleCoverage(p, center + direction * (radius * 1.7),
            base + perpendicular * (radius * 1.2), base - perpendicular * (radius * 1.2), 1.1);
        float alpha = color.a * coverage;
        return vec4(color.rgb * alpha, alpha);
    }
    float disc = color.a * (1.0 - smoothstep(radius - 1.2, radius, distance(p, center)));
    float heading = 0.0;
    if (directed)
    {
        vec2 base = center + direction * (cos(0.62) * radius);
        vec2 width = perpendicular * (sin(0.62) * radius);
        heading = color.a * TriangleCoverage(p, center + direction * (radius * 2.0), base + width, base - width, 1.1);
    }
    return vec4(color.rgb * disc + vec3(240.0 / 255.0) * heading * (1.0 - disc), disc + heading * (1.0 - disc));
}

void main()
{
    float uShape = vShape.x; // 0 disc, 1 cone, 2 triangle, 3 cross, 4 ring, 5 solid, 6 glyph, 7 question mark, 8 bomb, 9 disconnected signal, 10 player marker fade
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
        a = TriangleCoverage(p, uT0, uT1, uT2, uFeather);
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
    else if (uShape < 6.5)
    {
        // glyph: LUMINANCE_ALPHA atlas cell with alpha = coverage (the fill plus the baked drop
        // shadow) and luminance = the glyph fill. Modulate the caller colour by luminance, so the
        // body takes the colour while the baked drop shadow (luminance 0) stays black, matching the
        // engine's own scheme text.
        vec4 g = texture2D(uAtlas, uT0);
        a = g.a;
        rgb = uColor.rgb * g.r;
    }
    else if (uShape < 7.5)
    {
        // Question mark: a circular hook, a short stem and a dot share the cross's edge profile.
        vec2 q = (p - uCenter) / uRadius;
        vec2 rel = q - vec2(0.0, -0.5);
        vec2 arcEnd = vec2(0.35355339, 0.35355339);
        float hook = abs(length(rel) - 0.5);
        if (rel.y > 0.0 && rel.x < rel.y)
        {
            hook = min(distance(rel, vec2(-0.5, 0.0)), distance(rel, arcEnd));
        }
        float stem = segDist(q, arcEnd + vec2(0.0, -0.5), vec2(0.0, 0.05));
        float dotDist = distance(p, uCenter + vec2(0.0, uRadius)) + uLen * 0.25;
        float dl = min(min(hook, stem) * uRadius, dotDist);
        a = 1.0 - smoothstep(uLen - uFeather, uLen, dl);
    }
    else if (uShape < 8.5)
    {
        vec2 q = (p - uCenter) / uRadius;
        float body = roundedBoxDist(q - vec2(0.0, 0.12), vec2(0.9, 0.62), 0.10);
        float wire = min(segDist(q, vec2(-0.52, -0.42), vec2(-0.52, -0.80)),
                         min(segDist(q, vec2(-0.52, -0.80), vec2(0.12, -0.80)),
                             segDist(q, vec2(0.12, -0.80), vec2(0.12, -0.42)))) - 0.08;
        a = 1.0 - smoothstep(-uFeather, 0.0, min(body, wire) * uRadius);

        float detail = roundedBoxDist(q - vec2(-0.20, -0.20), vec2(0.40, 0.12), 0.02);
        detail = min(detail, roundedBoxDist(q - vec2(0.55, 0.10), vec2(0.13, 0.38), 0.03));
        for (int row = 0; row < 2; row++)
        {
            for (int column = 0; column < 3; column++)
            {
                vec2 key = vec2(-0.48 + float(column) * 0.24, 0.14 + float(row) * 0.24);
                detail = min(detail, roundedBoxDist(q - key, vec2(0.075, 0.055), 0.01));
            }
        }
        float ink = 1.0 - smoothstep(-0.4, 0.4, detail * uRadius);
        rgb = mix(uColor.rgb, uColor.rgb * 0.20, ink);
    }
    else if (uShape < 9.5)
    {
        vec2 q = p - uCenter;
        vec2 rel = q - vec2(0.0, 0.5 * uRadius);
        float arcs = min(signalArcDist(rel, 1.05 * uRadius), signalArcDist(rel, 0.60 * uRadius)) - uLen;
        float dotDist = length(rel) - max(1.65, 0.14 * uRadius);
        float slash = segDist(q, vec2(-0.85, 0.8) * uRadius, vec2(0.85, -0.85) * uRadius) - uLen;
        float signal = max(min(arcs, dotDist), 0.65 - slash);
        a = 1.0 - smoothstep(-uFeather, uFeather, min(signal, slash));
    }
    else
    {
        bool currentOffscreen = (uT2.x > 0.5 && uT2.x < 1.5) || uT2.x > 2.5;
        bool previousOffscreen = uT2.x > 1.5;
        vec4 current = PlayerMarker(p, uCenter, uT0, currentOffscreen, uRadius, uColor) * uSpread;
        vec4 previous = PlayerMarker(p, uDir, uT1, previousOffscreen, uRadius, vec4(uColor.rgb, uLen)) * uT2.y;
        // Blend the two premultiplied marker images once, including their shared pixels.
        vec4 marker = current + previous;
        a = marker.a;
        rgb = a > 0.0 ? marker.rgb / a : uColor.rgb;
        uColor.a = 1.0;
    }

    vec2 cd = abs(vScreen - uClipCenter);
    float cdist = (uClipSquare > 0.5) ? max(cd.x, cd.y) : length(cd);

    a *= 1.0 - smoothstep(uClipRadius - 1.0, uClipRadius, cdist);

    gl_FragColor = vec4(mix(rgb, uFadeColor, uFadeAmt), uColor.a * a);
}
