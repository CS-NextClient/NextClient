#include <md5.h>
#include <common/filesystem.h>

#include "../engine.h"
#include "download.h"
#include "cl_spectator.h"
#include "cl_main.h"
#include "cl_private_resources.h"
#include "../analytics.h"
#include "../vgui_int.h"
#include "../console/console.h"
#include "../common/sys_dll.h"
#include "../common/net_buffer.h"
#include "../common/net_chan.h"
#include "../common/net_ws.h"
#include "../common/zone.h"
#include "../common/com_strings.h"
#include "../common/common.h"
#include "../common/model.h"
#include "../graphics/gl_local.h"

void CL_SendConsistencyInfo(sizebuf_t *msg)
{
    eng()->CL_SendConsistencyInfo.InvokeChained(msg);
}

void Net_APICheckTimeouts()
{
    eng()->Net_APICheckTimeouts.InvokeChained();
}

void CL_ReallocateDynamicData(int maxclients)
{
    eng()->CL_ReallocateDynamicData.InvokeChained(maxclients);
}

void CL_PrecacheBSPModels(char* pfilename)
{
    if (!pfilename)
        return;

    if (Q_strnicmp(pfilename, "maps/", Q_strlen("maps/")) != 0)
        return;

    if (!Q_strstr(pfilename, ".bsp"))
        return;

    ContinueLoadingProgressBar("ClientConnect", 9, 0.0);

    for (resource_t* pResource = cl->resourcesonhand.pNext, * pNext; pResource != &cl->resourcesonhand; pResource = pNext)
    {
        pNext = pResource->pNext;

        if (pResource->type != t_model || pResource->szFileName[0] != '*')
            continue;

        cl->model_precache[pResource->nIndex] = Mod_ForName(pResource->szFileName, false, false);

        if (!cl->model_precache[pResource->nIndex])
        {
            Con_Printf("Model %s not model_found\n", pResource->szFileName);

            if (FBitSet(pResource->ucFlags, RES_FATALIFMISSING))
            {
                COM_ExplainDisconnection(true, "Cannot continue without model %s, disconnecting.", pfilename);
                CL_Disconnect();
            }
        }
    }
}

void CL_RegisterResources(sizebuf_t *msg)
{
    int mungebuffer[4];

    if (cls->dl.custom || (cls->signon == 2 && cls->state == ca_active))
    {
        cls->dl.custom = false;
        return;
    }

    if (fs_startup_timings->value != 0.0)
        AddStartupTiming("begin CL_RegisterResources()");

    ContinueLoadingProgressBar("ClientConnect", 8, 0.0);
    if (cls->demoplayback == false)
        CL_SendConsistencyInfo(msg);

    cl->worldmodel = cl->model_precache[1];

    if (cl->model_precache[1] && cl->maxclients > 0)
    {
        if (!*p_cl_entities)
            CL_ReallocateDynamicData(cl->maxclients);

        (*p_cl_entities)->model = cl->model_precache[1];

        CL_PrecacheBSPModels(cl->model_precache[1]->name);

        if (cls->state != ca_disconnected)
        {
            Con_DPrintf(ConLogType::Info, "Setting up renderer...\n");
            R_NewMap();
            Hunk_Check();
            *p_noclip_anglehack = false;

            if (cls->passive == false)
            {
                mungebuffer[0] = cl->mapCRC;
                MSG_WriteByte(msg, clc_stringcmd);
                COM_Munge2((uint8_t*) mungebuffer, 4, (uint8_t) (-1 - cl->servercount));
                MSG_WriteString(msg, va("spawn %i %i", cl->servercount, mungebuffer[0]));
            }

            CL_InitSpectator();

            if (fs_startup_timings->value != 0.0)
                AddStartupTiming("end   CL_RegisterResources()");
        }
    }
    else
    {
        Con_Printf("Client world model is NULL\n");
        COM_ExplainDisconnection(true, "Client world model is NULL\n");
        CL_Disconnect();
    }
}

void CL_ParseServerMessage(qboolean normal_message)
{
    eng()->CL_ParseServerMessage.InvokeChained(normal_message);
}

