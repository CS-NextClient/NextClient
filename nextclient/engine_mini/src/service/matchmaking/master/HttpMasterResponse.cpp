#include "HttpMasterResponse.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

#include <easylogging++.h>
#include <nitro_utils/string_utils.h>
#include <tao/json.hpp>
#include <tao/json/events/limit_nesting_depth.hpp>

#include "common/utf8.h"

namespace
{
    constexpr std::string_view kBlankChars = " \t\r\n";

    // The recursive JSON parser takes stack for every nesting level. An array of entry objects needs two;
    // the rest is room for nested members a later protocol revision may add.
    constexpr size_t kMaxJsonNestingDepth = 16;

    constexpr size_t kIpv4OctetCount = 4;
    constexpr uint32_t kOctetBits = 8;
    constexpr uint32_t kMaxOctet = 255;
    constexpr uint32_t kMaxPort = 65535;

    // Reads a decimal number of at most max from the front of text, consuming its digits
    bool ReadNumber(std::string_view& text, uint32_t max, uint32_t& out)
    {
        std::from_chars_result read = std::from_chars(text.data(), text.data() + text.size(), out);

        if (read.ec != std::errc{} || out > max)
        {
            return false;
        }

        text.remove_prefix(read.ptr - text.data());

        return true;
    }

    // Parses text that is exactly a.b.c.d:port in decimal
    bool ParseAddress(std::string_view text, netadr_t& out)
    {
        uint32_t ip = 0;

        for (size_t i = 0; i < kIpv4OctetCount; i++)
        {
            uint32_t octet;

            if (!ReadNumber(text, kMaxOctet, octet))
            {
                return false;
            }

            char separator = i + 1 < kIpv4OctetCount ? '.' : ':';

            if (text.empty() || text.front() != separator)
            {
                return false;
            }

            text.remove_prefix(1);

            ip = (ip << kOctetBits) | octet;
        }

        uint32_t port;

        if (!ReadNumber(text, kMaxPort, port) || !text.empty())
        {
            return false;
        }

        out = netadr_t(ip, static_cast<uint16_t>(port));

        return true;
    }

    std::string_view ReadString(const tao::json::value& object, const char* key)
    {
        const tao::json::value* field = object.find(key);

        if (field == nullptr || !field->is_string())
        {
            return {};
        }

        return field->get_string();
    }

    std::optional<std::vector<MasterServerEntry>> ParseJson(std::string_view body)
    {
        tao::json::events::limit_nesting_depth<tao::json::events::to_value, kMaxJsonNestingDepth> root;

        try
        {
            tao::json::events::from_string(root, body);
        }
        catch (const std::exception& e)
        {
            LOG(WARNING) << "[HttpMasterResponse] Malformed JSON server list: " << e.what();
            return std::nullopt;
        }

        if (!root.value.is_array())
        {
            return std::nullopt;
        }

        std::vector<MasterServerEntry> result;

        for (const tao::json::value& item : root.value.get_array())
        {
            MasterServerEntry entry;

            if (!item.is_object() || !ParseAddress(ReadString(item, "address"), entry.address) || !entry.address.IsValid())
            {
                continue;
            }

            entry.details = MasterDetails_Parse(ReadString(item, "game_mode"), ReadString(item, "country"));
            result.push_back(std::move(entry));
        }

        return result;
    }

    std::optional<std::vector<MasterServerEntry>> ParseLines(std::string_view body)
    {
        std::vector<MasterServerEntry> result;

        size_t pos = 0;
        while (pos <= body.size())
        {
            size_t end = body.find('\n', pos);

            if (end == std::string_view::npos)
            {
                end = body.size();
            }

            std::string_view line = nitro_utils::trim_view(body.substr(pos, end - pos), kBlankChars);
            pos = end + 1;

            if (line.empty())
            {
                continue;
            }

            MasterServerEntry entry;

            if (!ParseAddress(line, entry.address))
            {
                return std::nullopt;
            }

            if (entry.address.IsValid())
            {
                result.push_back(std::move(entry));
            }
        }

        return result;
    }
} // namespace

std::optional<std::vector<MasterServerEntry>> HttpMasterResponse_Parse(std::string_view body)
{
    if (body.starts_with(kUtf8ByteOrderMark))
    {
        body.remove_prefix(kUtf8ByteOrderMark.size());
    }

    std::string_view content = nitro_utils::trim_view(body, kBlankChars);

    if (content.starts_with('['))
    {
        return ParseJson(content);
    }

    return ParseLines(content);
}
