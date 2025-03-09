#include "SourceQueryRules.h"

#include "source_query_constants.h"

using namespace concurrencpp;

SourceQueryRules::SourceQueryRules(SOCKET socket) :
    socket_(socket)
{ }

result<SQResponseInfo<SQ_RULES>> SourceQueryRules::SendRulesQuery(netadr_t addr)
{
    response_promise_ = std::move(result_promise<SQResponseInfo<SQ_RULES>>());

    Send(addr);

    return response_promise_.get_result();
}

void SourceQueryRules::Send(netadr_t addr)
{
    ByteBuffer payload;
    payload << SQ_HEADER_SIMPLE
            << A2S_RULES
            << A2S_REQUEST_CHALLENGE;

    sockaddr_in sock_addr = addr.ToSockadr();
    sendto(socket_, (const char*)payload.GetBuffer(), (int)payload.Size(), 0, (const sockaddr*)&sock_addr, sizeof(sock_addr));
}

bool SourceQueryRules::TryResolve(netadr_t from_addr, ByteBuffer &buffer, char type, uint32_t ping_ms)
{
    if (type != S2A_RULES)
        return false;

    SQ_RULES rules;

    uint16_t numrules = 0;
    buffer >> numrules;
    for (int i = 0; i < numrules; i++)
    {
        SQ_RULE rule;
        buffer >> rule.name >> rule.value;
        rules.push_back(rule);
    }

    response_promise_.set_result(SQResponseInfo<SQ_RULES>(std::move(rules), from_addr, ping_ms));

    return true;
}

void SourceQueryRules::Resolve(SQErrorCode error_code, netadr_t from_addr)
{
    response_promise_.set_result(SQResponseInfo<SQ_RULES>(error_code, from_addr, 0));
}

void SourceQueryRules::ChallengeReceived(netadr_t from_addr, int challenge, uint32_t ping_ms)
{
    ByteBuffer payload;
    payload << SQ_HEADER_SIMPLE
            << A2S_RULES
            << challenge;

    sockaddr_in sock_addr = from_addr.ToSockadr();
    sendto(socket_, (const char*)payload.GetBuffer(), payload.Size(), 0, (const sockaddr*)&sock_addr, sizeof(sock_addr));
}
