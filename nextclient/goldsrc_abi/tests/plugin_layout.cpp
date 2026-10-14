#include <cstddef>
#include <type_traits>

#include <goldsrc/engine/net.h>
#include <goldsrc/usercmd.h>

static_assert(sizeof(goldsrc::net_status_t) == 72);
static_assert(alignof(goldsrc::net_status_t) == 8);
static_assert(offsetof(goldsrc::net_status_t, connected) == 0);
static_assert(offsetof(goldsrc::net_status_t, local_address) == 4);
static_assert(offsetof(goldsrc::net_status_t, remote_address) == 24);
static_assert(offsetof(goldsrc::net_status_t, packet_loss) == 44);
static_assert(offsetof(goldsrc::net_status_t, latency) == 48);
static_assert(offsetof(goldsrc::net_status_t, connection_time) == 56);
static_assert(offsetof(goldsrc::net_status_t, rate) == 64);
static_assert(std::is_same_v<decltype(goldsrc::net_status_t::connected), int>);
static_assert(std::is_same_v<decltype(goldsrc::net_status_t::local_address), goldsrc::netadr_t>);
static_assert(std::is_same_v<decltype(goldsrc::net_status_t::remote_address), goldsrc::netadr_t>);
static_assert(std::is_same_v<decltype(goldsrc::net_status_t::packet_loss), int>);
static_assert(std::is_same_v<decltype(goldsrc::net_status_t::latency), double>);
static_assert(std::is_same_v<decltype(goldsrc::net_status_t::connection_time), double>);
static_assert(std::is_same_v<decltype(goldsrc::net_status_t::rate), double>);

static_assert(sizeof(goldsrc::net_api_t) == 44);
static_assert(alignof(goldsrc::net_api_t) == 4);
static_assert(offsetof(goldsrc::net_api_t, InitNetworking) == 0);
static_assert(offsetof(goldsrc::net_api_t, Status) == 4);
static_assert(offsetof(goldsrc::net_api_t, SendRequest) == 8);
static_assert(offsetof(goldsrc::net_api_t, CancelRequest) == 12);
static_assert(offsetof(goldsrc::net_api_t, CancelAllRequests) == 16);
static_assert(offsetof(goldsrc::net_api_t, AdrToString) == 20);
static_assert(offsetof(goldsrc::net_api_t, CompareAdr) == 24);
static_assert(offsetof(goldsrc::net_api_t, StringToAdr) == 28);
static_assert(offsetof(goldsrc::net_api_t, ValueForKey) == 32);
static_assert(offsetof(goldsrc::net_api_t, RemoveKey) == 36);
static_assert(offsetof(goldsrc::net_api_t, SetValueForKey) == 40);
static_assert(std::is_same_v<decltype(goldsrc::net_api_t::Status), void (__cdecl *)(goldsrc::net_status_t*)>);
static_assert(std::is_same_v<decltype(goldsrc::net_api_t::AdrToString), char* (__cdecl *)(goldsrc::netadr_t*)>);
static_assert(goldsrc::IN_JUMP == 2);
