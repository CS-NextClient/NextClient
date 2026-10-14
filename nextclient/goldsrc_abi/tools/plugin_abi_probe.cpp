#include <cstddef>
#include <cstdio>

#include <net_api.h>
#include <in_buttons.h>

template<class T>
void PrintType(const char* name)
{
    std::printf("%s: %s\n", name, __FUNCSIG__);
}

#define FIELD(T, F) \
    std::printf(#T "." #F " offset=%zu size=%zu\n", offsetof(T, F), sizeof(((T*)nullptr)->F)); \
    PrintType<decltype(T::F)>(#T "." #F)

int main()
{
    std::printf("net_api_t size=%zu align=%zu\n", sizeof(net_api_t), alignof(net_api_t));
    std::printf("net_status_t size=%zu align=%zu\n", sizeof(net_status_t), alignof(net_status_t));
    FIELD(net_api_t, InitNetworking);
    FIELD(net_api_t, Status);
    FIELD(net_api_t, SendRequest);
    FIELD(net_api_t, CancelRequest);
    FIELD(net_api_t, CancelAllRequests);
    FIELD(net_api_t, AdrToString);
    FIELD(net_api_t, CompareAdr);
    FIELD(net_api_t, StringToAdr);
    FIELD(net_api_t, ValueForKey);
    FIELD(net_api_t, RemoveKey);
    FIELD(net_api_t, SetValueForKey);
    FIELD(net_status_t, connected);
    FIELD(net_status_t, local_address);
    FIELD(net_status_t, remote_address);
    FIELD(net_status_t, packet_loss);
    FIELD(net_status_t, latency);
    FIELD(net_status_t, connection_time);
    FIELD(net_status_t, rate);
    std::printf("IN_JUMP=%d\n", IN_JUMP);
}
