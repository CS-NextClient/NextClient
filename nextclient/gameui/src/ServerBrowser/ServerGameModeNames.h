#pragma once

#include <cstddef>
#include <iterator>

#include <strtools.h>

struct ServerGameModeName
{
    // identifier the master server sends, and the localization token of its display name
    const char* name{};
    const char* token{};
};

// Modes this build has display names for; docs/master-server-http-protocol.md lists them for the backend
inline constexpr ServerGameModeName kServerGameModeNames[] = {
    {"Public", "#ServerBrowser_GameMode_Public"},
    {"Deathmatch", "#ServerBrowser_GameMode_Deathmatch"},
    {"GunGame", "#ServerBrowser_GameMode_GunGame"},
    {"Zombie", "#ServerBrowser_GameMode_Zombie"},
    {"Deathrun", "#ServerBrowser_GameMode_Deathrun"},
    {"Surf", "#ServerBrowser_GameMode_Surf"},
    {"JailBreak", "#ServerBrowser_GameMode_JailBreak"},
    {"Knife", "#ServerBrowser_GameMode_Knife"},
    {"Awp", "#ServerBrowser_GameMode_Awp"},
    {"HideAndSeek", "#ServerBrowser_GameMode_HideAndSeek"},
    {"Kreedz", "#ServerBrowser_GameMode_Kreedz"},
    {"Aim", "#ServerBrowser_GameMode_Aim"},
    {"Warcraft", "#ServerBrowser_GameMode_Warcraft"},
    {"BaseBuilder", "#ServerBrowser_GameMode_BaseBuilder"},
    {"Mix", "#ServerBrowser_GameMode_Mix"},
    {"Special", "#ServerBrowser_GameMode_Special"},
};

// Index of the mode in kServerGameModeNames, matched in any letter case; -1 for a mode this build has no name for
inline int ServerGameMode_FindIndex(const char* mode)
{
    for (size_t i = 0; i < std::size(kServerGameModeNames); i++)
    {
        if (!V_stricmp(kServerGameModeNames[i].name, mode))
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

// Localization token of the mode's display name; nullptr for a mode this build has no name for
inline const char* ServerGameMode_GetNameToken(const char* mode)
{
    int index = ServerGameMode_FindIndex(mode);

    return index >= 0 ? kServerGameModeNames[index].token : nullptr;
}

// Text of the mode's list cell: the token of its display name, or the identifier itself when this build has no name
inline const char* ServerGameMode_GetCellText(const char* mode)
{
    const char* token = ServerGameMode_GetNameToken(mode);

    return token != nullptr ? token : mode;
}

// Whether a server of mode is listed under the filter item of filter_mode
inline bool ServerGameMode_MatchesFilter(const char* mode, const char* filter_mode)
{
    return !V_stricmp(mode, filter_mode);
}
