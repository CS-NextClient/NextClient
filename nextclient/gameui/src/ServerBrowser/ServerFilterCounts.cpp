#include "ServerFilterCounts.h"

void ServerFilterCounts_Add(
    const ServerDetailsNext& details,
    bool passes_game_mode_filter,
    bool passes_country_filter,
    ServerFilterCounts* counts
)
{
    if (passes_country_filter)
    {
        counts->all_game_modes++;

        int game_mode = ServerGameMode_FindIndex(details.game_mode);

        if (game_mode >= 0)
        {
            counts->game_modes[game_mode]++;
        }
    }

    if (passes_game_mode_filter)
    {
        counts->all_countries++;

        if (details.country_code[0])
        {
            counts->countries[details.country_code]++;
        }
    }
}
