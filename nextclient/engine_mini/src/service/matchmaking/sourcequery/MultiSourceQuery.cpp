#include "MultiSourceQuery.h"

#include <easylogging++.h>
#include <optick.h>
#include <ranges>

#include <taskcoro/TaskCoro.h>

#include "SourceQueryInfo.h"
#include "SourceQueryRules.h"
#include "SourceQueryPlayers.h"
#include "source_query_constants.h"

using namespace std::chrono;
using namespace concurrencpp;
using namespace taskcoro;

MultiSourceQuery::MultiSourceQuery(int32_t timeout, uint8_t retries) :
    cancellation_token_(CancellationToken::Create()),
    timeout_ms_(timeout),
    retries_(retries)
{
    manual_sync_ctx_ = std::make_shared<SynchronizationContextManual>();
    sync_ctx_ = std::make_shared<SynchronizationContext>(manual_sync_ctx_);

    TaskCoro::RunInNewThread([this]
    {
        OPTICK_THREAD("MultiSourceQuery");
        MainLoop();
    });
}

MultiSourceQuery::~MultiSourceQuery()
{
    cancellation_token_->SetCanceled();

    while (!main_loop_done_)
    {
        std::this_thread::yield();
    }
}

result<SQResponseInfo<SQ_INFO>> MultiSourceQuery::GetInfoAsync(netadr_t address)
{
    std::shared_ptr<SynchronizationContext> caller_ctx = SynchronizationContext::Current();

    co_await sync_ctx_->SwitchTo();

    CreateSocketIfNeeded(address.GetType() == NA_BROADCAST);

    auto query = std::make_unique<SourceQueryInfo>(sockets_.back().socket);
    auto response = query->SendInfoQuery(address);
    queries_[address].emplace_back(std::move(query), high_resolution_clock::now());

    SQResponseInfo<SQ_INFO> result = co_await response;

    if (caller_ctx)
    {
        co_await caller_ctx->SwitchTo();
    }

    co_return result;
}

result<SQResponseInfo<SQ_RULES>> MultiSourceQuery::GetRulesAsync(netadr_t address)
{
    std::shared_ptr<SynchronizationContext> caller_ctx = SynchronizationContext::Current();

    co_await sync_ctx_->SwitchTo();

    CreateSocketIfNeeded(address.GetType() == NA_BROADCAST);

    auto query = std::make_unique<SourceQueryRules>(sockets_.back().socket);
    auto response = query->SendRulesQuery(address);
    queries_[address].emplace_back(std::move(query), high_resolution_clock::now());

    SQResponseInfo<SQ_RULES> result = co_await response;

    if (caller_ctx)
    {
        co_await caller_ctx->SwitchTo();
    }

    co_return result;
}

result<SQResponseInfo<SQ_PLAYERS>> MultiSourceQuery::GetPlayersAsync(netadr_t address)
{
    std::shared_ptr<SynchronizationContext> caller_ctx = SynchronizationContext::Current();

    co_await sync_ctx_->SwitchTo();

    CreateSocketIfNeeded(address.GetType() == NA_BROADCAST);

    auto query = std::make_unique<SourceQueryPlayers>(sockets_.back().socket);
    auto response = query->SendPlayersQuery(address);
    queries_[address].emplace_back(std::move(query), high_resolution_clock::now());

    SQResponseInfo<SQ_PLAYERS> result = co_await response;

    if (caller_ctx)
    {
        co_await caller_ctx->SwitchTo();
    }

    co_return result;
}

result<void> MultiSourceQuery::SwitchToNewSocket(bool broadcast)
{
    std::shared_ptr<SynchronizationContext> caller_ctx = SynchronizationContext::Current();
    co_await sync_ctx_->SwitchTo();

    CreateSocket(broadcast);

    if (caller_ctx)
    {
        co_await caller_ctx->SwitchTo();
    }
}

void MultiSourceQuery::MainLoop()
{
    while (!cancellation_token_->IsCanceled())
    {
        OPTICK_EVENT("MainLoop")

        auto current_time = high_resolution_clock::now();

        ReceiveAndAssembleBuffers();
        ProcessTimeoutQueries(current_time);
        ProcessIncomingBuffers(current_time);
        CloseExpiredSockets(current_time);

        {
            OPTICK_EVENT("Update")
            manual_sync_ctx_->Update();
        }

        // TODO make conditional variable
        std::this_thread::sleep_for(5ms);
    }

    main_loop_done_ = true;
}

