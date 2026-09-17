#include <cstring>
#include <type_traits>

#include <goldsrc/engine/net.h>
#include <goldsrc/types.h>
#include <tier1/netadr.h>

#include "../console/console.h"

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
