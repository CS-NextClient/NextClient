#pragma once

#include <cstddef>
#include <string_view>

namespace cvars
{
    struct CvarDefault
    {
        const char* name{};
        const char* value{};
    };

    struct CvarDefinition
    {
        const char* name{};
        const char* default_value{};
        float min_value{};
        float max_value{};
        int flags{};
    };

    // A missing name returns a value-initialized definition.
    template <class Definition, std::size_t Size>
    constexpr Definition FindDefinition(const Definition (&definitions)[Size], std::string_view name)
    {
        for (const Definition& definition : definitions)
        {
            if (name == definition.name)
            {
                return definition;
            }
        }

        return Definition{};
    }
} // namespace cvars
