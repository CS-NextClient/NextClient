#pragma once

#include "../hlsdk.h"
#include <model.h>
#include "qgl.hpp"

//
// gl_vidnt.cpp
//

extern int TEXTURE0_SGIS;
extern int TEXTURE1_SGIS;
extern int TEXTURE2_SGIS;

void GL_SetMode_Subscriber(void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine, bool result);
void GL_Init_Pre();
void GL_Init_Post();

//
// gl_draw.cpp
//
extern GLenum oldtarget;

enum GL_TEXTURETYPE : int
{
    GLT_SYSTEM_0     = 0,
    GLT_DECAL_0      = 1,
    GLT_HUDSPRITE_0  = 2,
    GLT_STUDIO_0     = 3,
    GLT_WORLD_0      = 4,
    GLT_SPRITE_0     = 5
};

void GL_Bind(int texnum);
void GL_SelectTexture(GLenum target);
int GL_LoadTexture(const char* identifier, GL_TEXTURETYPE textureType, int width, int height, uint8_t* data, int mipmap, int iType, uint8_t* pPal);
void GL_DisableMultitexture();

void Draw_DecalSetName(int decal, const char* name);

void Draw_SpriteFrame(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect);
void Draw_SpriteFrameAdditive(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect);
void Draw_SpriteFrameHoles(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect);
void Draw_SpriteFrameGeneric(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect, int src, int dest, int width, int height);
void Draw_Frame(mspriteframe_t* pFrame, int ix, int iy, const wrect_t* prcSubRect);

void EnableScissorTest(int x, int y, int width, int height);
void DisableScissorTest();

//
// gl_rmain.cpp
//
void R_NewMap();
void R_RenderView();
void AppendTEntity_Subscriber(cl_entity_t* ent);

//
// gl_sprite.cpp
//
mspriteframe_t* R_GetSpriteFrame(msprite_t* pSprite, int frame);

//
// gl_studio.cpp
//
int* R_StudioReloadSkin(model_t* pModel, int index, skin_t* pskin);
