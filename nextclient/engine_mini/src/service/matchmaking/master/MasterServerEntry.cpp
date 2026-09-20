#include "MasterServerEntry.h"

#include <algorithm>

#include <next_engine_mini/ServerDetailsNext.h>
#include <nitro_utils/string_utils.h>

namespace
{
    bool IsGameModeChar(char c)
    {
        return nitro_utils::is_alpha_numeric_ascii(c) || c == '_';
    }
} // namespace

bool MasterDetails_IsGameModeId(std::string_view text)
{
    return !text.empty() && text.size() < kGameModeIdSize && std::ranges::all_of(text, IsGameModeChar);
}

MasterDetails MasterDetails_Parse(std::string_view game_mode, std::string_view country)
{
    MasterDetails details;

    if (MasterDetails_IsGameModeId(game_mode))
    {
        details.game_mode = game_mode;
    }

    if (country.size() == kCountryCodeLength && std::ranges::all_of(country, nitro_utils::is_alpha_ascii))
    {
        details.country_code = country;
        nitro_utils::to_upper(details.country_code);
    }

    return details;
}
