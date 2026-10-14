#pragma once
#include <goldsrc/cvar.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/entity.h>
#include <goldsrc/format/wad.h>
#include <goldsrc/types.h>
#include "qgl.hpp"

enum class TextureFormat
{
    Invalid = -1,
    Opaque = 0, // indexed
    Alpha = 1, // indexed
    Grayscale = 2,
    AlphaGradient = 3, // indexed
    RGBA = 4
};

inline int GetBytesPerElement(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::Opaque:
    case TextureFormat::Alpha:
    case TextureFormat::Grayscale:
    case TextureFormat::AlphaGradient:
        return 1;

    case TextureFormat::RGBA:
        return 4;

    default:
        return 0;
    }
}

inline bool IsIndexedTexture(TextureFormat format)
{
    return format == TextureFormat::Opaque || format == TextureFormat::Alpha || format == TextureFormat::AlphaGradient;
}

inline bool IsTextureAlpha(TextureFormat type)
{
    return type == TextureFormat::Alpha || type == TextureFormat::AlphaGradient || type == TextureFormat::RGBA;
}

struct glpic_t
{
    goldsrc::qpic_t pic;
    int texnum;
    float sl, tl, sh, th;
};


extern uint8_t texgammatable[256];
extern int32_t lineargammatable[1024];
extern int32_t lightgammatable[1024];
extern int32_t screengammatable[1024];

extern GLenum oldtarget;

//
// gl_vidnt.cpp
//

extern goldsrc::cvar_t* r_lightmap;
extern goldsrc::cvar_t* gl_clear;
extern goldsrc::cvar_t* r_novis;
extern goldsrc::cvar_t* r_fullbright;
extern goldsrc::cvar_t* gl_monolights;
extern goldsrc::cvar_t* gl_wireframe;
extern goldsrc::cvar_t* r_dynamic;
extern goldsrc::cvar_t* gl_alphamin;
extern goldsrc::cvar_t* gl_polyoffset;

extern int TEXTURE0_SGIS;
extern int TEXTURE1_SGIS;
extern int TEXTURE2_SGIS;
extern goldsrc::qboolean bSupportsNPOTTextures;
extern int gl_mtexable;

void GL_SetMode_Subscriber(void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine, bool result);
void GL_Init();
void GL_Config();

//
// gl_draw.cpp
//

enum GL_TEXTURETYPE : int
{
    GLT_SYSTEM = 0,
    GLT_DECAL = 1,
    GLT_HUDSPRITE = 2,
    GLT_STUDIO = 3,
    GLT_WORLD = 4,
    GLT_SPRITE = 5
};

GLuint GL_GenTexture();
void GL_Bind(int texnum);
void GL_SelectTexture(GLenum target);
void GL_UnloadTextures();
void GL_UnloadTexture(const char* identifier);

int GL_LoadTexture(
    const char* identifier,
    GL_TEXTURETYPE textureType,
    int width,
    int height,
    uint8_t* data,
    int mipmap,
    int iType,
    uint8_t* pPal
);

int GL_LoadTexture2(
    const char* identifier,
    GL_TEXTURETYPE textureType,
    int width,
    int height,
    uint8_t* data,
    goldsrc::qboolean mipmap,
    int iType,
    uint8_t* pPal,
    int filter
);

void GL_DisableMultitexture();
void Draw_DecalSetName(int decal, const char* name);
void Draw_SpriteFrame(goldsrc::mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const goldsrc::wrect_t* prcSubRect);
void Draw_SpriteFrameAdditive(goldsrc::mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const goldsrc::wrect_t* prcSubRect);
void Draw_SpriteFrameHoles(goldsrc::mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const goldsrc::wrect_t* prcSubRect);
void Draw_SpriteFrameGeneric(
    goldsrc::mspriteframe_t* pFrame,
    unsigned short* pPalette,
    int x,
    int y,
    const goldsrc::wrect_t* prcSubRect,
    int src,
    int dest,
    int width,
    int height
);

void Draw_Frame(goldsrc::mspriteframe_t* pFrame, int ix, int iy, const goldsrc::wrect_t* prcSubRect);
void Draw_Shutdown();
void Draw_Init();
goldsrc::qpic_t* Draw_PicFromWad(const char* name);

void V_UpdatePalette();
bool V_CheckGamma();
void BuildGammaTable(float g);

void EnableScissorTest(int x, int y, int width, int height);
void DisableScissorTest();

//
// gl_rmain.cpp
//
void R_NewMap();
void R_RenderView();
void R_ForceCVars(goldsrc::qboolean mp);
void R_DestroyObjects();
void AppendTEntity_Subscriber(goldsrc::cl_entity_t* ent);

//
// gl_sprite.cpp
//
goldsrc::mspriteframe_t* R_GetSpriteFrame(goldsrc::msprite_t* pSprite, int frame);

//
// gl_studio.cpp
//
int* R_StudioReloadSkin(goldsrc::model_t* pModel, int index, goldsrc::skin_t* pskin);
