#include "cl_main.h"
#include "../engine.h"
#include <optick.h>

#include "cl_private_resources.h"
#include "spriteapi.h"
#include "../analytics.h"
#include "../vgui_int.h"
#include "../console/console.h"
#include "../common/common.h"
#include "../common/model.h"
#include "../common/zone.h"
#include "../common/cvar.h"
#include "../common/com_strings.h"
#include "../common/net_ws.h"
#include "../common/net_chan.h"
#include "../common/verificator.h"
#include "../common/nclm/nclm_proto.h"
#include "../common/nclm/NclmBodyReader.h"
#include "../common/nclm/NclmBodyWriter.h"
#include "../graphics/gl_local.h"

void SetCareerAudioState(int state)
{
    OPTICK_EVENT();

    eng()->SetCareerAudioState.InvokeChained(state);
}

void AddStartupTiming(const char* name)
{
    OPTICK_EVENT();

    eng()->AddStartupTiming.InvokeChained(name);
}

void CL_Disconnect()
{
    OPTICK_EVENT();

    eng()->CL_Disconnect.InvokeChained();
}

void S_BeginPrecaching()
{
    OPTICK_EVENT();

    cl->fPrecaching = 1;
}

void S_EndPrecaching()
{
    OPTICK_EVENT();

    cl->fPrecaching = 0;
}

void CL_ConnectionlessPacket()
{
    OPTICK_EVENT();

    eng()->CL_ConnectionlessPacket.InvokeChained();
}

qboolean CL_IsDevOverviewMode()
{
    OPTICK_EVENT();

    return eng()->CL_IsDevOverviewMode.InvokeChained();
}

void CL_SetDevOverView(refdef_t* refdef)
{
    OPTICK_EVENT();

    return eng()->CL_SetDevOverView.InvokeChained(refdef);
}

const char* CL_CleanFileName(const char* filename)
{
    OPTICK_EVENT();

    const char* pfilename = filename;

    if (COM_CheckString(filename) && filename[0] == '!')
        pfilename = "customization";

    return pfilename;
}

int CL_GetFragmentSize(void* state)
{
    OPTICK_EVENT();

    return eng()->CL_GetFragmentSize.InvokeChained(state);
}

void CL_ConnectClient()
{
    OPTICK_EVENT();

    ContinueLoadingProgressBar("ClientConnect", 2, 0.0);

    if (cls->state == ca_connected)
    {
        if (cls->demoplayback == false)
            Con_DPrintf(ConLogType::Info, "Duplicate connect ack. received.  Ignored.\n");

        return;
    }

    cls->userid = 0;
    cls->trueaddress[0] = 0;

    if (Cmd_Argc() > 2)
    {
        cls->userid = V_atoi(Cmd_Argv(1));
        V_strcpy_safe(cls->trueaddress, Cmd_Argv(2));

        if (Cmd_Argc() > 4)
            cls->build_num = V_atoi(Cmd_Argv(4));
    }

    Netchan_Setup(NS_CLIENT, &cls->netchan, *net_from, -1, &cls, CL_GetFragmentSize);

    NET_ClearLagData(true, false);

    if (fs_startup_timings->value != 0.0)
        AddStartupTiming("connection accepted by server");

    if (net_from->IsLoopback())
        Con_DPrintf(ConLogType::Info, "Connection accepted.\n");
    else
        Con_Printf("Connection accepted by %s\n", NET_AdrToString(*net_from));

    MSG_WriteByte(&cls->netchan.message, clc_ncl_message);
    MSG_WriteLong(&cls->netchan.message, NCLM_HEADER);
    NclmBodyWriter writer(&cls->netchan.message);
    if (!VerificatorGetKeyVersion().empty())
    {
        writer.WriteByte((uint8_t)NCLM_C2S::VERIFICATION_REQUEST);
        writer.WriteString(VerificatorGetKeyVersion());
    }
    else
    {
        writer.WriteByte((uint8_t)NCLM_C2S::DECLARE_VERSION_REQUEST);
        writer.WriteString(va("%d.%d.%d", g_NextClientVersion.major, g_NextClientVersion.minor, g_NextClientVersion.patch));
    }
    writer.Send();

    MSG_WriteByte(&cls->netchan.message, clc_stringcmd);
    MSG_WriteString(&cls->netchan.message, "new\n");

    cls->state = ca_connected;
    cls->demonum = -1;
    cls->signon = 0;
    cls->lastoutgoingcommand = -1;
    cls->connect_time = *realtime;
    cls->nextcmdtime = *realtime;

    cl->validsequence = 0;
    cl->delta_sequence = -1;
}

