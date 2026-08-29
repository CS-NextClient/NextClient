#include "net_ws.h"

#include <winsock2.h>
#include <optick.h>

#include "console/console.h"
#include "sys_dll.h"

void NET_ClearLagData(qboolean bClient, qboolean bServer)
{
    eng()->NET_ClearLagData.InvokeChained(bClient, bServer);
}

const char* NET_AdrToString(netadr_t a)
{
    return a.ToString();
}

qboolean NET_LeaveGroup(netsrc_t sock, netadr_t addr)
{
    return eng()->NET_LeaveGroup(sock, addr);
}

qboolean NET_CompareAdr(netadr_t a, netadr_t b)
{
    return a == b;
}

void NET_SendPacket(netsrc_t sock, int length, void* data, netadr_t to)
{
    OPTICK_EVENT();

    const netadrtype_t addr_type = to.GetType();

    if (addr_type == NA_LOOPBACK)
    {
        eng()->NET_SendLoopPacket(sock, length, data, to);
        return;
    }

    int net_socket;
    if (addr_type == NA_BROADCAST || addr_type == NA_IP)
    {
        net_socket = p_ip_sockets[sock];
    }
    else if (addr_type == NA_IPX || addr_type == NA_BROADCAST_IPX)
    {
        net_socket = p_ipx_sockets[sock];
    }
    else
    {
        Sys_Error("NET_SendPacket: bad address type");
    }

    if (!net_socket)
    {
        return;
    }

    sockaddr addr;
    eng()->NET_AdrToSockadr(&to, &addr);

    int ret = eng()->NET_SendLong(sock, net_socket, reinterpret_cast<const char*>(data), length, 0, &addr, sizeof(addr));
    if (ret != -1)
    {
        return;
    }

    int err = WSAGetLastError();

    if (err == WSAEWOULDBLOCK || err == WSAECONNREFUSED || err == WSAECONNRESET)
    {
        return;
    }

    // some PPP links dont allow broadcasts
    if (err == WSAEADDRNOTAVAIL && (addr_type == NA_BROADCAST || addr_type == NA_BROADCAST_IPX))
    {
        return;
    }

    if (err == WSAEADDRNOTAVAIL || err == WSAENOBUFS)
    {
        Con_DPrintf(ConLogType::Warning, "NET_SendPacket Warning: %s : %s\n", eng()->NET_ErrorString(err), NET_AdrToString(to));
    }
    else
    {
        Con_Printf("NET_SendPacket ERROR: %s\n", eng()->NET_ErrorString(err));
    }
}
