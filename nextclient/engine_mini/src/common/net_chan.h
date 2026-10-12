#pragma once

#include <goldsrc/engine/net.h>
#include <goldsrc/types.h>

#include "../engine.h"

void Netchan_Setup(
    goldsrc::netsrc_t socketnumber,
    goldsrc::netchan_t* chan,
    goldsrc::netadr_t adr,
    int player_slot,
    void* connection_status,
    goldsrc::qboolean (*pfnNetchan_Blocksize)(void*)
);
goldsrc::qboolean Netchan_CopyFileFragments(goldsrc::netchan_t* chan);
goldsrc::qboolean Netchan_CopyNormalFragments(goldsrc::netchan_t* chan);
goldsrc::qboolean Netchan_IncomingReady(goldsrc::netchan_t* chan);
goldsrc::qboolean Netchan_Process(goldsrc::netchan_t* chan);
void Netchan_CreateFragments(bool server, goldsrc::netchan_t* chan, goldsrc::sizebuf_t* msg);
void Netchan_FragSend(goldsrc::netchan_t* chan);
void Netchan_FlushIncoming(goldsrc::netchan_t* chan, int stream);
void Netchan_Transmit(goldsrc::netchan_t* chan, int length, uint8_t* data);
void Netchan_UpdateProgress(goldsrc::netchan_t* chan);
void Netchan_Clear(goldsrc::netchan_t* chan);
goldsrc::qboolean NET_GetPacket(goldsrc::netsrc_t sock);
goldsrc::qboolean NET_GetPacket_0();
