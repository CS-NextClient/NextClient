#pragma once

#include "../hlsdk.h"
#include <APIProxy.h>
#include <model.h>

#define MAX_SPRITES 256

struct SPRITELIST
{
    model_t* pSprite;
    char* pName;
    int frameCount;
};

void SPR_Init();
void SPR_Shutdown();
void SPR_Shutdown_NoModelFree();

HSPRITE_t SPR_Load(const char* pTextureName);
bool SPR_IsLoaded(const char* sprite_name);
void SPR_Set(HSPRITE_t hsprite, int r, int g, int b);
void SPR_Draw(int frame, int x, int y, const wrect_t* prc);
void SPR_DrawAdditive(int frame, int x, int y, const wrect_t* prc);
void SPR_DrawHoles(int frame, int x, int y, const wrect_t* prc);
void SPR_DrawGeneric(int frame, int x, int y, const wrect_t* prc, int src, int dest, int width, int height);
void SPR_EnableScissor(int x, int y, int width, int height);
void SPR_DisableScissor();

int SPR_Width(HSPRITE_t hsprite, int frame);
int SPR_Height(HSPRITE_t hsprite, int frame);
int SPR_Frames(HSPRITE_t hsprite);
const model_t* SPR_GetModelPointer(HSPRITE_t hsprite);
