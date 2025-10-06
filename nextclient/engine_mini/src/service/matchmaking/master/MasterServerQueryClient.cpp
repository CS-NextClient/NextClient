#include "MasterServerQueryClient.h"

#include <algorithm>
#include <easylogging++.h>
#include <format>
#include <optick.h>

#include <winsock2.h>
#include <service/matchmaking/sourcequery/source_query_constants.h>
#include <steam/steam_api.h>
#include <ncl_utils/scope_exit.h>
#include <taskcoro/TaskCoro.h>

using namespace concurrencpp;
using namespace taskcoro;
using namespace std::chrono_literals;

MasterServerQueryClient::MasterServerQueryClient(
    netadr_t master_addr,
    MasterRegionCode region_code,
    int appid,
    const std::string& gamedir,
    std::chrono::milliseconds next_request_delay,
    std::chrono::milliseconds query_delay,
    std::chrono::milliseconds query_timeout
) :
    master_addr_(master_addr.ToSockadr()),
    region_code_(region_code),
    next_request_delay_(next_request_delay),
    query_delay_(query_delay),
    query_timeout_(query_timeout),
    base_filter_ (std::format(R"(\appid\{}\gamedir\{})", appid, gamedir))
{
    
}

result<std::vector<netadr_t>> MasterServerQueryClient::GetServerAddressesAsync(
    std::function<void(const netadr_t&)> address_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    result_promise<void> request_done = co_await BeginSequentialRequest();

    uint32_t request_num = ++request_counter_;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        LOG(ERROR) << "[MasterServerQueryClient] Req: " << request_num << ". Socket failed with error INVALID_SOCKET: " << WSAGetLastError();
        co_return std::vector<netadr_t>{};
    }

    auto exit_guard = ncl_utils::MakeScopeExit([&]
    {
        closesocket(sock);
        next_query_time_ = std::chrono::steady_clock::now() + next_request_delay_;

        LOG(DEBUG) << "[MasterServerQueryClient] Req: " << request_num << ". Completed";
        request_done.set_result();
    });

    try
    {
        LOG(DEBUG) << "[MasterServerQueryClient] Req: " << request_num << ". Start";
        co_return co_await GetServerAddressesInternalAsync(sock, request_num, std::move(address_received_callback), std::move(cancellation_token));
    }
    catch (const OperationCanceledException&)
    {
        LOG(DEBUG) << "[MasterServerQueryClient] Req: " << request_num << ". Cancelled";
        throw;
    }
}

result<std::vector<netadr_t>> MasterServerQueryClient::GetServerAddressesInternalAsync(
    SOCKET sock,
    uint32_t request_num,
    std::function<void(const netadr_t&)> address_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token)
{
    std::vector<netadr_t> result_addresses{};
    netadr_t first_server_address{};

    while (true)
    {
        cancellation_token->ThrowIfCancelled();

        ByteBuffer payload;
        payload << (uint8_t)0x31
                << (uint8_t)region_code_
                << first_server_address.ToString()
                << base_filter_.c_str();

        auto query_delay = std::chrono::duration_cast<std::chrono::milliseconds>(next_query_time_ - std::chrono::steady_clock::now());
        if (query_delay.count() > 0)
        {
            LOG(DEBUG) << "[MasterServerQueryClient] Req: " << request_num << ". Query delay: " << query_delay;
            co_await TaskCoro::WaitForMs(query_delay, cancellation_token);
        }

        LOG(DEBUG) << "[MasterServerQueryClient] Req: " << request_num << ". Sending payload with initial game server ip: " << first_server_address.ToString();
        auto [status, recv_buffer] = co_await async_io::SendAndRecv(sock, query_timeout_, payload, master_addr_, cancellation_token);

        next_query_time_ = std::chrono::steady_clock::now() + query_delay_;

        if (status != async_io::SendAndRecvStatus::Success)
        {
            LOG(DEBUG) << "[MasterServerQueryClient] Req: " << request_num << ". Receive failure status: " << magic_enum::enum_name(status);
            break;
        }

        std::vector<netadr_t> addresses = ParseMasterServerResponse(recv_buffer);
        if (addresses.empty())
        {
            LOG(DEBUG) << "[MasterServerQueryClient] Req: " << request_num << ". Can't parse address list";
            break;
        }

        const netadr_t last_address = addresses.back();

        // Some master servers do not return an end-of-list marker,
        // and any requests are answered with the same list,
        // thus causing duplicate servers and endless querying.
        // This prevents this behavior.
        if (last_address != netadr_t{} && last_address == first_server_address)
        {
            LOG(DEBUG) << "[MasterServerQueryClient] Req: " << request_num << ". Bad master server with infinite querying detected";
            break;
        }

        std::ranges::move(addresses, std::back_inserter(result_addresses));
        if (last_address == netadr_t{})
        {
            // cut 0.0.0.0 from the server list
            addresses.erase(addresses.end() - 1);
        }

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

result<result_promise<void>> MasterServerQueryClient::BeginSequentialRequest()
{
    result<void> prev_tail;
    result_promise<void> done_promise;

    {
        std::scoped_lock lock(requests_mutex_);

        prev_tail = std::move(tail_result_);
        done_promise = concurrencpp::result_promise<void>();

        tail_result_ = done_promise.get_result();
    }

    try
    {
        co_await prev_tail;
    }
    catch (...)
    {
    }

    co_return std::move(done_promise);
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
