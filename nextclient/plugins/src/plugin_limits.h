#pragma once
#include <cstddef>
#include <string_view>

namespace plugins
{
    inline constexpr size_t max_id_length = 96;
    inline bool valid_id(std::string_view id)
    {
        return !id.empty() && id.size() <= max_id_length &&
               id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") == std::string_view::npos;
    }
    inline constexpr size_t max_local_cvar_length = 48;
    // Preserve support for external names and fit every generated name.
    inline constexpr size_t max_cvar_name_length = 160;
    static_assert(4 + max_id_length + max_local_cvar_length <= max_cvar_name_length);
} // namespace plugins
