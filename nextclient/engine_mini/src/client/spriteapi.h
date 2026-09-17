#pragma once

#include "../hlsdk.h"
#include <goldsrc/client_api.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/types.h>

#define MAX_SPRITES 256

struct SPRITELIST
{
    goldsrc::model_t* pSprite;
    char* pName;
    int frameCount;
};

void SPR_Init();
void SPR_Shutdown();
void SPR_Shutdown_NoModelFree();
void SPR_ResetState();

goldsrc::HSPRITE_t SPR_Load(const char* pTextureName);
bool SPR_IsLoaded(const char* sprite_name);
void SPR_Set(goldsrc::HSPRITE_t hsprite, int r, int g, int b);
void SPR_Draw(int frame, int x, int y, const goldsrc::wrect_t* prc);
void SPR_DrawAdditive(int frame, int x, int y, const goldsrc::wrect_t* prc);
void SPR_DrawHoles(int frame, int x, int y, const goldsrc::wrect_t* prc);
void SPR_DrawGeneric(int frame, int x, int y, const goldsrc::wrect_t* prc, int src, int dest, int width, int height);
void SPR_EnableScissor(int x, int y, int width, int height);
void SPR_DisableScissor();

int SPR_Width(goldsrc::HSPRITE_t hsprite, int frame);
int SPR_Height(goldsrc::HSPRITE_t hsprite, int frame);
int SPR_Frames(goldsrc::HSPRITE_t hsprite);
const goldsrc::model_t* SPR_GetModelPointer(goldsrc::HSPRITE_t hsprite);
