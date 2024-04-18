#include "../console/console.h"

void NET_ClearLagData(qboolean bClient, qboolean bServer)
{
    eng()->NET_ClearLagData.InvokeChained(bClient, bServer);
}

const char* NET_AdrToString(netadr_t a)
{
    if (a.type == NA_LOOPBACK)
        return "loopback";

    return va("%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
}

qboolean NET_CompareAdr(netadr_t a, netadr_t b)
{
    if (a.type != b.type)
        return false;

    if (a.type == NA_LOOPBACK)
        return true;

    if (a.type == NA_IP)
    {
        if (!Q_memcmp(a.ip, b.ip, 4) && a.port == b.port)
            return true;

        return false;
    }

    Con_DPrintf(ConLogType::Info, "NET_CompareAdr: bad address type\n");
    return false;
}
