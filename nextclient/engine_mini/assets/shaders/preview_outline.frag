#version 330 compatibility

uniform usampler2D uIds;
uniform uint uId;
uniform vec2 uOrigin;
out vec4 outColor;

uint IdAt(ivec2 position)
{
    ivec2 size = textureSize(uIds, 0);
    return any(lessThan(position, ivec2(0))) || any(greaterThanEqual(position, size)) ? 0u : texelFetch(uIds, position, 0).r;
}

void main()
{
    ivec2 position = ivec2(gl_FragCoord.xy - uOrigin);
    bool selected = IdAt(position) == uId;
    bool edge = false;

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            edge = edge || ((IdAt(position + ivec2(x, y)) == uId) != selected);
        }
    }

    if (!edge)
    {
        discard;
    }

    outColor = vec4(1.0, 0.78, 0.12, 1.0);
}