void MultiSourceQuery::AssembleBuffer(ByteBuffer& buffer, netadr_t from_addr)
{
    OPTICK_EVENT()

    auto current_time = high_resolution_clock::now();

    int32_t packet_type{};
    buffer >> packet_type;

    if (packet_type == SQ_HEADER_SIMPLE)
    {
        buffer.Seek(0);
        recv_[from_addr].emplace_back(buffer, current_time);
    }
    else if (packet_type == SQ_HEADER_MULTI)
    {
        int32_t requestid{};
        uint8_t numpacket{}, numpackets{};

        buffer >> requestid >> numpacket;
        numpackets = numpacket & 0xF;
        numpacket >>= 0x4;

        auto it = std::find_if(
            recv_[from_addr].begin(),
            recv_[from_addr].end(),
            [requestid](const RecvData& item) { return item.requestid == requestid; }
        );
        bool create_new = it == recv_[from_addr].end();

        RecvData& multi_info = create_new ? recv_[from_addr].emplace_back(current_time) : *it;

        if (create_new)
        {
            multi_info.requestid = requestid;
            multi_info.numpackets = numpackets;
        }

        if (multi_info.recv_done)
        {
            LOG(DEBUG) << "[MultiSourceQuery] Multi-packet was done parsed, but new message with same requestid received";
            return;
        }

        if (multi_info.numpackets != numpackets)
        {
            multi_info.has_error = true;
            multi_info.recv_done = true;
            LOG(DEBUG) << "[MultiSourceQuery] Error while parsing multi-packet, next packet has different numpackets then first "
                          "(first: " <<  multi_info.numpackets << ", current: " << numpackets << ")";
            return;
        }

        auto avail = (size_t)(buffer.Size() - buffer.Tell());
        multi_info.packets[numpacket].Resize(avail);
        buffer.Read(multi_info.packets[numpacket].GetBuffer(), avail);
        multi_info.received++;

        if (multi_info.received >= numpackets)
        {
            multi_info.recv_buffer.Clear();

            for (uint8_t i = 0; i < numpackets; ++i)
                multi_info.recv_buffer.Write(multi_info.packets[i].GetBuffer(), (size_t)multi_info.packets[i].Size());

            multi_info.recv_buffer.Seek(0);
            multi_info.has_error = false;
            multi_info.recv_done = true;
        }
    }
}

void MultiSourceQuery::ProcessIncomingBuffers(high_resolution_clock::time_point current_time)
{
    OPTICK_EVENT()

    for (auto& rcv : recv_)
    {
        netadr_t address = rcv.first;
        std::vector<RecvData>& buffers = rcv.second;

        for (auto it = buffers.begin(); it != buffers.end(); )
        {
            if (it->has_error)
            {
                LOG(DEBUG) << "[MultiSourceQuery] Dropping a buffer with error. addr: " << address.ToString();
                it = buffers.erase(it);
                continue;
            }

            if (GetElapsedMs(current_time, it->first_packet_time) >= kMaxBufferHoldsTimeMs)
            {
                LOG(DEBUG) << "[MultiSourceQuery] Dropping a buffer by timeout. addr: " << address.ToString();
                it = buffers.erase(it);
                continue;
            }

            if (it->recv_done)
            {
                ProcessQueries(address, it->recv_buffer);
                it = buffers.erase(it);
                continue;
            }

            it++;
        }
    }
}

void MultiSourceQuery::ProcessQueries(netadr_t address, ByteBuffer& buffer)
{
    OPTICK_EVENT()

    netadr_t query_address = address;
    if (!queries_.contains(query_address))
    {
        query_address.SetType(NA_BROADCAST);

        if (!queries_.contains(query_address))
            return;
    }

    auto& addr_queries = queries_[query_address];
    auto current_time = high_resolution_clock::now();

    int32_t code{};
    char type{};
    buffer >> code >> type;

    if (type == S2C_CHALLENGE)
    {
        int32_t challenge{};
        buffer >> challenge;

        for (auto& data : addr_queries)
        {
            data.query->ChallengeReceived(address, challenge, GetElapsedMs(current_time, data.send_time));
            break; // TODO Remember the reason for break. Most likely, the loop is not needed.
        }
    }
    else
    {
        for (auto it = addr_queries.begin(); it != addr_queries.end(); it++)
        {
            if (it->query->TryResolve(address, buffer, type, GetElapsedMs(current_time, it->send_time)))
            {
                addr_queries.erase(it);
                break;
            }
        }
    }

    if (queries_[query_address].empty())
        queries_.erase(query_address);
}

