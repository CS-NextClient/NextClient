#pragma once
#include <cstddef>

namespace plugins
{
    inline constexpr size_t max_id_length = 96;
    inline constexpr size_t max_local_cvar_length = 48;
    // Preserve support for external names and fit every generated name.
    inline constexpr size_t max_cvar_name_length = 160;
    static_assert(4 + max_id_length + max_local_cvar_length <= max_cvar_name_length);
} // namespace plugins
