#pragma once

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <next_engine_mini/ServerDetailsNext.h>

struct CountryNativeNames
{
    // lower-case names by upper-case ISO 3166-1 alpha-2 code
    std::unordered_map<std::string, std::vector<std::wstring>> by_code{};
};

// case-insensitive; an empty (unknown) value sorts after every known one
int ServerBrowserText_CompareUnknownLast(const char* v1, const char* v2);
int ServerBrowserText_CompareUnknownLast(const wchar_t* v1, const wchar_t* v2);

std::string ServerBrowserText_GetCountryLabel(const std::string& code, const std::string& name);

// docs/master-server-http-protocol.md describes the file; a line of another form is skipped
CountryNativeNames ServerBrowserText_ParseCountryNativeNames(std::string_view text);

// countries maps a code to its UTF-8 name; lower_text has to be a whole label or native name
std::string ServerBrowserText_FindCountryCode(
    const std::map<std::string, std::string>& countries,
    const CountryNativeNames& native_names,
    const wchar_t* lower_text
);

// with a code_filter the codes must be equal, otherwise lower_text must be the code or start a name of the country
bool ServerBrowserText_MatchesCountryFilter(
    const ServerDetailsNext& details,
    const CountryNativeNames& native_names,
    const char* code_filter,
    const wchar_t* lower_text
);
