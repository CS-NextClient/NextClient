#include "spriteapi.h"
#include "../engine.h"
#include <unordered_set>
#include <string>
#include <optick.h>
#include "../console/console.h"
#include "../common/zone.h"
#include "../common/sys_dll.h"
#include "../common/model.h"
#include "../common/r_studio.h"
#include "../graphics/gl_local.h"

extern HSPRITE_t ghCrosshair;

unsigned short gSpritePalette[256];
msprite_t* gpSprite;
SPRITELIST* gSpriteList;
std::unordered_set<std::string> gLoadedSpriteNames;
int gSpriteCount;

static SPRITELIST* SPR_Get(HSPRITE_t hsprite)
{
    OPTICK_EVENT();

    hsprite--;

    if (hsprite >= 0 && hsprite < gSpriteCount)
        return &gSpriteList[hsprite];

    return nullptr;
}

void SPR_Init()
{
    OPTICK_EVENT();

    if (!gSpriteList)
    {
        ghCrosshair = 0;
        gSpriteCount = MAX_SPRITES;
        gSpriteList = (SPRITELIST*)Mem_ZeroMalloc(sizeof(SPRITELIST) * MAX_SPRITES);
        gLoadedSpriteNames.clear();
        gpSprite = nullptr;
    }
}

void SPR_Shutdown()
{
    OPTICK_EVENT();

    // Previously there was a return from the function if host_initialized == false.
    // The check has been removed, because host_initialized is set to false always before this function is called (bug in the original engine).

    if (gSpriteList)
    {
        for (int i = 0; i < gSpriteCount; ++i)
        {
            SPRITELIST* sprite = &gSpriteList[i];

            if (sprite->pSprite)
                Mod_UnloadSpriteTextures(sprite->pSprite);

            if (sprite->pName)
                Mem_Free(sprite->pName);
        }

        Mem_Free(gSpriteList);
    }

    gpSprite = nullptr;
    gSpriteList = nullptr;
    gLoadedSpriteNames.clear();
    gSpriteCount = 0;
    ghCrosshair = 0;
}

void SPR_Shutdown_NoModelFree()
{
    OPTICK_EVENT();

    // Previously there was a return from the function if host_initialized == false.
    // The check has been removed, because host_initialized is set to false always before this function is called (bug in the original engine).

    if (gSpriteList)
    {
        for (int i = 0; i < gSpriteCount; ++i)
        {
            SPRITELIST* sprite = &gSpriteList[i];

            if (sprite->pName)
                Mem_Free(sprite->pName);
        }

        Mem_Free(gSpriteList);
    }

    gpSprite = nullptr;
    gSpriteList = nullptr;
    gLoadedSpriteNames.clear();
    gSpriteCount = 0;
    ghCrosshair = 0;
}

HSPRITE_t SPR_Load(const char* pTextureName)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_Load(&pTextureName);

    if (pTextureName && gSpriteList && gSpriteCount > 0)
    {
        int i = 0;
        SPRITELIST* sprite = nullptr;

        for (i = 0; i < MAX_SPRITES; i++)
        {
            sprite = &gSpriteList[i];

            if (!sprite->pSprite)
            {
                sprite->pName = (char*) Mem_Malloc(Q_strlen(pTextureName) + 1);
                Q_strcpy(sprite->pName, pTextureName);
            }

            if (!Q_stricmp(pTextureName, sprite->pName))
                break;

            if (gSpriteCount <= i)
                Sys_Error("cannot allocate more than %d HUD sprites\n", MAX_SPRITES);
        }

        gSpriteMipMap = false;
        sprite->pSprite = Mod_ForName(pTextureName, false, true);
        gSpriteMipMap = true;

        if (sprite->pSprite)
        {
            sprite->frameCount = ModelFrameCount(sprite->pSprite);

            gLoadedSpriteNames.emplace(pTextureName);

            return i + 1;
        }
    }

    return 0;
}

bool SPR_IsLoaded(const char* sprite_name)
{
    return gLoadedSpriteNames.contains(sprite_name);
}

