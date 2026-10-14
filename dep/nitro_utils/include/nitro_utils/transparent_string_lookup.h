#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <nitro_utils/char_utils.h>

namespace nitro_utils
{
    enum class StringCase
    {
        Sensitive,
        Insensitive
    };

    template <StringCase Case = StringCase::Sensitive>
    struct transparent_string_hash
    {
        using is_transparent = void;

        size_t operator()(std::string_view value) const noexcept
        {
            if constexpr (Case == StringCase::Sensitive)
            {
                return std::hash<std::string_view>{}(value);
            }
            else
            {
                size_t hash = 0;
                for (char c : value)
                {
                    hash = hash * 31u + static_cast<unsigned char>(_fast_to_lower(c));
                }

                return hash;
            }
        }
    };

    template <StringCase Case = StringCase::Sensitive>
    struct transparent_string_equal
    {
        using is_transparent = void;

        bool operator()(std::string_view lhs, std::string_view rhs) const noexcept
        {
            if constexpr (Case == StringCase::Sensitive)
            {
                return lhs == rhs;
            }
            else
            {
                if (lhs.size() != rhs.size())
                {
                    return false;
                }

                for (size_t i = 0; i < lhs.size(); ++i)
                {
                    if (_fast_to_lower(lhs[i]) != _fast_to_lower(rhs[i]))
                    {
                        return false;
                    }
                }

                return true;
            }
        }
    };

    template <typename TValue, StringCase Case = StringCase::Sensitive>
    using transparent_string_map =
        std::unordered_map<std::string, TValue, transparent_string_hash<Case>, transparent_string_equal<Case>>;

    template <StringCase Case = StringCase::Sensitive>
    using basic_transparent_string_set =
        std::unordered_set<std::string, transparent_string_hash<Case>, transparent_string_equal<Case>>;

    using transparent_string_set = basic_transparent_string_set<>;
} // namespace nitro_utils