void MultiSourceQuery::ProcessTimeoutQueries(high_resolution_clock::time_point current_time)
{
    OPTICK_EVENT()

    for (auto& q : queries_)
    {
        netadr_t address = q.first;
        auto& addr_query = q.second;

        for (auto it = addr_query.begin(); it != addr_query.end(); )
        {
            uint32_t ellapsed = GetElapsedMs(current_time, it->send_time);
            if (ellapsed > timeout_ms_)
            {
                if (it->retry >= retries_)
                {
                    LOG(DEBUG) << "[MultiSourceQuery] Resolving a request by timeout. ms: " << ellapsed << "; addr: " << address.ToString();
                    it->query->Resolve(SQErrorCode::Timeout, address);
                    it = addr_query.erase(it);
                    continue;
                }
                else
                {
                    it->query->Send(address);
                    //it->send_time = current_time;
                    it->retry++;
                }
            }

            it++;
        }
    }
}

void MultiSourceQuery::ReceiveAndAssembleBuffers()
{
    OPTICK_EVENT()

    int cycle_guard = 1000;
    int total_bytes_received;
    int sockets_count = std::min((int)sockets_.size(), FD_SETSIZE);

    if (sockets_count == 0)
        return;

    do
    {
        fd_set set;
        FD_ZERO(&set);
        for (int i = 0; i < sockets_count; i++)
            FD_SET(sockets_[i].socket, &set);

        timeval timeout {0, 0};

        int ready_sockets = select(0, &set, nullptr, nullptr, &timeout);
        if (ready_sockets == 0 || ready_sockets == SOCKET_ERROR)
        {
            if (ready_sockets == SOCKET_ERROR)
                LOG(DEBUG) << "[MultiSourceQuery] select completed with SOCKET_ERROR: " << WSAGetLastError();
            return;
        }

        total_bytes_received = 0;

        for (int i = 0; i < sockets_count && ready_sockets > 0; i++)
        {
            SOCKET socket = sockets_[i].socket;

            if (!FD_ISSET(socket, &set))
                continue;

            sockaddr from_addr {};
            int from_len = sizeof(from_addr);

            u_long pending = 0;
            bool pending_ok = ioctlsocket(socket, FIONREAD, &pending) == 0;
            size_t recv_capacity = 65535;

            if (pending_ok && pending > 0)
            {
                recv_capacity = (size_t)std::min<u_long>(pending, 65535);
            }

            ByteBuffer recv_buffer(recv_capacity);
            recv_buffer.Seek(0);

            int bytes_received = recvfrom(socket, (char*)recv_buffer.GetBuffer(), (int)recv_buffer.Size(), 0, &from_addr, &from_len);
            if (bytes_received == SOCKET_ERROR)
            {
                LOG(DEBUG) << "[MultiSourceQuery] recvfrom completed with error: " << WSAGetLastError();
                break;
            }

            if (bytes_received > 0)
            {
                netadr_t netadr{};
                netadr.SetFromSockadr(&from_addr);

                recv_buffer.Resize(bytes_received);
                AssembleBuffer(recv_buffer, netadr);
            }

            total_bytes_received += bytes_received;
            ready_sockets--;
        }
    } while (total_bytes_received > 0 && cycle_guard-- > 0);
}

void MultiSourceQuery::CloseExpiredSockets(high_resolution_clock::time_point current_time)
{
    OPTICK_EVENT()

    for (auto it = sockets_.begin(); it != sockets_.end(); )
    {
        if (current_time > it->close_time)
        {
            closesocket(it->socket);
            it = sockets_.erase(it);
            continue;
        }

        it++;
    }
}

void MultiSourceQuery::CreateSocket(bool broadcast)
{
    OPTICK_EVENT()

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (broadcast)
    {
        int opt_broadcast = 1;
        setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&opt_broadcast, sizeof(opt_broadcast));
    }

    sockets_.emplace_back(s, high_resolution_clock::now() + milliseconds(kSocketLifeTimeMs));
}

void MultiSourceQuery::CreateSocketIfNeeded(bool broadcast)
{
    OPTICK_EVENT()

    if (!sockets_.empty())
    {
        int opt_broadcast;
        int opt_broadcast_len = sizeof(opt_broadcast);

        SOCKET s = sockets_.back().socket;
        if (getsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&opt_broadcast, &opt_broadcast_len) != SOCKET_ERROR)
        {
            if ((bool)opt_broadcast == broadcast)
                return;
        }
    }

    CreateSocket(broadcast);
}
