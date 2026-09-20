#include "GeoIpCountryDatabase.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>

#include <winsock2.h>
#include <Windows.h>
#include <maxminddb.h>

#include "common/utf8.h"

namespace
{
    // libmaxminddb takes the file name as UTF-8 on Windows
    std::string AnsiToUtf8(const char* ansi)
    {
        int wide_size = MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);

        if (wide_size <= 0)
        {
            return {};
        }

        std::wstring wide(wide_size, L'\0');
        MultiByteToWideChar(CP_ACP, 0, ansi, -1, wide.data(), wide_size);

        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);

        if (utf8_size <= 0)
        {
            return {};
        }

        std::string utf8(utf8_size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8_size, nullptr, nullptr);
        utf8.resize(utf8_size - 1);

        return utf8;
    }

    // The UTF-8 string field at path (NULL-terminated key list); the view points into the database's memory
    std::optional<std::string_view> ReadUtf8Field(MMDB_entry_s& entry, const char* const* path)
    {
        MMDB_entry_data_s data{};

        if (MMDB_aget_value(&entry, &data, path) != MMDB_SUCCESS || !data.has_data || data.type != MMDB_DATA_TYPE_UTF8_STRING)
        {
            return std::nullopt;
        }

        return std::string_view(data.utf8_string, data.data_size);
    }

    bool IsCountryCode(std::string_view text)
    {
        return text.size() == kCountryCodeLength && std::ranges::all_of(text, [](char c) { return c >= 'A' && c <= 'Z'; });
    }
} // namespace

GeoIpCountryDatabase::GeoIpCountryDatabase() = default;

GeoIpCountryDatabase::~GeoIpCountryDatabase()
{
    Close();
}

bool GeoIpCountryDatabase::Open(const char* path)
{
    Close();

    std::string utf8_path = AnsiToUtf8(path);
    std::unique_ptr<MMDB_s> db = std::make_unique<MMDB_s>();

    if (MMDB_open(utf8_path.c_str(), MMDB_MODE_MMAP, db.get()) != MMDB_SUCCESS)
    {
        return false;
    }

    db_ = std::move(db);

    return true;
}

void GeoIpCountryDatabase::Close()
{
    if (db_ == nullptr)
    {
        return;
    }

    MMDB_close(db_.get());
    db_ = nullptr;
}

bool GeoIpCountryDatabase::is_open() const
{
    return db_ != nullptr;
}

bool GeoIpCountryDatabase::ResolveCountry(uint32_t ip, const char* language, GeoIpCountry& out) const
{
    out = GeoIpCountry{};

    if (db_ == nullptr)
    {
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(ip);

    int error = MMDB_SUCCESS;
    MMDB_lookup_result_s lookup = MMDB_lookup_sockaddr(db_.get(), reinterpret_cast<const sockaddr*>(&address), &error);

    if (error != MMDB_SUCCESS || !lookup.found_entry)
    {
        return false;
    }

    const char* code_path[] = {"country", "iso_code", nullptr};
    std::optional<std::string_view> code = ReadUtf8Field(lookup.entry, code_path);

    if (!code || !IsCountryCode(*code))
    {
        return false;
    }

    std::memcpy(out.code, code->data(), kCountryCodeLength);

    const char* name_path[] = {"country", "names", language, nullptr};
    const char* english_name_path[] = {"country", "names", "en", nullptr};
    std::optional<std::string_view> name = ReadUtf8Field(lookup.entry, name_path);

    if (!name)
    {
        name = ReadUtf8Field(lookup.entry, english_name_path);
    }

    if (name)
    {
        size_t size = GeoIp_GetUtf8PrefixSize(*name, sizeof(out.name) - 1);
        std::memcpy(out.name, name->data(), size);
    }

    return true;
}

size_t GeoIp_GetUtf8PrefixSize(std::string_view text, size_t max_size)
{
    size_t size = std::min(text.size(), max_size);

    while (size > 0 && size < text.size() && (static_cast<uint8_t>(text[size]) & kUtf8ContinuationMask) == kUtf8ContinuationBits)
    {
        size--;
    }

    return size;
}
