#pragma once

#include "../engine.h"

void Netchan_Setup(netsrc_t socketnumber, netchan_t* chan, netadr_t adr, int player_slot, void* connection_status,
                   qboolean (*pfnNetchan_Blocksize)(void*));
qboolean Netchan_CopyFileFragments(netchan_t* chan);
qboolean Netchan_CopyNormalFragments(netchan_t *chan);
qboolean Netchan_IncomingReady(netchan_t *chan);
qboolean Netchan_Process(netchan_t *chan);
void Netchan_CreateFragments(bool server, netchan_t *chan, sizebuf_t *msg);
void Netchan_FragSend(netchan_t *chan);
void Netchan_FlushIncoming(netchan_t *chan, int stream);
void Netchan_UpdateProgress(netchan_t *chan);
qboolean NET_GetPacket(netsrc_t sock);
qboolean NET_GetPacket_0();

void NET_SendPacketPost(netsrc_t sock, int length, void *data, netadr_t to, int result);
