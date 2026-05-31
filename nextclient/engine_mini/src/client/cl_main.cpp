#include "engine.h"
#include "cl_main.h"

#include <optick.h>
#include <next_engine_mini/nclm_proto.h>

#include "ncl_entity/cl_ncl_entity_sync.h"
#include "graphics/gl_local.h"
#include "console/console.h"
#include "steam/steam_api.h"
#include "common/common.h"
#include "common/model.h"
#include "common/zone.h"
#include "common/cvar.h"
#include "common/com_strings.h"
#include "common/filesystem.h"
#include "common/net_ws.h"
#include "common/net_chan.h"
#include "common/host.h"
#include "common/nclm/NclmBodyReader.h"
#include "common/nclm/NclmBodyWriter.h"
#include "common/nclm/hwid_collector.h"
#include "common/nclm/hwid_sender.h"
#include "cl_demo.h"
#include "spriteapi.h"
#include "vgui_int.h"
#include "cl_private_resources.h"
#include "cl_string_registry.h"

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

void CL_ClearState(qboolean quiet)
{
    OPTICK_EVENT();

    eng()->CL_ClearState.InvokeChained(quiet);
}

void CL_Disconnect()
{
    OPTICK_EVENT();

    cls->connect_time = -99999.0;
    cls->connect_retry = 0;

    PrivateRes_Clear();
    client_stateex.resourcesNeeded.clear();

    *p_g_careerState = CAREER_NONE;

    SetCareerAudioState(0);
    SPR_Shutdown_NoModelFree();
    S_StopAllSounds(true);

    if (cls->demoplayback)
    {
        CL_StopPlayback();
    }
    else if (cls->state == ca_connected || cls->state == ca_active || cls->state == ca_uninitialized || cls->state == ca_connecting)
    {
        if (cls->demorecording)
        {
            CL_Stop_f();
        }

        FS_LogLevelLoadStarted("ExitGame");

        if (cls->passive)
        {
            NET_LeaveGroup(cls->netchan.sock, cls->connect_stream);
            NET_LeaveGroup(cls->netchan.sock, cls->game_stream);
        }
        else if (cls->netchan.remote_address.GetType() != NA_UNUSED)
        {
            char final[32];
            final[0] = clc_stringcmd;
            V_strcpy(&final[1], "dropclient\n");

            Netchan_Transmit(&cls->netchan, 13, reinterpret_cast<uint8_t*>(final));
            Netchan_Transmit(&cls->netchan, 13, reinterpret_cast<uint8_t*>(final));
            Netchan_Transmit(&cls->netchan, 13, reinterpret_cast<uint8_t*>(final));
        }

        cls->state = ca_disconnected;

        netadr_t* cur_addr = p_net_local_adr;
        if (p_g_GameServerAddress->GetType() != NA_LOOPBACK)
        {
            cur_addr = p_g_GameServerAddress;
        }

        SteamUser()->TerminateGameConnection(cur_addr->GetIPHostByteOrder(), cur_addr->GetPortHostByteOrder());
        V_memset(p_g_GameServerAddress, 0, sizeof(netadr_t));

        if (Host_IsServerActive())
        {
            Host_ShutdownServer(false);
        }

        // TODO: The original engine calls CloseSecurityModule() here, but there is no hook for it in NextClient yet.
        // Not relevant for CS 1.6, since CS is not shipped as a protected module.
    }

    cls->timedemo = false;
    cls->demoplayback = false;
    cls->signon = 0;

    CL_ClearState(true);

    Netchan_Clear(&cls->netchan);
    CL_DeallocateDynamicData();

    scr_downloading->value = -1.0;
    sys_timescale->value = 1.0;
    *p_g_LastScreenUpdateTime = 0.0;

    VGuiWrap2_NotifyOfServerDisconnect();
    StopLoadingProgressBar();

    hwid::Reset();
    CL_CvarsSandboxClear();
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

void CL_ClearCaches()
{
    OPTICK_EVENT();

    for (int i = 1; i < ARRAYSIZE(cl->event_precache) && cl->event_precache[i].pszScript; ++i)
    {
        Mem_Free((void*)cl->event_precache[i].pszScript);
        Mem_Free((void*)cl->event_precache[i].filename);

        Q_memset(&cl->event_precache[i], 0, sizeof(cl->event_precache[i]));
    }
}

void CL_ClearClientState()
{
    OPTICK_EVENT();

    CL_NclEntitySyncClear();
    CL_StringRegistryClear();

    for (auto& frame : cl->frames)
    {
        if (frame.packet_entities.entities)
        {
            Mem_Free(frame.packet_entities.entities);
        }

        frame.packet_entities.entities = nullptr;
        frame.packet_entities.num_entities = 0;
    }

    CL_ClearResourceLists();

    for (auto& player : cl->players)
    {
        COM_ClearCustomizationList(&player.customdata, false);
    }

    CL_ClearCaches();

    Q_memset(cl, 0, sizeof(client_state_t));

    cl->resourcesneeded.pPrev = &cl->resourcesneeded;
    cl->resourcesneeded.pNext = &cl->resourcesneeded;
    cl->resourcesonhand.pPrev = &cl->resourcesonhand;
    cl->resourcesonhand.pNext = &cl->resourcesonhand;

    CL_CreateResourceList();
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

    sizebuf_t* msgbuf = &cls->netchan.message;

    if (g_NclmVerificator != nullptr)
    {
        char version[32];
        g_NclmVerificator->GetVersion(version, sizeof(version));

        MSG_WriteByte(msgbuf, clc_ncl_message);
        MSG_WriteLong(msgbuf, NCLM_HEADER_OLD);
        MSG_WriteByte(msgbuf, (int)NCLM_C2S::VERIFICATION_REQUEST);
        MSG_WriteString(msgbuf, version);
    }
    else
    {
        MSG_WriteByte(msgbuf, clc_ncl_message);
        MSG_WriteLong(msgbuf, NCLM_HEADER);
        NclmBodyWriter writer(msgbuf);
        writer.WriteByte((int)NCLM_C2S::DECLARE_VERSION_REQUEST);
        writer.WriteString(va("%d.%d.%d", g_NextClientVersion.major, g_NextClientVersion.minor, g_NextClientVersion.patch));
        writer.Send();
    }

    MSG_WriteByte(msgbuf, clc_stringcmd);
    MSG_WriteString(msgbuf, "new\n");

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

    SetLoadingProgressBarStatusText("#GameUI_PrecachingResources");
    ContinueLoadingProgressBar("ClientConnect", 7, 0.0);

    if (fs_startup_timings->value != (float)0.0)
        AddStartupTiming("begin CL_PrecacheResources()");

    PrivateRes_PrepareToPrecache();

    for (resource_t *pResource = cl->resourcesonhand.pNext, *pNext; pResource != &cl->resourcesonhand; pResource = pNext)
    {
        pNext = pResource->pNext;

        if (FBitSet(pResource->ucFlags, RES_PRECACHED))
            continue;

        switch (pResource->type)
        {
            case t_sound:
                if (FBitSet(pResource->ucFlags, RES_WASMISSING))
                {
                    cl->sound_precache[pResource->nIndex] = nullptr;
                    SetBits(pResource->ucFlags, RES_PRECACHED);
                    break;
                }

                S_BeginPrecaching();
                cl->sound_precache[pResource->nIndex] = S_PrecacheSound(pResource->szFileName);
                S_EndPrecaching();

                if (cl->sound_precache[pResource->nIndex] || !FBitSet(pResource->ucFlags, RES_FATALIFMISSING))
                {
                    SetBits(pResource->ucFlags, RES_PRECACHED);
                    break;
                }

                COM_ExplainDisconnection(true, "Cannot continue without sound %s, disconnecting.", pResource->szFileName);
                CL_Disconnect();
                return false;

            case t_skin:
            case t_generic:
                SetBits(pResource->ucFlags, RES_PRECACHED);
                break;

            case t_model:
                if (pResource->nIndex >= cl->model_precache_count)
                    cl->model_precache_count = std::min(pResource->nIndex + 1, MAX_MODELS);

                if (pResource->szFileName[0] == '*')
                {
                    SetBits(pResource->ucFlags, RES_PRECACHED);
                    break;
                }

                if (fs_lazy_precache->value == 0.0 || !Q_strnicmp(pResource->szFileName, "maps", 4))
                {
                    cl->model_precache[pResource->nIndex] = Mod_ForName(pResource->szFileName, 0, true);
                    if (cl->model_precache[pResource->nIndex])
                        break;
                }
                else
                {
                    cl->model_precache[pResource->nIndex] = Mod_FindName(true, pResource->szFileName);
                }

                if (!cl->model_precache[pResource->nIndex] && pResource->ucFlags)
                {
                    Con_Printf("Model %s not found and not available from server\n", pResource->szFileName);

                    if (FBitSet(pResource->ucFlags, RES_FATALIFMISSING))
                    {
                        COM_ExplainDisconnection(true, "Cannot continue without model %s, disconnecting.", pResource->szFileName);
                        CL_Disconnect();

                        return false;
                    }
                }

                if (!Q_stricmp(pResource->szFileName, "models/player.mdl"))
                    *cl_playerindex = pResource->nIndex;

                SetBits(pResource->ucFlags, RES_PRECACHED);
                break;

            case t_decal:
                if (!FBitSet(pResource->ucFlags, RES_CUSTOM))
                    Draw_DecalSetName(pResource->nIndex, pResource->szFileName);

                SetBits(pResource->ucFlags, RES_PRECACHED);
                break;

            case t_eventscript:
                cl->event_precache[pResource->nIndex].filename = Mem_Strdup(pResource->szFileName);
                cl->event_precache[pResource->nIndex].pszScript = (const char*)COM_LoadFile(pResource->szFileName, 5, 0);

                if (cl->event_precache[pResource->nIndex].pszScript)
                {
                    SetBits(pResource->ucFlags, RES_PRECACHED);
                    break;
                }

                Con_Printf("Script %s not found and not available from server\n", pResource->szFileName);

                if (!FBitSet(pResource->ucFlags, RES_FATALIFMISSING))
                {
                    SetBits(pResource->ucFlags, RES_PRECACHED);
                    break;
                }

                COM_ExplainDisconnection(true, "Cannot continue without script %s, disconnecting.", pResource->szFileName);
                CL_Disconnect();

                return false;
        }
    }

    if (fs_startup_timings->value != 0.0)
        AddStartupTiming("end  CL_PrecacheResources()");

    // To work private resources we need to reinitialize gamedll to pick up new sprites
    eng()->cldll_func->pHudVidInitFunc();

    return true;
}

void CL_HandleNclMessage()
{
    OPTICK_EVENT();

    const char* raw_buffer = MSG_ReadString();

    NclmBodyReader body(raw_buffer);
    NCLM_S2C opcode = static_cast<NCLM_S2C>(body.ReadByte());

    switch (opcode)
    {
        case NCLM_S2C::VERIFICATION_PAYLOAD:
            {
                std::vector<uint8_t> encrypted_payload = body.ReadBuf(NCLM_VERIF_ENCRYPTED_PAYLOAD_SIZE);

                if (g_NclmVerificator == nullptr)
                {
                    break;
                }

                // Protocol quirks across server module versions:
                // - 1.4.0 sends and consumes 256 bytes.
                // - 1.5.0 sends 196 bytes and should stop processing after consuming
                //   the payload, but it does not; control passes back to ReHLDS,
                //   which then attempts to continue parsing from the wrong offset,
                //   causing an invalid read.
                // - 1.5.1+ sends 196 bytes and correctly halts further processing by
                //   breaking the handler chain.
                // Because we cannot distinguish the server version at this stage,
                // we always respond with the full 256-byte padded buffer so that
                // 1.5.0 does not trigger the invalid read.

                uint8_t result[NCLM_VERIF_PAYLOAD_SIZE_PADDED];
                V_memset(result, 0, sizeof(result));

                size_t written_size =
                    g_NclmVerificator->DecryptPayload(encrypted_payload.data(), encrypted_payload.size(), result, sizeof(result));

                if (written_size != NCLM_VERIF_PAYLOAD_SIZE)
                {
                    break;
                }

                sizebuf_t* msgbuf = &cls->netchan.message;

                MSG_WriteByte(msgbuf, clc_ncl_message);
                MSG_WriteLong(msgbuf, NCLM_HEADER_OLD);
                MSG_WriteByte(msgbuf, static_cast<int>(NCLM_C2S::VERIFICATION_RESPONSE));
                MSG_WriteString(msgbuf, va("%d.%d.%d", g_NextClientVersion.major, g_NextClientVersion.minor, g_NextClientVersion.patch));
                MSG_WriteBuf(msgbuf, sizeof(result), result);

                hwid::SendToServer(msgbuf);
                break;
            }
    }
}

void CL_Send_CvarValue()
{
    OPTICK_EVENT();

    // nclm protocol check
    int readcount = *pMsg_readcount;
    long header = MSG_ReadLong();
    if (header == NCLM_HEADER_OLD)
    {
        CL_HandleNclMessage();
        return;
    }
    *pMsg_readcount = readcount;
    // end nclm protocol check

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

void CL_StopPlayback()
{
    OPTICK_EVENT();

    eng()->CL_StopPlayback.InvokeChained();
}