bool CL_PrecacheResources()
{
    OPTICK_EVENT();

    resource_t *p;

    SetLoadingProgressBarStatusText("#GameUI_PrecachingResources");
    ContinueLoadingProgressBar("ClientConnect", 7, 0.0);

    if (fs_startup_timings->value != (float) 0.0)
        AddStartupTiming("begin CL_PrecacheResources()");

    PrivateRes_PrepareToPrecache();

    for (p = cl->resourcesonhand.pNext; p != &cl->resourcesonhand; p = p->pNext)
    {
        if (p == nullptr)
        {
            AN_AddBreadcrumb("CL_PrecacheResources: cl->resourcesonhand is corrupted, p->pNext == nullptr");
            break;
        }

        if (FBitSet(p->ucFlags, RES_PRECACHED))
            continue;

        switch (p->type)
        {
            case t_sound:
                if (FBitSet(p->ucFlags, RES_WASMISSING))
                {
                    cl->sound_precache[p->nIndex] = nullptr;
                    SetBits(p->ucFlags, RES_PRECACHED);
                    break;
                }

                S_BeginPrecaching();
                cl->sound_precache[p->nIndex] = S_PrecacheSound(p->szFileName);
                S_EndPrecaching();

                if (cl->sound_precache[p->nIndex] || !FBitSet(p->ucFlags, RES_FATALIFMISSING))
                {
                    SetBits(p->ucFlags, RES_PRECACHED);
                    break;
                }

                COM_ExplainDisconnection(true, "Cannot continue without sound %s, disconnecting.", p->szFileName);
                CL_Disconnect();
                return false;

            case t_skin:
            case t_generic:
                SetBits(p->ucFlags, RES_PRECACHED);
                break;

            case t_model:
                if (p->nIndex >= cl->model_precache_count)
                    cl->model_precache_count = std::min(p->nIndex + 1, MAX_MODELS);

                if (p->szFileName[0] == '*')
                {
                    SetBits(p->ucFlags, RES_PRECACHED);
                    break;
                }

                if (fs_lazy_precache->value == 0.0 || !Q_strnicmp(p->szFileName, "maps", 4))
                {
                    cl->model_precache[p->nIndex] = Mod_ForName(p->szFileName, 0, true);
                    if (cl->model_precache[p->nIndex])
                        break;
                }
                else
                {
                    cl->model_precache[p->nIndex] = Mod_FindName(true, p->szFileName);
                }

                if (!cl->model_precache[p->nIndex] && p->ucFlags)
                {
                    Con_Printf("Model %s not found and not available from server\n", p->szFileName);

                    if (FBitSet(p->ucFlags, RES_FATALIFMISSING))
                    {
                        COM_ExplainDisconnection(true, "Cannot continue without model %s, disconnecting.", p->szFileName);
                        CL_Disconnect();

                        return false;
                    }
                }

                if (!Q_stricmp(p->szFileName, "models/player.mdl"))
                    *cl_playerindex = p->nIndex;

                SetBits(p->ucFlags, RES_PRECACHED);
                break;

            case t_decal:
                if (!FBitSet(p->ucFlags, RES_CUSTOM))
                    Draw_DecalSetName(p->nIndex, p->szFileName);

                SetBits(p->ucFlags, RES_PRECACHED);
                break;

            case t_eventscript:
                cl->event_precache[p->nIndex].filename = Mem_Strdup(p->szFileName);
                cl->event_precache[p->nIndex].pszScript = (const char*) COM_LoadFile(p->szFileName, 5, 0);

                if (cl->event_precache[p->nIndex].pszScript)
                {
                    SetBits(p->ucFlags, RES_PRECACHED);
                    break;
                }

                Con_Printf("Script %s not found and not available from server\n", p->szFileName);

                if (!FBitSet(p->ucFlags, RES_FATALIFMISSING))
                {
                    SetBits(p->ucFlags, RES_PRECACHED);
                    break;
                }

                COM_ExplainDisconnection(true, "Cannot continue without script %s, disconnecting.", p->szFileName);
                CL_Disconnect();

                return false;
        }
    }

    if (fs_startup_timings->value != 0.0)
        AddStartupTiming("end  CL_PrecacheResources()");

    // To work private resources we need to reinitialize the sprite subsystem to clear the cache and reinitialize gamedll to pick up new sprites
    SPR_Shutdown();
    SPR_Init();
    eng()->cldll_func->pHudVidInitFunc();

    return true;
}

void CL_HandleNclMessage()
{
    const char* raw_buffer = MSG_ReadString();
    NclmBodyReader body(raw_buffer);

    auto opcode = (NCLM_S2C)body.ReadByte();
    switch (opcode)
    {
        case NCLM_S2C::VERIFICATION_PAYLOAD:
            auto encryptedPayload = body.ReadBuf(NCLM_VERIF_ENCRYPTED_PAYLOAD_SIZE);

            verificator_result_t result;
            if (!VerificatorDecrypt(encryptedPayload, result))
                break;

            MSG_WriteByte(&cls->netchan.message, clc_ncl_message);
            MSG_WriteLong(&cls->netchan.message, NCLM_HEADER);
            NclmBodyWriter writer(&cls->netchan.message);
            writer.WriteByte((int)NCLM_C2S::VERIFICATION_RESPONSE);
            writer.WriteString(va("%d.%d.%d", g_NextClientVersion.major, g_NextClientVersion.minor, g_NextClientVersion.patch));
            writer.WriteBuf(result.data);
            writer.Send();
            break;
    }
}

void CL_Send_CvarValue()
{
    int readcount = *pMsg_readcount;
    long header = MSG_ReadLong();
    if (header == NCLM_HEADER_OLD)
    {
        CL_HandleNclMessage();
        return;
    }
    *pMsg_readcount = readcount;

    const char* cvar_name = MSG_ReadString();

    MSG_WriteByte(&cls->netchan.message, clc_cvarvalue);
    if (V_strlen(cvar_name) >= 255)
    {
        MSG_WriteString(&cls->netchan.message, "Bad CVAR request");
        return;
    }

    cvar_t* cvar = Cvar_FindVar(cvar_name);
    if (!cvar)
    {
        MSG_WriteString(&cls->netchan.message, "Bad CVAR request");
        return;
    }

    if (FBitSet(cvar->flags, FCVAR_PRIVILEGED))
        MSG_WriteString(&cls->netchan.message, "CVAR is privileged");
    else if (FBitSet(cvar->flags, FCVAR_SERVER))
        MSG_WriteString(&cls->netchan.message, "CVAR is server-only");
    else if (FBitSet(cvar->flags, FCVAR_PROTECTED))
        MSG_WriteString(&cls->netchan.message, "CVAR is protected");
    else
        MSG_WriteString(&cls->netchan.message, cvar->string);
}
