#include "MasterListCache.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace
{
    constexpr uint8_t kHeaderMagic[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    constexpr uint8_t kLayoutVersion = 2;
    constexpr size_t kHeaderSize = sizeof(kHeaderMagic) + sizeof(kLayoutVersion);
    constexpr size_t kAddressSize = sizeof(uint32_t) + sizeof(uint16_t);
    constexpr size_t kMaxFieldLength = 255;

    template <class T>
    void Append(std::string& out, const T& value)
    {
        out.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    void AppendField(std::string& out, const std::string& value)
    {
        uint8_t length = static_cast<uint8_t>(std::min(value.size(), kMaxFieldLength));

        Append(out, length);
        out.append(value.data(), length);
    }

    template <class T>
    T ReadAt(std::string_view data, size_t offset)
    {
        T value;
        std::memcpy(&value, data.data() + offset, sizeof(value));

        return value;
    }

    // Reads a length-prefixed field at offset, advancing it; false when the data ends inside the field.
    bool ReadField(std::string_view data, size_t& offset, std::string& out)
    {
        if (offset + sizeof(uint8_t) > data.size())
        {
            return false;
        }

        uint8_t length = ReadAt<uint8_t>(data, offset);
        offset += sizeof(length);

        if (offset + length > data.size())
        {
            return false;
        }

        out.assign(data.data() + offset, length);
        offset += length;

        return true;
    }
} // namespace

std::string MasterListCache_Encode(const std::vector<MasterServerEntry>& server_list)
{
    std::string out;

    out.append(reinterpret_cast<const char*>(kHeaderMagic), sizeof(kHeaderMagic));
    Append(out, kLayoutVersion);

    for (const MasterServerEntry& entry : server_list)
    {
        Append(out, entry.address.GetIPHostByteOrder());
        Append(out, entry.address.GetPortHostByteOrder());
        AppendField(out, entry.details.game_mode);
        AppendField(out, entry.details.country_code);
    }

    return out;
}

std::vector<MasterServerEntry> MasterListCache_Decode(std::string_view data)
{
    std::vector<MasterServerEntry> result;

    bool has_header = data.size() >= kHeaderSize && std::memcmp(data.data(), kHeaderMagic, sizeof(kHeaderMagic)) == 0;

    if (!has_header)
    {
        for (size_t offset = 0; offset + kAddressSize <= data.size(); offset += kAddressSize)
        {
            uint32_t ip = ReadAt<uint32_t>(data, offset);
            uint16_t port = ReadAt<uint16_t>(data, offset + sizeof(ip));

            result.push_back(MasterServerEntry{netadr_t(ip, port)});
        }

        return result;
    }

    if (ReadAt<uint8_t>(data, sizeof(kHeaderMagic)) != kLayoutVersion)
    {
        return result;
    }

    size_t offset = kHeaderSize;

    while (offset + kAddressSize <= data.size())
    {
        uint32_t ip = ReadAt<uint32_t>(data, offset);
        uint16_t port = ReadAt<uint16_t>(data, offset + sizeof(ip));
        offset += kAddressSize;

        std::string game_mode;
        std::string country;

        if (!ReadField(data, offset, game_mode) || !ReadField(data, offset, country))
        {
            break;
        }

        result.push_back(MasterServerEntry{netadr_t(ip, port), MasterDetails_Parse(game_mode, country)});
    }

    return result;
}
