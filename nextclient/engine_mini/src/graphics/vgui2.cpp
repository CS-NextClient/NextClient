#include "engine.h"

#include <easylogging++.h>

#include "text/text_surface.h"

void VGUI2_Draw_Init()
{
    eng()->VGUI2_Draw_Init();
}

void VGUI2_ResetCurrentTexture()
{
    eng()->VGUI2_ResetCurrentTexture();
}

void VGUI2_Draw_SetTextColor(int r, int g, int b)
{
    eng()->VGUI2_Draw_SetTextColor(r, g, b);
}

// The engine keeps its own pointer to its surface, and so does stock client.dll, which resolves it by
// name and draws the scoreboard, the buy menu and the rest of the in-game panels through it. Neither
// pointer is ours to redirect, so the text slots of the object they share are replaced instead.
void VGUI2_InstallTextSurface()
{
#ifdef _WIN32
    void* surface = eng()->vgui2_surface ? *eng()->vgui2_surface : nullptr;

    if (!surface)
    {
        LOG(ERROR) << "The engine's vgui2 surface was not found, its text path stays as it is";
        return;
    }

    TextSurface_Install(surface);
#endif
}

// Undo of VGUI2_InstallTextSurface, for the engine shutdown that precedes unloading this module.
void VGUI2_UninstallTextSurface()
{
#ifdef _WIN32
    TextSurface_Uninstall();
#endif
}
