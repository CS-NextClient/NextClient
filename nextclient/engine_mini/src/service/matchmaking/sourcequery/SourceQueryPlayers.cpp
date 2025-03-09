#include "SourceQueryPlayers.h"

#include "source_query_constants.h"

using namespace concurrencpp;

SourceQueryPlayers::SourceQueryPlayers(SOCKET socket) :
    socket_(socket)
{ }

result<SQResponseInfo<SQ_PLAYERS>> SourceQueryPlayers::SendPlayersQuery(netadr_t addr)
{
    response_promise_ = std::move(result_promise<SQResponseInfo<SQ_PLAYERS>>());

    Send(addr);

    return response_promise_.get_result();
}

void SourceQueryPlayers::Send(netadr_t addr)
{
    ByteBuffer payload;
    payload << SQ_HEADER_SIMPLE
            << A2S_PLAYER
            << A2S_REQUEST_CHALLENGE;

    sockaddr_in sock_addr = addr.ToSockadr();
    sendto(socket_, (const char*)payload.GetBuffer(), (int)payload.Size(), 0, (const sockaddr*)&sock_addr, sizeof(sock_addr));
}

bool SourceQueryPlayers::TryResolve(netadr_t from_addr, ByteBuffer &buffer, char type, uint32_t ping_ms)
{
    if (type != S2A_PLAYER)
        return false;

    SQ_PLAYERS players;

    uint8_t numplayers = 0;
    buffer >> numplayers;
    for (int i = 0; i < numplayers; i++)
    {
        SQ_PLAYER player;
        buffer >> player.index >> player.player_name >> player.kills >> player.time_connected;
        players.push_back(player);
    }

    response_promise_.set_result(SQResponseInfo<SQ_PLAYERS>(std::move(players), from_addr, ping_ms));

    return true;
}

void SourceQueryPlayers::Resolve(SQErrorCode error_code, netadr_t from_addr)
{
    response_promise_.set_result(SQResponseInfo<SQ_PLAYERS>(error_code, from_addr, 0));
}

void SourceQueryPlayers::ChallengeReceived(netadr_t from_addr, int challenge, uint32_t ping_ms)
{
    ByteBuffer payload;
    payload << SQ_HEADER_SIMPLE
            << A2S_PLAYER
            << challenge;

    sockaddr_in sock_addr = from_addr.ToSockadr();
    sendto(socket_, (const char*)payload.GetBuffer(), (int)payload.Size(), 0, (const sockaddr*)&sock_addr, sizeof(sock_addr));
}
