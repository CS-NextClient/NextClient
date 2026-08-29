#pragma once

#include "engine.h"

#define MAX_INIT_MSG		20480	// max length of possible message

void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to);
void NET_ClearLagData(qboolean bClient, qboolean bServer);
const char *NET_AdrToString(netadr_t a);
qboolean NET_LeaveGroup(netsrc_t sock, netadr_t addr);
qboolean NET_CompareAdr(netadr_t a, netadr_t b);
