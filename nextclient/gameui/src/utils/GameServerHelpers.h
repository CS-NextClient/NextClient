#pragma once

#include <algorithm>
#include <steam/steam_api.h>

inline int GetHumanPlayerCount(const gameserveritem_t& server)
{
    return std::max(0, server.m_nPlayers - server.m_nBotPlayers);
}

inline bool IsServerFull(const gameserveritem_t& server)
{
    return server.m_nPlayers >= server.m_nMaxPlayers;
}
