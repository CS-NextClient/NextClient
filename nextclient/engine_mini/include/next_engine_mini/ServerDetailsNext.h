#pragma once

#include <cstddef>

inline constexpr size_t kCountryCodeLength = 2;

// Capacities of the ServerDetailsNext buffers in bytes, terminator included
inline constexpr size_t kGameModeIdSize = 32;
inline constexpr size_t kCountryCodeSize = kCountryCodeLength + 1;
inline constexpr size_t kCountryNameSize = 96;

// NextClient details of a server list entry, complementing the Steam gameserveritem_t; an unknown value is empty.
// docs/master-server-http-protocol.md lists the mode identifiers and describes where the values come from.
struct ServerDetailsNext
{
    char game_mode[kGameModeIdSize]{};

    // ISO 3166-1 alpha-2 code in upper case
    char country_code[kCountryCodeSize]{};

    // UI language, UTF-8
    char country_name[kCountryNameSize]{};
};
