#include "net_chan.h"
#include <filesystem>
#include <ncl_perf/profiler.h>
#include <bzlib.h>
#include <nitro_utils/string_utils.h>
#include <resource/ResourceDescriptor.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/net.h>
#include <goldsrc/types.h>
#include "zone.h"
#include "sys_dll.h"
#include "../console/console.h"
#include "../client/cl_private_resources.h"
#include "../client/download.h"

void Netchan_Setup(
    goldsrc::netsrc_t socketnumber,
    goldsrc::netchan_t* chan,
    goldsrc::netadr_t adr,
    int player_slot,
    void* connection_status,
    goldsrc::qboolean (*pfnNetchan_Blocksize)(void*)
)
{
    NCL_PROFILE_SCOPE();

    eng()->Netchan_Setup(socketnumber, chan, adr, player_slot, connection_status, pfnNetchan_Blocksize);
}

goldsrc::qboolean Netchan_CopyFileFragments(goldsrc::netchan_t* chan)
{
    NCL_PROFILE_SCOPE();

    goldsrc::fragbuf_t* p;
    int nsize;
    unsigned char *buffer;
    int pos;
    signed int cursize;
    char compressor[32];
    char filename[2048];
    goldsrc::fragbuf_t* n;
    goldsrc::qboolean bCompressed = FALSE;
    unsigned int uncompressedSize;

    if (!chan->incomingready[goldsrc::FRAG_FILE_STREAM])
        return FALSE;

    p = chan->incomingbufs[goldsrc::FRAG_FILE_STREAM];
    if (!p)
    {
        Con_Printf("%s:  Called with no fragments readied\n", __func__);
        chan->incomingready[goldsrc::FRAG_FILE_STREAM] = FALSE;
        return FALSE;
    }

    SZ_Clear(net_message);
    SZ_Write(net_message, (char *)p->frag_message.data, p->frag_message.cursize);

    MSG_BeginReading();
    Q_strncpy(filename, MSG_ReadString(), sizeof(filename));
    Q_strncpy(compressor, MSG_ReadString(), sizeof(compressor));
    if (!Q_stricmp(compressor, "bz2"))
        bCompressed = TRUE;
    uncompressedSize = (unsigned int)MSG_ReadLong();

    V_strcpy_safe(chan->incomingfilename, filename);

    if (V_strlen(filename) <= 0)
    {
        Con_Printf("File fragment received with no filename\nFlushing input queue\n");
        Netchan_FlushIncoming(chan, goldsrc::FRAG_FILE_STREAM);
        return FALSE;
    }

    bool bPrivateResourceList = V_strcmp(filename, client_stateex.privateResListDownloadPath.c_str()) == 0;

    if (bPrivateResourceList)
    {
        if (!IsSafeFileToDownload(filename))
        {
            g_DownloadFileLogger->AddLogFileError(filename, LogFileTypeError::FileBlocked, 0, 0);
            Con_Printf("Private resource list received with bad path, ignoring\n");
            Netchan_FlushIncoming(chan, goldsrc::FRAG_FILE_STREAM);
            return FALSE;
        }
    }
    else if (filename[0] != '!')
    {
        if (cls->state == goldsrc::ca_dedicated || !IsSafeFileToDownload(filename))
        {
            g_DownloadFileLogger->AddLogFileError(filename, LogFileTypeError::FileBlocked, 0, 0);
            Con_Printf(
                cls->state == goldsrc::ca_dedicated ? "Non-customization file fragment received, ignoring\n"
                                           : "File fragment received with bad path, ignoring\n"
            );
            Netchan_FlushIncoming(chan, goldsrc::FRAG_FILE_STREAM);
            return FALSE;
        }

        for (const auto &desc : ResourceDescriptorFactory::MakeByDownloadPath(filename))
        {
            if (!IsSafeFileToDownload(desc.get_save_path()))
            {
                g_DownloadFileLogger->AddLogFileError(desc.get_save_path().c_str(), LogFileTypeError::FileBlocked, 0, 0);
                Con_Printf("File fragment received with bad save path, ignoring\n");
                Netchan_FlushIncoming(chan, goldsrc::FRAG_FILE_STREAM);
                return FALSE;
            }

            if (!desc.NeedToDownload())
            {
                Netchan_FlushIncoming(chan, goldsrc::FRAG_FILE_STREAM);
                return TRUE;
            }
        }
    }
    else
    {
        if (V_strstr(filename, ".."))
        {
            g_DownloadFileLogger->AddLogFileError(filename, LogFileTypeError::FileBlocked, 0, 0);
            Con_Printf("File fragment received with bad path, ignoring\n");
            Netchan_FlushIncoming(chan, goldsrc::FRAG_FILE_STREAM);
            return FALSE;
        }
    }

    nsize = 0;
    while (p)
    {
        nsize += p->frag_message.cursize;
        if (p == chan->incomingbufs[goldsrc::FRAG_FILE_STREAM])
            nsize -= *pMsg_readcount;
        p = p->next;
    }

    buffer = (unsigned char *)Mem_ZeroMalloc(nsize + 1);
    if (!buffer)
    {
        Con_Printf("Buffer allocation failed on %i bytes\n", nsize + 1);
        Netchan_FlushIncoming(chan, goldsrc::FRAG_FILE_STREAM);
        return FALSE;
    }

    p = chan->incomingbufs[goldsrc::FRAG_FILE_STREAM];
    pos = 0;
    while (p)
    {
        n = p->next;

        cursize = p->frag_message.cursize;
        // First message has the file name, don't write that into the data stream, just write the rest of the actual data
        if (p == chan->incomingbufs[goldsrc::FRAG_FILE_STREAM])
        {
            // Copy it in
            cursize -= *pMsg_readcount;
            Q_memcpy(&buffer[pos], &p->frag_message.data[*pMsg_readcount], cursize);
            p->frag_message.cursize = cursize;
        }
        else
        {
            Q_memcpy(&buffer[pos], p->frag_message.data, cursize);
        }
        pos += p->frag_message.cursize;
        Mem_Free(p);
        p = n;
    }

    // FIXED: We have concat fragment buffer above, make sure that the fisrt fragment is null
    // otherwise we will get memory access violation at next call Netchan_FlushIncoming
    chan->incomingbufs[goldsrc::FRAG_FILE_STREAM] = nullptr;
    chan->incomingready[goldsrc::FRAG_FILE_STREAM] = FALSE;

    if (bCompressed)
    {
        unsigned char *uncompressedBuffer = (unsigned char *)Mem_Malloc(uncompressedSize);
        Con_DPrintf(ConLogType::Info, "Decompressing file %s (%d -> %d)\n", filename, nsize, uncompressedSize);
        BZ2_bzBuffToBuffDecompress((char *)uncompressedBuffer, &uncompressedSize, (char *)buffer, nsize, 1, 0);
        Mem_Free(buffer);
        pos = uncompressedSize;
        buffer = uncompressedBuffer;
    }

    if (bPrivateResourceList)
    {
        Con_DPrintf(
            ConLogType::Info, "Private resource list received from location: %s\n", client_stateex.privateResListDownloadPath.c_str()
        );

        if (client_stateex.privateResListState != PrivateResListState::DownloadingResList)
            Con_DPrintf(ConLogType::Info, "Invalid privateResListState state: %d\n", client_stateex.privateResListState);

        client_stateex.privateResListState = PrivateResListState::RerunBatchResources;
        Con_DPrintf(ConLogType::Info, "privateResListState = RerunBatchResources\n");

        PrivateRes_ParseList((const char *)buffer, pos);
    }
    else if (filename[0] == '!')
    {
        if (chan->tempbuffer)
        {
            Con_DPrintf(ConLogType::Info, "Netchan_CopyFragments:  Freeing holdover tempbuffer\n");
            Mem_Free(chan->tempbuffer);
        }
        chan->tempbuffer = buffer;
        chan->tempbuffersize = pos;
    }
    else
    {
        if (pos > 0)
        {
            for (const auto &desc : ResourceDescriptorFactory::MakeByDownloadPath(filename))
            {
                bool is_saved = desc.SaveToFile((const char *)buffer, pos);
                if (!is_saved)
                {
                    g_DownloadFileLogger->AddLogFileError(desc.get_save_path().c_str(), LogFileTypeError::FileSaveError, 0, 0);

                    Con_Printf("Save to file failed %s\n", desc.get_save_path().c_str());
                    Netchan_FlushIncoming(chan, goldsrc::FRAG_FILE_STREAM);

                    Mem_Free(buffer);
                    return FALSE;
                }

                g_DownloadFileLogger->AddLogFile(desc.get_download_path().c_str(), desc.get_download_size(), LogFileType::FileDownloaded);
            }
        }
        else
        {
            g_DownloadFileLogger->AddLogFileError(filename, LogFileTypeError::FileMissing, 0, 0);
        }

        Mem_Free(buffer);
    }

    SZ_Clear(net_message);
    *pMsg_readcount = 0;

    // we do not want to precache private resource list
    if (bPrivateResourceList)
        return FALSE;

    return TRUE;
}

