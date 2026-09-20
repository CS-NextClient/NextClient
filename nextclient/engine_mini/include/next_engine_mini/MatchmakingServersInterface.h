#pragma once

#include <steam/steam_api.h>

#include "ServerDetailsNext.h"

class MatchmakingServersInterface : public ISteamMatchmakingServers
{
public:
    // Returns false and leaves out zeroed when the request or the server index is unknown
    virtual bool GetServerDetailsNext(HServerListRequest request_id, int server_index, ServerDetailsNext* out) = 0;
};
