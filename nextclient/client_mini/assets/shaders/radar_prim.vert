#version 120
// Prim batch vertex shader: every primitive is a quad. Analytic shapes carry the
// same per-primitive parameters on all four vertices, so the fragment shader sees
// them as constants across the quad; solid quads also vary colour per vertex.
attribute vec2 aPos;
attribute vec4 aShape;  // shape, feather, radius, len
attribute vec4 aCenter; // center.xy, dir.xy
attribute vec4 aColor;
attribute vec4 aTri;    // t0.xy, t1.xy
attribute vec4 aTri2;   // t2.xy, spread, pad

varying vec2 vScreen;
varying vec4 vShape;
varying vec4 vCenter;
varying vec4 vColor;
varying vec4 vTri;
varying vec4 vTri2;

void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * vec4(aPos, 0.0, 1.0);
    vScreen = aPos;
    vShape = aShape;
    vCenter = aCenter;
    vColor = aColor;
    vTri = aTri;
    vTri2 = aTri2;
}