goldsrc::qboolean Netchan_CopyNormalFragments(goldsrc::netchan_t* chan)
{
    NCL_PROFILE_SCOPE();

    return eng()->Netchan_CopyNormalFragments.InvokeChained(chan);
}

goldsrc::qboolean Netchan_Process(goldsrc::netchan_t* chan)
{
    NCL_PROFILE_SCOPE();

    return eng()->Netchan_Process.InvokeChained(chan);
}

goldsrc::qboolean Netchan_IncomingReady(goldsrc::netchan_t* chan)
{
    NCL_PROFILE_SCOPE();

    if (chan->incomingready[0] == false)
        return chan->incomingready[1] != 0;

    return true;
}

void Netchan_CreateFragments(bool server, goldsrc::netchan_t* chan, goldsrc::sizebuf_t* msg)
{
    NCL_PROFILE_SCOPE();

    eng()->Netchan_CreateFragments.InvokeChained(server, chan, msg);
}

void Netchan_FragSend(goldsrc::netchan_t* chan)
{
    NCL_PROFILE_SCOPE();

    eng()->Netchan_FragSend.InvokeChained(chan);
}

void Netchan_FlushIncoming(goldsrc::netchan_t* chan, int stream)
{
    NCL_PROFILE_SCOPE();

    eng()->Netchan_FlushIncoming.InvokeChained(chan, stream);
}

void Netchan_Transmit(goldsrc::netchan_t* chan, int length, uint8_t* data)
{
    NCL_PROFILE_SCOPE();

    eng()->Netchan_Transmit.InvokeChained(chan, length, data);
}

void Netchan_UpdateProgress(goldsrc::netchan_t* chan)
{
    NCL_PROFILE_SCOPE();

    eng()->Netchan_UpdateProgress.InvokeChained(chan);
}

void Netchan_Clear(goldsrc::netchan_t* chan)
{
    NCL_PROFILE_SCOPE();
    
    eng()->Netchan_Clear.InvokeChained(chan);
}

goldsrc::qboolean NET_GetPacket(goldsrc::netsrc_t sock)
{
    NCL_PROFILE_SCOPE();

    return eng()->NET_GetPacket.InvokeChained(sock);
}

goldsrc::qboolean NET_GetPacket_0()
{
    NCL_PROFILE_SCOPE();

    return eng()->NET_GetPacket_0.InvokeChained();
}
