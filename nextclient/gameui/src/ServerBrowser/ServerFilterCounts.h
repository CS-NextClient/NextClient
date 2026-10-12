#pragma once

#include <array>
#include <iterator>
#include <map>
#include <string>

#include <next_engine_mini/ServerDetailsNext.h>

#include "ServerBrowser/ServerGameModeNames.h"

// Servers behind the items of the game mode and country filters. A server counts for the items of one of the two
// filters when it passes every other filter, so an item holds the number of servers listed once it is picked.
struct ServerFilterCounts
{
    // servers of any game mode, and per entry of kServerGameModeNames
    int all_game_modes{};
    std::array<int, std::size(kServerGameModeNames)> game_modes{};

    // servers of any country, and per country code
    int all_countries{};
    std::map<std::string, int> countries{};
};

// Counts a server that passes the filters other than the game mode and country ones
void ServerFilterCounts_Add(
    const ServerDetailsNext& details,
    bool passes_game_mode_filter,
    bool passes_country_filter,
    ServerFilterCounts* counts
);
