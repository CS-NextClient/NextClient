#pragma once

#include <goldsrc/fwd.h>

namespace goldsrc
{
    inline constexpr int TRI_TRIANGLES = 0;
    inline constexpr int TRI_API_VERSION = 1;
    inline constexpr int TRI_TRIANGLE_FAN = 1;
    inline constexpr int TRI_QUADS = 2;
    inline constexpr int TRI_POLYGON = 3;
    inline constexpr int TRI_LINES = 4;
    inline constexpr int TRI_TRIANGLE_STRIP = 5;
    inline constexpr int TRI_QUAD_STRIP = 6;

    enum TRICULLSTYLE
    {
        TRI_FRONT = 0,
        TRI_NONE = 1
    };

    struct triangleapi_t
    {
        int version;
        void (*RenderMode)(int);
        void (*Begin)(int);
        void (*End)();
        void (*Color4f)(float, float, float, float);
        void (*Color4ub)(unsigned char, unsigned char, unsigned char, unsigned char);
        void (*TexCoord2f)(float, float);
        void (*Vertex3fv)(float*);
        void (*Vertex3f)(float, float, float);
        void (*Brightness)(float);
        void (*CullFace)(TRICULLSTYLE);
        int (*SpriteTexture)(model_t*, int);
        int (*WorldToScreen)(float*, float*);
        void (*Fog)(float*, float, float, int);
        void (*ScreenToWorld)(float*, float*);
        void (*GetMatrix)(const int, float*);
        int (*BoxInPVS)(float*, float*);
        void (*LightAtPoint)(float*, float*);
        void (*Color4fRendermode)(float, float, float, float, int);
        void (*FogParams)(float, int);
    };
} // namespace goldsrc
