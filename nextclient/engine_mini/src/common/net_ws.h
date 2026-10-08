#pragma once

#include <goldsrc/engine/net.h>
#include <goldsrc/types.h>
#include <tier1/netadr.h>

#define MAX_INIT_MSG		20480	// max length of possible message

void NET_SendPacket(goldsrc::netsrc_t sock, int length, void* data, goldsrc::netadr_t to);
void NET_ClearLagData(goldsrc::qboolean bClient, goldsrc::qboolean bServer);
const char* NET_AdrToString(goldsrc::netadr_t a);
goldsrc::qboolean NET_LeaveGroup(goldsrc::netsrc_t sock, goldsrc::netadr_t addr);
goldsrc::qboolean NET_CompareAdr(goldsrc::netadr_t a, goldsrc::netadr_t b);

// The same address as the tier1 class, whose methods format and compare it.
::netadr_t NET_ToTier1(const goldsrc::netadr_t& address);
