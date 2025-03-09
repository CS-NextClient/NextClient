#include "MasterServerQueryClient.h"

#include <algorithm>
#include <easylogging++.h>
#include <format>
#include <optick.h>

#include <winsock2.h>
#include <service/matchmaking/sourcequery/source_query_constants.h>
#include <steam/steam_api.h>
#include <taskcoro/TaskCoro.h>

using namespace concurrencpp;
using namespace taskcoro;

MasterServerQueryClient::MasterServerQueryClient(netadr_t master_addr, MasterRegionCode region_code, int appid, const std::string& gamedir) :
    master_addr_(master_addr),
    region_code_(region_code)
{
    base_filter_ = std::format(R"(\appid\{}\gamedir\{})", appid, gamedir);
}

result<std::vector<netadr_t>> MasterServerQueryClient::GetServerAddressesAsync(
    std::function<void(const netadr_t&)> address_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        LOG(ERROR) << "[MasterServerQueryClient] socket failed with error INVALID_SOCKET: " << WSAGetLastError();
        co_return std::vector<netadr_t>{};
    }

    std::vector<netadr_t> result_addresses;
    try
    {
        result_addresses = co_await GetServerAddressesAsyncInternal(sock, address_received_callback, cancellation_token);
    }
    catch (OperationCanceledException&)
    { }

    closesocket(sock);

    co_return result_addresses;
}

result<std::vector<netadr_t>> MasterServerQueryClient::GetServerAddressesAsyncInternal(
    SOCKET sock,
    std::function<void(const netadr_t&)> address_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    std::vector<netadr_t> result_addresses;
    netadr_t first_server_address{};

    while (true)
    {
        cancellation_token->ThrowIfCancelled();

        ByteBuffer payload;
        payload << (uint8_t)0x31
                << (uint8_t)region_code_
                << first_server_address.ToString()
                << base_filter_.c_str();

        ByteBuffer data = co_await async_io::SendAndRecv(sock, timeval{5, 0}, payload, master_addr_.ToSockadr());
        cancellation_token->ThrowIfCancelled();

        std::vector<netadr_t> addresses = ParseMasterServerResponse(data);

        if (addresses.empty())
        {
            break;
        }

        const netadr_t last_address = addresses.back();

        // Some master servers do not return an end-of-list marker,
        // and any requests are answered with the same list,
        // thus causing duplicate servers and endless querying.
        // This prevents this behavior.
        if (last_address != netadr_t{} && last_address == first_server_address)
        {
            break;
        }

        std::ranges::move(addresses, std::back_inserter(result_addresses));

        if (address_received_callback)
        {
            for (auto& address : addresses)
            {
                address_received_callback(address);
            }
        }

        if (last_address == netadr_t{})
        {
            break;
        }

        first_server_address = last_address;
    }

    co_return result_addresses;
}

std::vector<netadr_t> MasterServerQueryClient::ParseMasterServerResponse(ByteBuffer& buffer)
{
    OPTICK_EVENT();

    std::vector<netadr_t> addresses;

    int32_t packet_type{};
    buffer >> packet_type;

    int16_t ms_header{};
    buffer >> ms_header;

    if (packet_type != SQ_HEADER_SIMPLE || ms_header != 0x0A66)
    {
        return addresses;
    }

    while (buffer.IsValid() && buffer.Tell() < buffer.Size())
    {
        uint32_t ip{};
        uint16_t port{};

        buffer >> ip;
        buffer >> port;

        addresses.emplace_back(ntohl(ip), ntohs(port));
    }

    return addresses;
}
