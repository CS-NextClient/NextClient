#include "ServerBrowserText.h"

#include <algorithm>
#include <cwchar>

#include <nitro_utils/string_utils.h>
#include <strtools.h>

namespace
{
    constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
    constexpr std::string_view kCommentStart = "//";
    constexpr std::string_view kBlankChars = " \t\r";
    constexpr char kNativeNameSeparator = ';';

    std::wstring LowerCaseUtf8(std::string_view utf8)
    {
        return nitro_utils::to_lower_copy(nitro_utils::utf8_to_wide(utf8));
    }
} // namespace

int ServerBrowserText_CompareUnknownLast(const char* v1, const char* v2)
{
    if (!v1[0] || !v2[0])
    {
        return (v1[0] ? 0 : 1) - (v2[0] ? 0 : 1);
    }

    return V_stricmp(v1, v2);
}

int ServerBrowserText_CompareUnknownLast(const wchar_t* v1, const wchar_t* v2)
{
    if (!v1[0] || !v2[0])
    {
        return (v1[0] ? 0 : 1) - (v2[0] ? 0 : 1);
    }

    return _wcsicmp(v1, v2);
}

std::string ServerBrowserText_GetCountryLabel(const std::string& code, const std::string& name)
{
    return name.empty() ? code : name;
}

CountryNativeNames ServerBrowserText_ParseCountryNativeNames(std::string_view text)
{
    CountryNativeNames native_names;

    if (text.starts_with(kUtf8Bom))
    {
        text.remove_prefix(kUtf8Bom.size());
    }

    while (!text.empty())
    {
        size_t line_end = text.find('\n');
        std::string_view line = nitro_utils::trim_view(text.substr(0, line_end), kBlankChars);
        text.remove_prefix(line_end == std::string_view::npos ? text.size() : line_end + 1);

        bool has_code = line.size() > kCountryCodeLength && nitro_utils::is_alpha_ascii(line[0]) &&
                        nitro_utils::is_alpha_ascii(line[1]) &&
                        (line[kCountryCodeLength] == ' ' || line[kCountryCodeLength] == '\t');

        if (line.starts_with(kCommentStart) || !has_code)
        {
            continue;
        }

        std::string code(line.substr(0, kCountryCodeLength));
        nitro_utils::to_upper(code);

        std::string_view names = line.substr(kCountryCodeLength);

        while (!names.empty())
        {
            size_t separator = names.find(kNativeNameSeparator);
            std::string_view name = nitro_utils::trim_view(names.substr(0, separator), kBlankChars);
            names.remove_prefix(separator == std::string_view::npos ? names.size() : separator + 1);

            if (!name.empty())
            {
                native_names.by_code[code].push_back(LowerCaseUtf8(name));
            }
        }
    }

    return native_names;
}

std::string ServerBrowserText_FindCountryCode(
    const std::map<std::string, std::string>& countries,
    const CountryNativeNames& native_names,
    const wchar_t* lower_text
)
{
    if (!lower_text[0])
    {
        return {};
    }

    for (const auto& [code, name] : countries)
    {
        if (LowerCaseUtf8(ServerBrowserText_GetCountryLabel(code, name)) == lower_text)
        {
            return code;
        }

        auto native_it = native_names.by_code.find(code);

        if (native_it != native_names.by_code.end() &&
            std::ranges::find(native_it->second, std::wstring_view(lower_text)) != native_it->second.end())
        {
            return code;
        }
    }

    return {};
}

bool ServerBrowserText_MatchesCountryFilter(
    const ServerDetailsNext& details,
    const CountryNativeNames& native_names,
    const char* code_filter,
    const wchar_t* lower_text
)
{
    if (code_filter[0])
    {
        return !V_stricmp(details.country_code, code_filter);
    }

    if (!details.country_code[0])
    {
        return false;
    }

    if (LowerCaseUtf8(details.country_code) == lower_text)
    {
        return true;
    }

    std::wstring name = LowerCaseUtf8(details.country_name);

    if (name.starts_with(lower_text))
    {
        return true;
    }

    auto native_it = native_names.by_code.find(details.country_code);

    return native_it != native_names.by_code.end() && std::ranges::any_of(native_it->second, [lower_text](const std::wstring& native_name) {
               return native_name.starts_with(lower_text);
           });
}