void SPR_Set(HSPRITE_t hsprite, int r, int g, int b)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_Set(&hsprite, &r, &g, &b);

    SPRITELIST* sprite = SPR_Get(hsprite);
    if (sprite)
    {
        gpSprite = (msprite_t*) sprite->pSprite->cache.data;
        if (gpSprite)
            qglColor4f(r / 255.0, g / 255.0, b / 255.0, 1.0);
    }
}

void SPR_Draw(int frame, int x, int y, const wrect_t* prc)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_Draw(&frame, &x, &y, &prc);

    if (gpSprite && p_vid->width > x && p_vid->height > y)
    {
        mspriteframe_t* sprframe = R_GetSpriteFrame(gpSprite, frame);
        if (sprframe)
            Draw_SpriteFrame(sprframe, gSpritePalette, x, y, prc);
        else
            Con_DPrintf(ConLogType::Info, "Client.dll SPR_Draw error:  invalid frame\n");
    }
}

void SPR_DrawAdditive(int frame, int x, int y, const wrect_t* prc)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_DrawAdditive(&frame, &x, &y, &prc);

    if (gpSprite && p_vid->width > x && p_vid->height > y)
    {
        mspriteframe_t* sprframe = R_GetSpriteFrame(gpSprite, frame);
        if (sprframe)
            Draw_SpriteFrameAdditive(sprframe, gSpritePalette, x, y, prc);
        else
            Con_DPrintf(ConLogType::Info, "Client.dll SPR_DrawAdditive error:  invalid frame\n");
    }
}

void SPR_DrawHoles(int frame, int x, int y, const wrect_t* prc)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_DrawHoles(&frame, &x, &y, &prc);

    if (gpSprite && p_vid->width > x && p_vid->height > y)
    {
        mspriteframe_t* sprframe = R_GetSpriteFrame(gpSprite, frame);
        if (sprframe)
            Draw_SpriteFrameHoles(sprframe, gSpritePalette, x, y, prc);
        else
            Con_DPrintf(ConLogType::Info, "Client.dll SPR_DrawHoles error:  invalid frame\n");
    }
}

void SPR_DrawGeneric(int frame, int x, int y, const wrect_t* prc, int src, int dest, int width, int height)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_DrawGeneric(&frame, &x, &y, &prc, &src, &dest, &width, &height);

    if (gpSprite && p_vid->width > x && p_vid->height > y)
    {
        mspriteframe_t* sprframe = R_GetSpriteFrame(gpSprite, frame);
        if (sprframe)
            Draw_SpriteFrameGeneric(sprframe, gSpritePalette, x, y, prc, src, dest, width, height);
        else
            Con_DPrintf(ConLogType::Info, "Client.dll SPR_DrawGeneric error: invalid frame\n");
    }
}

void SPR_EnableScissor(int x, int y, int width, int height)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_EnableScissor(&x, &y, &width, &height);

    EnableScissorTest(x, y, width, height);
}

void SPR_DisableScissor()
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_DisableScissor();

    DisableScissorTest();
}

int SPR_Width(HSPRITE_t hsprite, int frame)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_Width(&hsprite, &frame);

    SPRITELIST* sprite = SPR_Get(hsprite);
    if (sprite)
    {
        mspriteframe_t* sprframe = R_GetSpriteFrame((msprite_t*) sprite->pSprite->cache.data, frame);
        if (sprframe)
            return sprframe->width;
    }

    return 0;
}

int SPR_Height(HSPRITE_t hsprite, int frame)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_Height(&hsprite, &frame);

    SPRITELIST* sprite = SPR_Get(hsprite);
    if (sprite)
    {
        mspriteframe_t* sprframe = R_GetSpriteFrame((msprite_t*) sprite->pSprite->cache.data, frame);
        if (sprframe)
            return sprframe->height;
    }

    return 0;
}

int SPR_Frames(HSPRITE_t hsprite)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSPR_Frames(&hsprite);

    SPRITELIST* sprite = SPR_Get(hsprite);
    if (sprite)
        return sprite->frameCount;

    return 0;
}

const model_t* SPR_GetModelPointer(HSPRITE_t hsprite)
{
    OPTICK_EVENT();

    SPRITELIST* sprite = SPR_Get(hsprite);
    if (sprite)
        return sprite->pSprite;

    return nullptr;
}
