#include "../engine.h"
#include "sys_dll.h"
#include "cvar.h"
#include "filesystem.h"
#include "../console/console.h"
#include "../pr_cmds.h"
#include "../vgui_int.h"
#include "../server/sv_remoteaccess.h"
#include "../client/cl_scrn.h"

void Host_InitializeGameDLL()
{
    eng()->Host_InitializeGameDLL.InvokeChained();
}

void Host_Map(qboolean bIsDemo, const char* mapstring, const char* mapName, qboolean loadGame)
{
    eng()->Host_Map.InvokeChained(bIsDemo, mapstring, mapName, loadGame);
}

void Host_Map_f()
{
    char mapstring[64];
    char name[64];
    CareerStateType careerState = *g_careerState;

    if (*cmd_source != src_command)
    {
        *g_careerState = CAREER_NONE;
        return;
    }

    if (g_engfuncs.pfnCmd_Argc() > 1 && Q_strlen(g_engfuncs.pfnCmd_Args()) > 54)
    {
        *g_careerState = CAREER_NONE;
        Con_Printf("map change failed: command string is too long.\n");
        return;
    }

    if (g_engfuncs.pfnCmd_Argc() < 2)
    {
        *g_careerState = CAREER_NONE;
        Con_Printf("map <levelname> : changes server to specified map\n");
        return;
    }

    CL_Disconnect();
    //TODO: what it? why is this?
    if (careerState == CAREER_LOADING)
        *g_careerState = CAREER_LOADING;

    if (COM_CheckParm("-steam") && PF_IsDedicatedServer())
        *g_bMajorMapChange = TRUE;

    FS_LogLevelLoadStarted("Map_Common");
    mapstring[0] = 0;
    for (int i = 0; i < g_engfuncs.pfnCmd_Argc(); i++)
    {
        Q_strncat(mapstring, g_engfuncs.pfnCmd_Argv(i), 62 - Q_strlen(mapstring));
        Q_strncat(mapstring, " ", 62 - Q_strlen(mapstring));
    }
    Q_strcat(mapstring, "\n", sizeof(mapstring));
    Q_strncpy(name, g_engfuncs.pfnCmd_Argv(1), sizeof(name));

    if (!g_psvs->dll_initialized)
        Host_InitializeGameDLL();

    int iLen = Q_strlen(name);
    if (iLen > 4 && !Q_stricmp(&name[iLen - 4], ".bsp"))
        name[iLen - 4] = 0;

    FS_LogLevelLoadStarted(name);

    if (!PF_IsMapValid_I(name))
    {
        Con_Printf("map change failed: '%s' not found on server.\n", name);
        if (COM_CheckParm("-steam"))
        {
            if (PF_IsDedicatedServer())
            {
                *g_bMajorMapChange = FALSE;
                Sys_Printf("\n");
            }
        }
        return;
    }

    VGuiWrap2_LoadingStarted("level", name);
    SCR_UpdateScreen();
    SCR_UpdateScreen();

    StartLoadingProgressBar("Server", 24);
    SetLoadingProgressBarStatusText("#GameUI_StartingServer");
    ContinueLoadingProgressBar("Server", 1, 0.0);
    Cvar_Set("HostMap", name);
    Host_Map(FALSE, mapstring, name, FALSE);
    if (PF_IsDedicatedServer() && COM_CheckParm("-steam"))
    {
        *g_bMajorMapChange = FALSE;
        Sys_Printf("\n");
    }
    ContinueLoadingProgressBar("Server", 11, 0.0);
    NotifyDedicatedServerUI("UpdateMap");

    if (*g_careerState == CAREER_LOADING)
    {
        *g_careerState = CAREER_PLAYING;
        SetCareerAudioState(1);
    }
    else
        SetCareerAudioState(0);
}
