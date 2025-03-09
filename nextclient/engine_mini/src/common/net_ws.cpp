#include "../console/console.h"

void NET_ClearLagData(qboolean bClient, qboolean bServer)
{
    eng()->NET_ClearLagData.InvokeChained(bClient, bServer);
}

const char* NET_AdrToString(netadr_t a)
{
    return a.ToString();
}

qboolean NET_CompareAdr(netadr_t a, netadr_t b)
{
    return a == b;
}
