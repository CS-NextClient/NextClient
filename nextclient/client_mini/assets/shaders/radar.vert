#version 120
varying vec2 vScreen;

void main()
{
    gl_Position = ftransform();
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_FrontColor = gl_Color;
    vScreen = gl_Vertex.xy;
}
