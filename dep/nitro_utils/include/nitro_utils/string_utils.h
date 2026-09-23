#pragma once
#include <algorithm>
#include <functional>
#include <ranges>
#include <cctype>
#include <locale>
#include <vector>
#include <sstream>

#include <nitro_utils/char_utils.h>

namespace nitro_utils
{
    enum class CompareOptions
    {
        None = 0,
        RegisterIndependent = 1
    };

    //
    //  REGISTER FUNCTIONS
    //

    inline void to_lower(std::string& s)
    {
        std::ranges::for_each(s, [](char& c)
        {
            c = _fast_to_lower(c);
        });
    }

    [[nodiscard]] inline std::string to_lower_copy(const std::string& s)
    {
        std::string result = s;

        std::ranges::for_each(result, [](char& c)
        {
            c = _fast_to_lower(c);
        });

        return result;
    }

    inline void to_upper(std::string& s)
    {
        std::ranges::for_each(s, [](char& c)
        {
            c = ::toupper(c);
        });
    }

    [[nodiscard]] inline std::string to_upper_copy(const std::string& s)
    {
        std::string result = s;

        std::ranges::for_each(result, [](char& c)
        {
            c = ::toupper(c);
        });

        return result;
    }

    //
    //  TRIM FUNCTIONS
    //

    constexpr void ltrim(std::string& s, const std::function<bool(unsigned char)>& condition)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), condition));
    }

    constexpr void ltrim(std::string& s)
    {
        ltrim(s, [](unsigned char ch) { return !std::isspace(ch); });
    }

    constexpr void ltrim(std::string& s, char trim_char)
    {
        ltrim(s, [trim_char](unsigned char ch) { return ch != trim_char; });
    }

    constexpr void rtrim(std::string& s, const std::function<bool(unsigned char)>& condition)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(), condition).base(), s.end());
    }

    constexpr void rtrim(std::string& s)
    {
        rtrim(s, [](unsigned char ch) { return !std::isspace(ch); });
    }

    constexpr void rtrim(std::string& s, char trim_char)
    {
        rtrim(s, [trim_char](unsigned char ch) { return ch != trim_char; });
    }

    constexpr void trim(std::string& s)
    {
        ltrim(s);
        rtrim(s);
    }

    constexpr void trim(std::string& s, char trim_char)
    {
        ltrim(s, trim_char);
        rtrim(s, trim_char);
    }

    constexpr void trim(std::string& s, const std::function<bool(unsigned char)>& condition)
    {
        ltrim(s, condition);
        rtrim(s, condition);
    }

    constexpr std::string trim_copy(const std::string& s)
    {
        std::string copy = s;
        trim(copy);
        return copy;
    }

    // Drops redundant fractional zeros from a decimal-number string
    // ("0.800000" -> "0.8", "1.000000" -> "1"). Non-numeric strings
    // (e.g. "255 255 255") are left unchanged.
    constexpr void trim_float_zeros(std::string& s)
    {
        if (s.find('.') == std::string::npos)
        {
            return;
        }

        if (s.find_first_not_of("0123456789.+-") != std::string::npos)
        {
            return;
        }

        rtrim(s, '0');
        rtrim(s, '.');
    }

    [[nodiscard]] constexpr std::string trim_float_zeros_copy(const std::string& s)
    {
        std::string copy = s;
        trim_float_zeros(copy);
        return copy;
    }

    //
    //  REPLACE FUNCTIONS
    //

    // from https://en.cppreference.com/w/cpp/string/basic_string/replace
    constexpr std::size_t replace_all(std::string& inout, std::string_view what, std::string_view with)
    {
        std::size_t count{};
        for (std::string::size_type pos{};
             std::string::npos != (pos = inout.find(what.data(), pos, what.length()));
             pos += with.length(), ++count)
        {
            inout.replace(pos, what.length(), with.data(), with.length());
        }
        return count;
    }

    [[nodiscard]] constexpr std::string replace_all_copy(const std::string& str, std::string_view what, std::string_view with)
    {
        std::string result(str);

        replace_all(result, what, with);

        return result;
    }

    constexpr void replace_nth(std::string& inout, std::string_view what, int nth, std::string_view with)
    {
        size_t start_pos = 0;
        int i = 0;
        while ((start_pos = inout.find(what, start_pos)) != std::string::npos)
        {
            if (i == nth)
            {
                inout.replace(start_pos, what.length(), with);
                break;
            }

            start_pos += what.size();
            i++;
        }
    }

    [[nodiscard]] constexpr std::string replace_nth_copy(const std::string& str, std::string_view what, int nth, std::string_view with)
    {
        std::string result(str);

        replace_nth(result, what, nth, with);

        return result;
    }

    //
    //  SPLIT FUNCTIONS
    //

    enum class SplitBehavior
    {
        KeepRemainder,
        DiscardRemainder
    };

    size_t split(std::string_view str, std::string_view split_by, std::vector<std::string_view>& tokens);

    inline size_t split(const char* str, std::string_view split_by, std::vector<std::string_view>& tokens)
    {
        return split(std::string_view(str), split_by, tokens);
    }

    template <size_t Count>
    size_t split(
        const char* str,
        std::string_view split_by,
        std::array<std::string_view, Count>& tokens,
        SplitBehavior behavior = SplitBehavior::DiscardRemainder
    )
    {
        return split(std::string_view(str), split_by, tokens, behavior);
    }

    template <size_t Count>
    size_t split(
        std::string_view str,
        std::string_view split_by,
        std::array<std::string_view, Count>& tokens,
        SplitBehavior behavior = SplitBehavior::DiscardRemainder
    )
    {
        if constexpr (Count == 0)
        {
            return 0;
        }

        if (split_by.empty())
        {
            tokens[0] = str;
            return 1;
        }

        size_t token_index = 0;
        size_t pos = 0;

        while (pos < str.size())
        {
            if (token_index + 1 == Count && behavior == SplitBehavior::KeepRemainder)
            {
                tokens[token_index++] = str.substr(pos);
                return token_index;
            }

            size_t found = str.find(split_by, pos);

            if (found == std::string_view::npos)
            {
                tokens[token_index++] = str.substr(pos);
                return token_index;
            }

            tokens[token_index++] = str.substr(pos, found - pos);
            pos = found + split_by.size();

            if (token_index == Count)
            {
                return token_index;
            }
        }

        if (pos == str.size() && token_index < Count)
        {
            tokens[token_index++] = std::string_view{};
        }

        return token_index;
    }

    size_t split(std::string&&, std::string_view, std::vector<std::string_view>&) = delete;

    template <size_t Count>
    size_t split(std::string&&, std::string_view, std::array<std::string_view, Count>&, SplitBehavior = SplitBehavior::DiscardRemainder) = delete;

    size_t split_in_args(const std::string& str, std::vector<std::string>& args, size_t maxArgs);

    //
    //  CODEPAGE CONVERT FUNCTIONS
    //

