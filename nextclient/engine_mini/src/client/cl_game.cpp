#include "../engine.h"
#include <optick.h>
#include <goldsrc/client_api.h>
#include <goldsrc/types.h>
#include "spriteapi.h"
#include "graphics/render/MaterialEdit.h"

goldsrc::HSPRITE_t ghCrosshair;
goldsrc::wrect_t gCrosshairRc;
int gCrosshairR;
int gCrosshairG;
int gCrosshairB;

void SetCrosshair(goldsrc::HSPRITE_t hspr, goldsrc::wrect_t rc, int r, int g, int b)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSetCrosshair(&hspr, &rc, &r, &g, &b);
    ghCrosshair = hspr;
    gCrosshairRc = rc;
    gCrosshairR = r;
    gCrosshairG = g;
    gCrosshairB = b;
}

void DrawCrosshair(int x, int y)
{
    OPTICK_EVENT();

    // The engine centres this on the whole screen. While a panel is docked, the world is not
    // on the whole screen, and a crosshair that stays put is pointing at nothing.
    x -= render::MaterialEdit_AppliedInset() / 2;

    if (ghCrosshair)
    {
        SPR_Set(ghCrosshair, gCrosshairR, gCrosshairG, gCrosshairB);
        SPR_DrawHoles(0, x - (gCrosshairRc.right - gCrosshairRc.left) / 2, y - (gCrosshairRc.bottom - gCrosshairRc.top) / 2, &gCrosshairRc);
    }
}
