#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include <next_engine_mini/ServerDetailsNext.h>

struct MMDB_s;

struct GeoIpCountry
{
    // ISO 3166-1 alpha-2 code in upper case
    char code[kCountryCodeSize]{};

    // UTF-8 name in the requested language, or in English when the database lacks that language
    char name[kCountryNameSize]{};
};

class GeoIpCountryDatabase
{
    std::unique_ptr<MMDB_s> db_{};

public:
    GeoIpCountryDatabase();
    ~GeoIpCountryDatabase();

    // Opens a MaxMind DB file with a country section; path is in the ANSI code page of the process.
    bool Open(const char* path);
    void Close();
    bool is_open() const;

    // ip is in host byte order, language a key of the database's names table ("en", "ru", ...); false leaves out zeroed
    bool ResolveCountry(uint32_t ip, const char* language, GeoIpCountry& out) const;
};

// Size of the longest prefix of UTF-8 text that fits max_size bytes without splitting a character
size_t GeoIp_GetUtf8PrefixSize(std::string_view text, size_t max_size);