#ifdef _WIN32
    [[nodiscard]] std::string ConvertCurrentCodepageToUtf8(const std::string_view& str);
#endif

    //
    // VARIOUS FUNCTIONS
    //

    [[nodiscard]] constexpr bool contains(std::string_view str, std::string_view sub, CompareOptions compare_options = CompareOptions::None)
    {
        if (compare_options == CompareOptions::RegisterIndependent)
        {
            auto found = std::ranges::search(str, sub, [](char a, char b) { return _fast_to_lower(a) == _fast_to_lower(b); });
            return !found.empty();
        }

        return str.find(sub) != std::string::npos;
    }

    [[nodiscard]] constexpr bool equals(std::string_view a, std::string_view b, CompareOptions compare_options = CompareOptions::None)
    {
        if (compare_options == CompareOptions::RegisterIndependent)
        {
            return std::ranges::equal(a, b, [](char a, char b) { return _fast_to_lower(a) == _fast_to_lower(b); });
        }

        return a == b;
    }

    [[nodiscard]] constexpr bool is_alpha_numeric_ascii(char c) noexcept
    {
        return (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9');
    }

    [[nodiscard]] constexpr bool start_with(std::string_view str, std::string_view sub, CompareOptions compare_options = CompareOptions::None)
    {
        if (compare_options == CompareOptions::RegisterIndependent)
        {
            auto it = std::ranges::mismatch(str, sub, [](char a, char b) { return _fast_to_lower(a) == _fast_to_lower(b); });
            return it.in2 == sub.end();
        }

        return str.starts_with(sub);
    }

    template <class Iterator>
    [[nodiscard]] std::string join(const Iterator begin, const Iterator end, std::string_view delimiter)
    {
        std::stringstream ss;
        for (Iterator it = begin; it != end; it++)
        {
            ss << *it;

            if (std::next(it) != end)
                ss.write(delimiter.data(), delimiter.size());
        }
        return ss.str();
    }
}
