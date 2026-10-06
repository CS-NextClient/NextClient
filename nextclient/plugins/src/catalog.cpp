#include "plugin_limits.h"
#include "catalog.h"
#include "pe_image.h"
#include "runtime_budget.h"
#include <nextclient/plugin.h>
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tao/json/events/limit_nesting_depth.hpp>

namespace plugins
{
    namespace
    {
        constexpr const char* permission_ids[]{
            "ui.settings",
            "ui.draw",
            "ui.hide",
            "player.write",
            "cvars.read",
            "cvars.write",
            "audio.play",
            "cvars.create",
            "chat.read",
            "chat.send",
            "connection.connect",
            "connection.disconnect",
            "chat.print",
            "messages.read",
            "messages.filter",
            "ui.windows",
            "ui.input",
            "services.call"
        };
    }
    uint32_t permission_mask(const Json& permissions)
    {
        uint32_t mask = 0;
        for (const auto& name : permissions.get_array())
        {
            uint32_t bit = 0;
            for (size_t i = 0; i < std::size(permission_ids); ++i)
                if (name == permission_ids[i])
                    bit = 1u << i;
            if (!bit || (mask & bit))
                throw std::runtime_error(message("#NextPlugins_ErrorPermission"));
            mask |= bit;
        }
        return mask;
    }
    Json permission_names(uint32_t mask)
    {
        Json result = tao::json::empty_array;
        for (size_t i = 0; i < std::size(permission_ids); ++i)
            if (mask & (1u << i))
                result.push_back(permission_ids[i]);
        return result;
    }
    Json message_value(const char* token, std::initializer_list<Json> args)
    {
        Json values = tao::json::empty_array;
        for (const auto& arg : args)
            values.push_back(arg);
        return Json{{"token", token}, {"args", values}};
    }
    std::string message(const char* token, std::initializer_list<Json> args)
    {
        return tao::json::to_string(message_value(token, args));
    }
    std::string error_message(const std::exception& error)
    {
        try
        {
            auto value = tao::json::from_string(error.what());
            if (value.is_object() && value.find("token"))
                return error.what();
        }
        catch (...)
        {}
        OutputDebugStringA(error.what());
        return message("#NextPlugins_ErrorUnexpected");
    }
    void append_message(std::string& messages, const std::string& next)
    {
        if (next.empty())
            return;
        if (messages.empty())
        {
            messages = next;
            return;
        }
        auto values = parse(messages);
        if (!values.is_array())
            values = Json::array({values});
        values.push_back(parse(next));
        messages = tao::json::to_string(values);
    }
    static void check(bool condition, const char* error)
    {
        if (!condition)
            throw std::runtime_error(message(error));
    }
    Json parse(const std::string& text, size_t limit)
    {
        check(text.size() <= limit, "#NextPlugins_ErrorJsonTooLarge");
        struct Consumer : tao::json::events::limit_nesting_depth<tao::json::events::to_value, 16>
        {
            runtime::Budget memory;
            explicit Consumer(size_t bytes) :
                memory(bytes * 4 + 4096, true)
            {}
            void element()
            {
                memory.resize(memory.size() + 256, true);
                tao::json::events::to_value::element();
            }
            void member()
            {
                memory.resize(memory.size() + 512, true);
                tao::json::events::to_value::member();
            }
        } consumer(text.size());
        tao::json::events::from_string(consumer, text);
        return std::move(consumer.value);
    }
    int64_t integer(const Json& value, int64_t minimum, int64_t maximum)
    {
        check(value.is_signed() || value.is_unsigned(), "#NextPlugins_ErrorInteger");
        if (value.is_unsigned())
        {
            auto number = value.get_unsigned();
            check(number <= static_cast<uint64_t>(maximum), "#NextPlugins_ErrorIntegerRange");
            check(static_cast<int64_t>(number) >= minimum, "#NextPlugins_ErrorIntegerRange");
            return static_cast<int64_t>(number);
        }
        auto number = value.get_signed();
        check(number >= minimum && number <= maximum, "#NextPlugins_ErrorIntegerRange");
        return number;
    }
    static std::string str(const Json& v, const char* key, size_t limit = 4096)
    {
        const auto& s = v.at(key).get_string();
        check(s.size() <= limit && s.find('\0') == std::string::npos, "#NextPlugins_ErrorManifestString");
        return s;
    }
    static std::array<unsigned, 3> version(const std::string& s)
    {
        std::array<unsigned, 3> result{};
        size_t start = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            auto end = i == 2 ? s.size() : s.find('.', start);
            check(end != std::string::npos && end > start, "#NextPlugins_ErrorVersionFormat");
            auto parsed = std::from_chars(s.data() + start, s.data() + end, result[i]);
            check(parsed.ec == std::errc{} && parsed.ptr == s.data() + end, "#NextPlugins_ErrorVersionNumber");
            check(end - start == 1 || s[start] != '0', "#NextPlugins_ErrorVersionZeroes");
            start = end + 1;
        }
        return result;
    }
    bool matches(const std::string& v, const std::string& range)
    {
        auto val = version(v);
        if (range == "*")
            return true;
        check(range.find_first_not_of(' ') != std::string::npos, "#NextPlugins_ErrorVersionRange");
        // Deliberately small, documented grammar: whitespace-separated comparisons
        // are AND; no ambiguous npm/Cargo dialect or prerelease interpretation.
        size_t start = 0;
        bool ok = true;
        while (start < range.size())
        {
            auto end = range.find(' ', start);
            if (end == std::string::npos)
                end = range.size();
            auto token = range.substr(start, end - start);
            if (!token.empty())
            {
                auto n = token.find_first_not_of("<>=");
                check(n != std::string::npos, "#NextPlugins_ErrorVersionComparison");
                auto op = token.substr(0, n);
                auto bound = version(token.substr(n));
                if (op.empty() || op == "=")
                    ok &= val == bound;
                else if (op == ">=")
                    ok &= val >= bound;
                else if (op == "<=")
                    ok &= val <= bound;
                else if (op == ">")
                    ok &= val > bound;
                else if (op == "<")
                    ok &= val < bound;
                else
                    throw std::runtime_error(message("#NextPlugins_ErrorVersionOperator"));
            }
            start = end + 1;
        }
        return ok;
    }
    Manifest manifest(const std::string& text)
    {
        auto j = parse(text);
        check(integer(j.at("schema"), 0, UINT32_MAX) == 1, "#NextPlugins_ErrorManifestSchema");
        Manifest m;
        m.id = str(j, "id", max_id_length);
        check(valid_id(m.id), "#NextPlugins_ErrorPluginId");
        m.name = str(j, "name", 128);
        m.author = str(j, "author", 128);
        m.description = str(j, "description");
        if (auto translations = j.find("translations"))
        {
            check(translations->is_object() && translations->get_object().size() <= 32, "#NextPlugins_ErrorTranslations");
            for (const auto& [language, fields] : translations->get_object())
            {
                check(
                    !language.empty() && language.size() <= 16 &&
                        language.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") == std::string::npos && fields.is_object(),
                    "#NextPlugins_ErrorTranslations"
                );
                Json translated = tao::json::empty_object;
                for (const char* field : {"name", "description"})
                    if (fields.find(field))
                        translated[field] = str(fields, field, std::strcmp(field, "name") == 0 ? 128 : 4096);
                m.translations[language] = std::move(translated);
            }
        }
        m.version = str(j, "version", 32);
        m.sdk = str(j, "sdk", 32);
        version(m.version);
        version(m.sdk);
        m.abi = static_cast<unsigned>(integer(j.at("abi"), 0, UINT32_MAX));
        m.api = static_cast<unsigned>(integer(j.at("api"), 0, UINT32_MAX));
        m.revision = static_cast<unsigned>(integer(j.at("compatibility_revision"), 0, UINT32_MAX));
        check(m.abi == NC_ABI_VERSION, "#NextPlugins_ErrorAbi");
        check(m.api == NC_API_VERSION, "#NextPlugins_ErrorApi");
        if (auto permissions = j.find("permissions"))
            m.permissions = permission_mask(*permissions);
        if (auto caps = j.find("capabilities"))
            for (const auto& cap : caps->get_array())
                check(cap == "settings" || cap == "command", "#NextPlugins_ErrorCapability");
        auto relations = [&](const char* key, std::vector<Relation>& out) {
            if (auto list = j.find(key))
            {
                check(list->get_array().size() <= 64, "#NextPlugins_ErrorRuleCount");
                for (const auto& r : list->get_array())
                {
                    Relation rel{str(r, "id", max_id_length), str(r, "version", 128), str(r, "reason", 512)};
                    check(valid_id(rel.id) && rel.id != m.id, "#NextPlugins_ErrorRelationId");
                    matches("1.0.0", rel.range);
                    out.push_back(std::move(rel));
                }
            }
        };
        relations("requires", m.required);
        relations("conflicts", m.conflicts);
        relations("before", m.before);
        relations("after", m.after);
        return m;
    }
    std::string pe_manifest(const std::vector<unsigned char>& b)
    {
        const PeImage image(b);
        std::string found;
        for (unsigned i = 0; i < image.header.NumberOfSections; ++i)
        {
            auto section = image.Section(i);
            if (std::memcmp(section.Name, ".nclmeta", 8) != 0)
                continue;
            check(found.empty(), "#NextPlugins_ErrorMetadataDuplicate");
            check(section.SizeOfRawData > 0 && section.SizeOfRawData <= 65536, "#NextPlugins_ErrorMetadataSize");
            check(
                section.PointerToRawData <= b.size() && section.SizeOfRawData <= b.size() - section.PointerToRawData,
                "#NextPlugins_ErrorMetadataTruncated"
            );
            const char* data = reinterpret_cast<const char*>(b.data() + section.PointerToRawData);
            auto end = static_cast<const char*>(std::memchr(data, 0, section.SizeOfRawData));
            check(end != nullptr && end != data, "#NextPlugins_ErrorMetadataJson");
            found.assign(data, end);
        }
        check(!found.empty(), "#NextPlugins_ErrorMetadataMissing");
        return found;
    }
    std::string sha256(const std::vector<unsigned char>& bytes)
    {
        BCRYPT_ALG_HANDLE alg{};
        BCRYPT_HASH_HANDLE hash{};
        check(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0, "#NextPlugins_ErrorHashOpen");
        unsigned char digest[32]{};
        auto status = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
        if (status == 0)
            status = BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0);
        if (status == 0)
            status = BCryptFinishHash(hash, digest, sizeof(digest), 0);
        if (hash)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(alg, 0);
        check(status == 0, "#NextPlugins_ErrorHash");
        const char hex[] = "0123456789abcdef";
        std::string result;
        for (auto d : digest)
        {
            result += hex[d >> 4];
            result += hex[d & 15];
        }
        return result;
    }
    static int find(const std::vector<Item>& items, const std::string& id)
    {
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].enabled && items[i].error.empty() && !items[i].manifest.version.empty() && items[i].manifest.id == id)
            {
                return static_cast<int>(i);
            }
        return -1;
    }
    Json display_name(const Item& item)
    {
        Json translations = tao::json::empty_object;
        for (const auto& [language, fields] : item.manifest.translations.get_object())
            if (auto name = fields.find("name"))
                translations[language] = *name;
        return Json{{"text", item.manifest.name.empty() ? item.file : item.manifest.name}, {"translations", translations}};
    }
    std::string validate(const std::vector<Item>& items)
    {
        std::map<std::string, int> ids;
        for (const auto& i : items)
            if (!i.manifest.id.empty())
                ++ids[i.manifest.id];
        for (size_t n = 0; n < items.size(); ++n)
        {
            const auto& i = items[n];
            if (!i.enabled)
                continue;
            auto fail = [&](const std::string& s) { return message("#NextPlugins_DiagnosticContext", {display_name(i), parse(s)}); };
            if (!i.error.empty())
                return fail(i.error);
            if (ids[i.manifest.id] != 1)
                return fail(message("#NextPlugins_ErrorDuplicate"));
            for (const auto& r : i.manifest.required)
            {
                int other = find(items, r.id);
                Json requiredName = message_value("#NextPlugins_RequiredPlugin");
                for (const auto& candidate : items)
                    if (candidate.manifest.id == r.id)
                        requiredName = display_name(candidate);
                if (other < 0 || !matches(items[other].manifest.version, r.range))
                    return fail(message("#NextPlugins_ErrorRequires", {requiredName, r.range, r.reason}));
                if (static_cast<size_t>(other) >= n)
                    return fail(message("#NextPlugins_ErrorLoadFirst", {requiredName}));
            }
            for (const auto& r : i.manifest.conflicts)
            {
                int other = find(items, r.id);
                if (other >= 0 && matches(items[other].manifest.version, r.range))
                    return fail(message("#NextPlugins_ErrorConflict", {display_name(items[other]), r.reason}));
            }
        }
        return {};
    }
    std::string ordering_warnings(const std::vector<Item>& items)
    {
        std::string warning;
        for (size_t n = 0; n < items.size(); ++n)
            if (items[n].enabled)
            {
                auto examine = [&](const auto& rules, bool before) {
                    for (const auto& r : rules)
                    {
                        int other = find(items, r.id);
                        if (other >= 0 && matches(items[other].manifest.version, r.range) &&
                            (before ? n > static_cast<size_t>(other) : n < static_cast<size_t>(other)))
                            append_message(
                                warning,
                                message(
                                    before ? "#NextPlugins_OrderBefore" : "#NextPlugins_OrderAfter",
                                    {display_name(items[n]), display_name(items[other]), r.reason}
                                )
                            );
                    }
                };
                examine(items[n].manifest.before, true);
                examine(items[n].manifest.after, false);
            }
        return warning;
    }
    std::vector<size_t> recommend(const std::vector<Item>& items, std::string& warning)
    {
        const size_t count = items.size();
        std::vector<std::set<size_t>> edges(count);
        auto edge = [&](size_t a, size_t b) { edges[a].insert(b); };
        for (size_t n = 0; n < count; ++n)
            if (items[n].enabled)
            {
                for (const auto& r : items[n].manifest.required)
                {
                    int d = find(items, r.id);
                    if (d >= 0)
                        edge(static_cast<size_t>(d), n);
                }
                for (const auto& r : items[n].manifest.before)
                {
                    int d = find(items, r.id);
                    if (d >= 0 && matches(items[d].manifest.version, r.range))
                        edge(n, static_cast<size_t>(d));
                }
                for (const auto& r : items[n].manifest.after)
                {
                    int d = find(items, r.id);
                    if (d >= 0 && matches(items[d].manifest.version, r.range))
                        edge(static_cast<size_t>(d), n);
                }
            }
        std::vector<size_t> result;
        std::vector<bool> taken(count);
        while (result.size() < count)
        {
            bool progress = false;
            for (size_t n = 0; n < count; ++n)
                if (!taken[n])
                {
                    bool incoming = false;
                    for (size_t j = 0; j < count; ++j)
                        if (!taken[j] && edges[j].count(n))
                            incoming = true;
                    if (!incoming)
                    {
                        taken[n] = true;
                        result.push_back(n);
                        progress = true;
                        break;
                    }
                }
            if (!progress)
            {
                warning = message("#NextPlugins_ErrorOrderCycle");
                result.clear();
                for (size_t n = 0; n < count; ++n)
                    result.push_back(n);
                break;
            }
        }
        return result;
    }
} // namespace plugins
