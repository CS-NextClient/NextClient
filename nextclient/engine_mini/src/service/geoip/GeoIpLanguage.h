#pragma once

#include <utility>

#include <strtools.h>

// Steam API language name -> MaxMind DB names table key. A language the database has no names in maps to the
// closest one it has: Portuguese to Brazilian Portuguese, Traditional Chinese to Simplified Chinese.
inline constexpr std::pair<const char*, const char*> kGeoIpLanguages[] = {
    {"russian", "ru"},
    {"german", "de"},
    {"french", "fr"},
    {"spanish", "es"},
    {"latam", "es"},
    {"japanese", "ja"},
    {"koreana", "ko"},
    {"korean", "ko"},
    {"brazilian", "pt-BR"},
    {"portuguese", "pt-BR"},
    {"schinese", "zh-CN"},
    {"tchinese", "zh-CN"},
};

// Names table key for a Steam API language name; "en" for a language kGeoIpLanguages does not list
inline const char* GeoIp_GetNamesLanguage(const char* steam_language)
{
    for (const auto& [name, key] : kGeoIpLanguages)
    {
        if (V_stricmp(name, steam_language) == 0)
        {
            return key;
        }
    }

    return "en";
}
