#include "SteamMatchmakingServerListResponseProxy.h"

using namespace service::matchmaking;

SteamMatchmakingServerListResponseProxy::SteamMatchmakingServerListResponseProxy(ISteamMatchmakingServerListResponse* response, HServerListRequest request_id) :
    response_(response),
    request_id_(request_id)
{ }

void SteamMatchmakingServerListResponseProxy::ServerResponded(HServerListRequest hRequest, int iServer)
{
    response_->ServerResponded(request_id_, iServer);
}

void SteamMatchmakingServerListResponseProxy::ServerFailedToRespond(HServerListRequest hRequest, int iServer)
{
    response_->ServerFailedToRespond(request_id_, iServer);
}

void SteamMatchmakingServerListResponseProxy::RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response)
{
    response_->RefreshComplete(request_id_, response);
}
