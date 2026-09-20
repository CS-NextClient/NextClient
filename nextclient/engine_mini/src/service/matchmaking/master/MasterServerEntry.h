#pragma once

#include <string>
#include <string_view>

#include <tier1/netadr.h>

// What the master server reported for a server: a value it did not send, or sent in another form, is empty; the
// country code is upper-cased
struct MasterDetails
{
    std::string game_mode{};
    std::string country_code{};
};

struct MasterServerEntry
{
    netadr_t address{};
    MasterDetails details{};
};

bool MasterDetails_IsGameModeId(std::string_view text);
MasterDetails MasterDetails_Parse(std::string_view game_mode, std::string_view country);
