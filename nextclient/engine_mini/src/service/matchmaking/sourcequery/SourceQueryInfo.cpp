#include "SourceQueryInfo.h"

#include "source_query_constants.h"

using namespace concurrencpp;

SourceQueryInfo::SourceQueryInfo(SOCKET socket) :
    socket_(socket)
{ }

result<SQResponseInfo<SQ_INFO>> SourceQueryInfo::SendInfoQuery(netadr_t addr)
{
    response_promise_ = std::move(result_promise<SQResponseInfo<SQ_INFO>>());

    Send(addr);

    return response_promise_.get_result();
}

void SourceQueryInfo::Send(netadr_t addr)
{
    ByteBuffer payload;
    payload << SQ_HEADER_SIMPLE
            << A2S_INFO
            << A2S_INFO_PAYLOAD;

    sockaddr_in sock_addr = addr.ToSockadr();
    sendto(socket_, (const char*)payload.GetBuffer(), (int)payload.Size(), 0, (const sockaddr*)&sock_addr, sizeof(sock_addr));
}

bool SourceQueryInfo::TryResolve(netadr_t from_addr, ByteBuffer &buffer, char type, uint32_t ping_ms)
{
    if (type == S2A_INFO)
    {
        response_promise_.set_result(SQResponseInfo<SQ_INFO>(ParseInfo(buffer), from_addr, ping_ms));
        return true;
    }

    if (type == S2A_RULES_GS)
    {
        response_promise_.set_result(SQResponseInfo<SQ_INFO>(ParseRulesGs(buffer), from_addr, ping_ms));
        return true;
    }

    return false;
}

void SourceQueryInfo::Resolve(SQErrorCode error_code, netadr_t from_addr)
{
    response_promise_.set_result(SQResponseInfo<SQ_INFO>(error_code, from_addr, 0));
}

SQ_INFO SourceQueryInfo::ParseInfo(ByteBuffer &buffer)
{
    SQ_INFO info;

    uint8_t edf = 0;
    buffer >> info.version
           >> info.hostname
           >> info.map
           >> info.game_directory
           >> info.game_description
           >> info.app_id
           >> info.num_players
           >> info.max_players
           >> info.num_of_bots
           >> info.type
           >> info.os
           >> info.password
           >> info.secure
           >> info.game_version
           >> edf;

    if (edf == 0)
        return info;

    if (edf & 0x80)
        buffer >> info.port;

    if (edf & 0x10)
        buffer >> info.steamid;

    if (edf & 0x40)
        buffer >> info.tvport >> info.tvname;

    if (edf & 0x20)
        buffer >> info.tags;

    if (edf & 0x01)
        buffer >> info.gameid;

    return info;
}

SQ_INFO SourceQueryInfo::ParseRulesGs(ByteBuffer &buffer)
{
    SQ_INFO info;

    char protocol, mod;

    buffer >> info.address
           >> info.hostname
           >> info.map
           >> info.game_directory
           >> info.game_description
           >> info.num_players
           >> info.max_players
           >> protocol
           >> info.type
           >> info.os
           >> info.password
           >> mod;

    if (mod == 1)
    {
        std::string website, download;
        char nullbyte, type, dll;
        int version, size;

        buffer >> website
               >> download
               >> nullbyte
               >> version
               >> size
               >> type
               >> dll;
    }

    buffer >> info.secure
           >> info.num_of_bots;

    return info;
}

void SourceQueryInfo::ChallengeReceived(netadr_t from_addr, int challenge, uint32_t ping_ms)
{
    ByteBuffer payload;
    payload << SQ_HEADER_SIMPLE
            << A2S_INFO
            << A2S_INFO_PAYLOAD
            << challenge;

    sockaddr_in sock_addr = from_addr.ToSockadr();
    sendto(socket_, (const char*)payload.GetBuffer(), (int)payload.Size(), 0, (const sockaddr*)&sock_addr, sizeof(sock_addr));
}
