#pragma once

#define MAX_INIT_MSG		20480	// max length of possible message

void NET_ClearLagData(qboolean bClient, qboolean bServer);
const char *NET_AdrToString(netadr_t a);
qboolean NET_CompareAdr(netadr_t a, netadr_t b);