int CL_EstimateNeededResources()
{
    int nTotalSize = 0;

    client_stateex.resourcesNeeded.clear();

    for (resource_t* pResource = cl->resourcesneeded.pNext, * pNext; pResource != &cl->resourcesneeded; pResource = pNext)
    {
        pNext = pResource->pNext;

        ResourceDescriptor resource_descriptor = ResourceDescriptorFactory::MakeByResourceT(pResource);

        switch (pResource->type)
        {
            case t_sound:
                if (pResource->szFileName[0] != '*' && resource_descriptor.NeedToDownload())
                {
                    SetBits(pResource->ucFlags, RES_WASMISSING);
                    nTotalSize += pResource->nDownloadSize;
                    client_stateex.resourcesNeeded[pResource->szFileName] = *pResource;
                }
                break;
            case t_model:
                if (pResource->szFileName[0] != '*' && resource_descriptor.NeedToDownload())
                {
                    SetBits(pResource->ucFlags, RES_WASMISSING);
                    nTotalSize += pResource->nDownloadSize;
                    client_stateex.resourcesNeeded[pResource->szFileName] = *pResource;
                }
                break;
            case t_skin:
            case t_generic:
            case t_eventscript:
                if (resource_descriptor.NeedToDownload())
                {
                    SetBits(pResource->ucFlags, RES_WASMISSING);
                    nTotalSize += pResource->nDownloadSize;
                    client_stateex.resourcesNeeded[pResource->szFileName] = *pResource;
                }
                break;
            case t_decal:
                if (FBitSet(pResource->ucFlags, RES_CUSTOM))
                {
                    SetBits(pResource->ucFlags, RES_WASMISSING);
                    nTotalSize += pResource->nDownloadSize;
                    client_stateex.resourcesNeeded[pResource->szFileName] = *pResource;
                }
                break;
            case t_world:
                ASSERT(0);
                break;
            default: ;
        }
    }

    return nTotalSize;
}

qboolean CL_RequestMissingResources()
{
    if (!cls->dl.doneregistering && (cls->dl.custom || cls->state == ca_uninitialized))
    {
        resource_t* pResource = cl->resourcesneeded.pNext;

        if (pResource == &cl->resourcesneeded)
        {
            cls->dl.doneregistering = true;
            cls->dl.custom = false;
        }
        else if (!FBitSet(pResource->ucFlags, RES_WASMISSING))
        {
            CL_MoveToOnHandList(cl->resourcesneeded.pNext);
            return true;
        }
    }
    return false;
}

