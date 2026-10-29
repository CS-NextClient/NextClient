#include "net_ws.h"

#include <cstring>
#include <type_traits>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <cerrno>
    #define WSAEWOULDBLOCK      EWOULDBLOCK
    #define WSAECONNREFUSED     ECONNREFUSED
    #define WSAECONNRESET       ECONNRESET
    #define WSAEADDRNOTAVAIL    EADDRNOTAVAIL
    #define WSAENOBUFS          ENOBUFS
#endif
#include <ncl_perf/profiler.h>

#include "engine.h"
#include "console/console.h"
#include "common/sys_dll.h"

void NET_ClearLagData(goldsrc::qboolean bClient, goldsrc::qboolean bServer)
{
    eng()->NET_ClearLagData.InvokeChained(bClient, bServer);
}

static_assert(sizeof(goldsrc::netadr_t) == sizeof(::netadr_t));
static_assert(std::is_trivially_copyable_v<goldsrc::netadr_t> && std::is_trivially_copyable_v<::netadr_t>);

::netadr_t NET_ToTier1(const goldsrc::netadr_t& address)
{
    ::netadr_t converted;
    std::memcpy(&converted, &address, sizeof(converted));

    return converted;
}

const char* NET_AdrToString(goldsrc::netadr_t a)
{
    return NET_ToTier1(a).ToString();
}

goldsrc::qboolean NET_LeaveGroup(goldsrc::netsrc_t sock, goldsrc::netadr_t addr)
{
    return eng()->NET_LeaveGroup(sock, addr);
}

goldsrc::qboolean NET_CompareAdr(goldsrc::netadr_t a, goldsrc::netadr_t b)
{
    return NET_ToTier1(a) == NET_ToTier1(b);
}

void NET_SendPacket(goldsrc::netsrc_t sock, int length, void* data, goldsrc::netadr_t to)
{
    NCL_PROFILE_SCOPE();

    const goldsrc::netadrtype_t addr_type = to.type;

    if (addr_type == goldsrc::NA_LOOPBACK)
    {
        eng()->NET_SendLoopPacket(sock, length, data, to);
        return;
    }

    int net_socket;
    if (addr_type == goldsrc::NA_BROADCAST || addr_type == goldsrc::NA_IP)
    {
        net_socket = p_ip_sockets[sock];
    }
    else if (addr_type == goldsrc::NA_IPX || addr_type == goldsrc::NA_BROADCAST_IPX)
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

    #ifdef _WIN32
        int err = WSAGetLastError();
    #else
        int err = errno;
    #endif

    if (err == WSAEWOULDBLOCK || err == WSAECONNREFUSED || err == WSAECONNRESET)
    {
        return;
    }

    // some PPP links dont allow broadcasts
    if (err == WSAEADDRNOTAVAIL && (addr_type == goldsrc::NA_BROADCAST || addr_type == goldsrc::NA_BROADCAST_IPX))
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