void CL_RegisterCustomization(resource_t* resource)
{
    qboolean bFound = false;
    customization_t* pList;

    for (pList = cl->players[resource->playernum].customdata.pNext; pList; pList = pList->pNext)
    {
        if (!memcmp(pList->resource.rgucMD5_hash, resource->rgucMD5_hash, sizeof(resource->rgucMD5_hash)))
        {
            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        player_info_t* player = &cl->players[resource->playernum];

        if (!COM_CreateCustomization(&player->customdata, resource, resource->playernum, FCUST_FROMHPAK, NULL, NULL))
            Con_Printf("Unable to create custom decal for player %i\n", resource->playernum);
    }
    else
    {
        Con_DPrintf(ConLogType::Info, "Duplicate resource received and ignored.\n");
    }
}

void CL_ProcessFile_0(qboolean successfully_received, const char* filename)
{
    resource_t* pResource;

    if (successfully_received)
    {
        if (filename && filename[0] != '!')
            Con_Printf("processing %s\n", CL_CleanFileName(filename));
    }
    else
    {
        AN_AddBreadcrumb(va("CL_ProcessFile_0(successfully_received: %d, filename: %s)", successfully_received, filename));
        Con_Printf(S_ERROR "server failed to transmit file '%s'\n", CL_CleanFileName(filename));
    }

    for (pResource = cl->resourcesneeded.pNext; pResource != &cl->resourcesneeded; pResource = pResource->pNext)
    {
        if (!Q_strnicmp(filename, "!MD5", 4))
        {
            unsigned char rgucMD5_hash[16];
            COM_HexConvert(filename + 4, 32, rgucMD5_hash);

            if (!V_memcmp(pResource->rgucMD5_hash, rgucMD5_hash, sizeof(rgucMD5_hash)))
                break;

            continue;
        }

        if (pResource->type == t_generic)
        {
            if (!Q_stricmp(pResource->szFileName, filename))
                break;
        }
        else
        {
            const char* pfilename = filename;

            if (!Q_strnicmp(filename, DEFAULT_SOUNDPATH, Q_strlen(DEFAULT_SOUNDPATH)))
                pfilename += Q_strlen(DEFAULT_SOUNDPATH);

            if (!Q_stricmp(pResource->szFileName, pfilename))
                break;
        }
    }

    if (pResource != &cl->resourcesneeded)
    {
        if (successfully_received)
            ClearBits(pResource->ucFlags, RES_WASMISSING);

        if (filename[0] == '!')
        {
            if (cls->netchan.tempbuffer)
            {
                if (pResource->nDownloadSize == cls->netchan.tempbuffersize)
                {
                    if (pResource->ucFlags & RES_CUSTOM)
                    {
                        HPAK_AddLump(true, CUSTOM_RES_PATH, pResource, (byte*) cls->netchan.tempbuffer, NULL);
                        CL_RegisterCustomization(pResource);
                    }
                }
                else
                {
                    Con_Printf("Downloaded %i bytes for purported %i byte file, ignoring download\n",
                               cls->netchan.tempbuffersize, pResource->nDownloadSize);
                }

                if (cls->netchan.tempbuffer)
                    Mem_Free(cls->netchan.tempbuffer);
            }

            cls->netchan.tempbuffersize = 0;
            cls->netchan.tempbuffer = NULL;
        }

        // moving to 'onhandle' list even if file was missed
        CL_MoveToOnHandList(pResource);
    }

    if (cls->state != ca_disconnected)
    {
        if (cl->resourcesneeded.pNext == &cl->resourcesneeded)
        {
            byte msg_buf[MAX_INIT_MSG];
            sizebuf_t msg;

            MSG_Init(&msg, "Resource Registration", msg_buf, sizeof(msg_buf));

            if (CL_PrecacheResources())
                CL_RegisterResources(&msg);

            if (msg.cursize > 0)
            {
                Netchan_CreateFragments(false, &cls->netchan, &msg);
                Netchan_FragSend(&cls->netchan);
            }
        }

        if (cls->netchan.tempbuffer)
        {
            Con_Printf("CL_ProcessFile: Received a decal %s, but didn't find it in resources needed list!\n", filename);
            Mem_Free(cls->netchan.tempbuffer);
        }

        cls->netchan.tempbuffer = NULL;
        cls->netchan.tempbuffersize = 0;
    }
}

void CL_ReadPackets()
{
    const int kMaxPackets = 250;

    int packets_count = 0;

    if (cls->state == ca_dedicated)
        return;

    cl->oldtime = cl->time;
    cl->time = cl->time + *host_frametime;

    while (NET_GetPacket_0())
    {
        if (++packets_count > kMaxPackets && !cls->demoplayback)
        {
            Con_DPrintf(ConLogType::Info, "Ignored %i messages", packets_count - kMaxPackets);
            break;
        }

        if (*(int *) net_message->data == -1)
        {
            CL_ConnectionlessPacket();
            continue;
        }

        if (!cls->passive)
        {
            if (!cls->demoplayback && !NET_CompareAdr(*net_from, cls->netchan.remote_address))
                continue;

            if (cls->state == ca_disconnected || cls->state == ca_connecting)
                continue;
        }
        else if (cls->state == ca_connecting)
        {
            cls->state = ca_connected;
        }
        else if (cls->state == ca_disconnected)
        {
            continue;
        }

        if (cls->demoplayback)
        {
            MSG_BeginReading();
            CL_ParseServerMessage(TRUE);
            continue;
        }

        if (net_message->cursize < 8)
        {
            Con_Printf("CL_ReadPackets:  undersized packet %i bytes from %s\n", net_message->cursize, NET_AdrToString(*net_from));
            continue;
        }

        if (Netchan_Process(&cls->netchan))
        {
            CL_ParseServerMessage(TRUE);
            continue;
        }
    }

    CL_SetSolidEntities();

    if (cls->state != ca_dedicated && cls->state != ca_disconnected && Netchan_IncomingReady(&cls->netchan))
    {
        if (Netchan_CopyNormalFragments(&cls->netchan))
        {
            MSG_BeginReading();
            CL_ParseServerMessage(FALSE);
        }

        if (Netchan_CopyFileFragments(&cls->netchan))
        {
            if (gDownloadFile[0])
            {
                if (client_stateex.privateResListState == PrivateResListState::DownloadingResList &&
                    V_strncmp(gDownloadFile, client_stateex.privateResListDownloadPath.c_str(), kDownloadFileSize) == 0)
                {
                    // do nothing yet...
                }
                else
                {
                    if (gDownloadFile[0] == '!')
                        SetSecondaryProgressBarText("#GameUI_SecurityModule");
                    else
                        SetSecondaryProgressBarText(va("File '%s'", gDownloadFile));
                }

                SetSecondaryProgressBar(100.0);
            }

            for (const auto& res_descriptor : ResourceDescriptorFactory::MakeByDownloadPath(cls->netchan.incomingfilename))
            {
                CL_ProcessFile_0(TRUE, res_descriptor.get_filename().c_str());
            }
        }
    }

    if (cls->state < ca_connected || cls->demoplayback || *realtime - cls->netchan.last_received <= cl_timeout->value)
    {
        if (cl_shownet->value != 0.0)
            Con_Printf("\n");

        Netchan_UpdateProgress(&cls->netchan);

        if (scr_downloading->value > 0.0 && gDownloadFile[0])
        {
            if (client_stateex.privateResListState == PrivateResListState::DownloadingResList &&
                !Q_strncmp(gDownloadFile, client_stateex.privateResListDownloadPath.c_str(), kDownloadFileSize))
            {
                SetLoadingProgressBarStatusText("#GameUI_DownloadingResourceListOverrides");
            }
            else
            {
                if (gDownloadFile[0] == '!')
                    SetSecondaryProgressBarText("#GameUI_DownloadingSecurityModule");
                else
                    SetSecondaryProgressBarText(va("Downloading file '%s'", gDownloadFile));

                SetSecondaryProgressBar(scr_downloading->value / 100.f);
                SetLoadingProgressBarStatusText("#GameUI_VerifyingAndDownloading");
            }
        }
        Net_APICheckTimeouts();
    }
    else
    {
        COM_ExplainDisconnection(TRUE, "#GameUI_ServerConnectionTimeout");
        CL_Disconnect();
    }
}

/**
 * \brief The function checks if the file exists on the client. If the file does not exist, it adds the file to the download via http manager or dlfile.
 * \param msg
 * \param res_descriptor
 * \param dlfile_resources is used to prevent a file from being re-added to a download via a dlfile
 * \return true if file exists, and false if not.
 */
bool CL_CheckFile(sizebuf_t *msg, const ResourceDescriptor& res_descriptor, std::unordered_set<std::string>& dlfile_resources)
{
    if (GetCvarFloat("cl_allowdownload") == 0.0)
    {
        Con_DPrintf(ConLogType::Info, "Download refused, cl_allowdownload is 0\n");
        return true;
    }

    if (cls->state != ca_active || GetCvarFloat("cl_download_ingame") != 0.0)
    {
        if (!res_descriptor.NeedToDownload())
            return true;

        if (cls->passive || cls->demoplayback)
        {
            Con_Printf("Warning! File %s missing during demo playback.\n", res_descriptor.get_download_path().c_str());
            return true;
        }

        if (eng()->CL_CanUseHTTPDownload.InvokeChained())
        {
            CL_QueueHTTPDownload(res_descriptor);
        }
        else
        {
            if (!dlfile_resources.contains(res_descriptor.get_download_path()))
            {
                dlfile_resources.emplace(res_descriptor.get_download_path());

                MSG_WriteByte(msg, clc_stringcmd);
                MSG_WriteString(msg, va("dlfile %s", res_descriptor.get_download_path().c_str()));
            }
        }

        return false;
    }

    Con_DPrintf(ConLogType::Info, "In-game download refused...\n");
    return true;
}

void CL_BatchResourceRequest()
{
    byte data[65536];
    char filename[256];
    std::unordered_set<std::string> dlfile_resources;

    CL_HTTPStop_f();

    sizebuf_t msg;
    MSG_Init(&msg, "Resource Batch", data, sizeof(data));

    for (resource_t* pResource = cl->resourcesneeded.pNext, * pNext; pResource != &cl->resourcesneeded; pResource = pNext)
    {
        pNext = pResource->pNext;

        if (!FBitSet(pResource->ucFlags, RES_WASMISSING))
        {
            CL_MoveToOnHandList(pResource);
            continue;
        }

        if (cls->state == ca_active && !cl_download_ingame->value)
        {
            Con_Printf("skipping in game download of %s\n", pResource->szFileName);
            CL_MoveToOnHandList(pResource);
            continue;
        }

        if (!IsSafeFileToDownload(pResource->szFileName))
        {
            CL_RemoveFromResourceList(pResource);
            Con_Printf("Invalid file type...skipping download of %s\n", pResource->szFileName);
            Mem_Free(pResource);
            continue;
        }

        ResourceDescriptor resource_descriptor = ResourceDescriptorFactory::MakeByResourceT(pResource);

        switch (pResource->type)
        {
            case t_sound:
                if (pResource->szFileName[0] == '*' || CL_CheckFile(&msg, resource_descriptor, dlfile_resources))
                    CL_MoveToOnHandList(pResource);
                break;

            case t_skin:
                CL_MoveToOnHandList(pResource);
                break;

            case t_model:
                if (pResource->szFileName[0] == '*' || CL_CheckFile(&msg, resource_descriptor, dlfile_resources))
                    CL_MoveToOnHandList(pResource);
                break;

            case t_decal:
                if (!HPAK_GetDataPointer("custom.hpk", pResource, nullptr, nullptr))
                {
                    if (!FBitSet(pResource->ucFlags, RES_REQUESTED))
                    {
                        Q_snprintf(filename, sizeof(filename), "!MD5%s", COM_BinPrintf(pResource->rgucMD5_hash, sizeof(pResource->rgucMD5_hash)));
                        MSG_WriteByte(&msg, clc_stringcmd);
                        MSG_WriteString(&msg, va("dlfile %s", filename));

                        SetBits(pResource->ucFlags, RES_REQUESTED);
                    }
                    break;
                }

                CL_MoveToOnHandList(pResource);
                break;

            case t_generic:
                if (!IsSafeFileToDownload(pResource->szFileName))
                {
                    CL_RemoveFromResourceList(pResource);
                    Mem_Free(pResource);
                    break;
                }

                if (CL_CheckFile(&msg, resource_descriptor, dlfile_resources))
                    CL_MoveToOnHandList(pResource);
                break;

            case t_eventscript:
                if (CL_CheckFile(&msg, resource_descriptor, dlfile_resources))
                    CL_MoveToOnHandList(pResource);
                break;

            case t_world:
                ASSERT(0);
                break;
        }
    }

    int queue_size = CL_HttpGetDownloadQueueSize();
    if (queue_size > 0)
    {
        Q_strcpy(filename, cl->downloadUrl);
        CL_MarkMapAsUsingHTTPDownload();
        CL_Disconnect();
        StartLoadingProgressBar(filename, queue_size);
        SetLoadingProgressBarStatusText("#GameUI_VerifyingAndDownloading");
    }

    if (cls->state != ca_disconnected)
    {
        if (!msg.cursize && CL_PrecacheResources())
        {
            CL_RegisterResources(&msg);
        }

        if (!cls->passive)
        {
            Netchan_CreateFragments(false, &cls->netchan, &msg);
            Netchan_FragSend(&cls->netchan);
        }
    }
}

void CL_StartResourceDownloading(const char *pszMessage, bool bCustom)
{
    resourceinfo_t ri;

    if (fs_startup_timings->value != 0.0)
        AddStartupTiming("begin CL_StartResourceDownloading()");

    if (pszMessage)
        Con_DPrintf(ConLogType::Info, "%s", pszMessage);

    if (!bCustom && client_stateex.privateResListState == PrivateResListState::Active)
        PrivateRes_AddClientOnlyResources(&cl->resourcesneeded);

    cls->dl.nTotalSize = COM_SizeofResourceList(&cl->resourcesneeded, &ri);
    cls->dl.nTotalToTransfer = CL_EstimateNeededResources();

    if (bCustom)
    {
        cls->dl.custom = 1;
    }
    else
    {
        cls->state = ca_uninitialized;
        cls->dl.custom = 0;
        *gfExtendedError = 0;
    }
    cls->dl.doneregistering = false;
    cls->dl.fLastStatusUpdate = (float)*realtime;
    cls->dl.nRemainingToTransfer = cls->dl.nTotalToTransfer;
    Q_memset(cls->dl.rgStats, 0, sizeof(cls->dl.rgStats));
    cls->dl.nCurStat = 0;

    if (fs_startup_timings->value != 0.0)
        AddStartupTiming("end   CL_StartResourceDownloading()");

    if (!bCustom && client_stateex.privateResListState == PrivateResListState::PrepareToDownloadResList)
    {
        cls->dl.doneregistering = true;

        client_stateex.privateResListState = PrivateResListState::DownloadingResList;
        Con_DPrintf(ConLogType::Info, "privateResListState = DownloadingResList\n");

        PrivateRes_ListRequest();
    }
    else
    {
        CL_BatchResourceRequest();
    }
}

void CL_ClearResourceLists()
{
    cl->downloadUrl[0] = '\0';

    for (resource_t *pResource = cl->resourcesneeded.pNext, * pNext; pResource != &cl->resourcesneeded; pResource = pNext)
    {
        pNext = pResource->pNext;

        CL_RemoveFromResourceList(pResource);
        Mem_Free(pResource);
    }

    cl->resourcesneeded.pNext = &cl->resourcesneeded;
    cl->resourcesneeded.pPrev = &cl->resourcesneeded;

    for (resource_t *pResource = cl->resourcesonhand.pNext, * pNext; pResource != &cl->resourcesonhand; pResource = pNext)
    {
        pNext = pResource->pNext;

        CL_RemoveFromResourceList(pResource);
        Mem_Free(pResource);
    }

    cl->resourcesonhand.pNext = &cl->resourcesonhand;
    cl->resourcesonhand.pPrev = &cl->resourcesonhand;
}

void CL_CreateResourceList()
{
    if (cls->state != ca_dedicated)
    {
        HPAK_FlushHostQueue();

        cl->num_resources = 0;

        char szFileName[MAX_PATH];
        snprintf(szFileName, sizeof(szFileName), "tempdecal.wad");

        FileHandle_t hFile = FS_Open(szFileName, "rb");

        if (FILESYSTEM_INVALID_HANDLE != hFile)
        {
            const int uiSize = FS_Size(hFile);

            unsigned char rgucMD5_hash[16];
            Q_memset(rgucMD5_hash, 0, sizeof(rgucMD5_hash));
            MD5_Hash_File(rgucMD5_hash, szFileName, false, false, nullptr);

            if (uiSize)
            {
                if (cl->num_resources > (MAX_RESOURCE_LIST - 1))
                    Sys_Error("Too many resources on client.");

                resource_t* pResource = &cl->resourcelist[cl->num_resources];

                ++cl->num_resources;

                Q_strncpy(pResource->szFileName, szFileName, sizeof(pResource->szFileName));
                pResource->type = t_decal;
                pResource->nDownloadSize = uiSize;
                pResource->nIndex = 0;
                pResource->ucFlags |= RES_CUSTOM;

                Q_memcpy(pResource->rgucMD5_hash, rgucMD5_hash, sizeof(rgucMD5_hash));

                HPAK_AddLump(false, "custom.hpk", pResource, nullptr, hFile);
            }

            FS_Close(hFile);
        }
    }
}
