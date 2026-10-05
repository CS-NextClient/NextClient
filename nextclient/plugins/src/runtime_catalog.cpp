#include "runtime_internal.h"
#include <algorithm>

namespace plugins::runtime
{
    namespace
    {
        std::string catalog_error;
    }
    void validate_profile(const Json& value)
    {
        if (!value.is_object())
            throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
        for (const auto& [key, field] : value.get_object())
            if (key != "schema" && key != "plugins" && key != "settings_slot")
                throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
        if (value.get_object().empty())
            return;
        // Accept the unused hint in profiles saved by builds with manual restore.
        if (auto slot = value.find("settings_slot"); slot && *slot != "a" && *slot != "b")
            throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
        if (value.at("schema") != 2 || value.at("plugins").get_array().size() > 256)
            throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
        std::set<std::string> files;
        for (const auto& row : value.at("plugins").get_array())
        {
            if (row.get_object().size() != 6)
                throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
            const auto& file = row.at("file").get_string();
            // Windows limits a filename component by UTF-16 units, not UTF-8
            // bytes. Count without converting or allocating an intermediate path.
            const auto filename_units =
                file.empty() || file.size() > 4 * 255
                    ? 0
                    : MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, file.data(), static_cast<int>(file.size()), nullptr, 0);
            if (!filename_units || filename_units > 255 || !files.insert(file).second || file.find_first_of("/\\:") != std::string::npos ||
                file.find('\0') != std::string::npos || row.at("id").get_string().size() > 96 || row.at("hash").get_string().size() > 128 ||
                row.at("consent_version") != 1)
                throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
            (void)row.at("enabled").get_boolean();
            permission_mask(row.at("permissions"));
        }
    }
    const Json* approval(const Item& i)
    {
        auto list = profile.find("plugins");
        if (!list)
            return nullptr;
        for (const auto& p : list->get_array())
            if (p.at("file") == i.file && p.at("hash") == i.hash && p.at("id") == i.manifest.id && p.find("permissions") &&
                permission_mask(p.at("permissions")) == i.manifest.permissions && p.find("consent_version") && p.at("consent_version") == 1)
                return &p;
        return nullptr;
    }
    size_t position(const Item& i)
    {
        auto list = profile.find("plugins");
        if (!list)
            return SIZE_MAX;
        for (size_t n = 0; n < list->get_array().size(); ++n)
            if (list->at(n).at("file") == i.file)
                return n;
        return SIZE_MAX;
    }
    void refresh_state(Item& item)
    {
        item.approved = item.enabled = item.running = false;
        if (auto a = approval(item))
        {
            item.approved = true;
            item.enabled = a->at("enabled").get_boolean();
        }
        for (const auto& p : loaded)
            if (p->item.file == item.file)
                item.running = !p->failed;
    }
    void discover()
    {
        catalog_error.clear();
        if (!started)
        {
            observed.clear();
            catalog_memory.resize(0);
            return;
        }
        // Hold the last complete snapshot and its reservation until replacement
        // succeeds. Even protected capacity can be occupied by result handles;
        // failure to allocate a new catalog must not erase existing controls.
        auto previous = std::move(observed);
        auto previous_memory = std::move(catalog_memory);
        try
        {
            const auto folder = root / L"plugins";
            if (!fs::exists(folder))
                return;
            for (const auto& entry : fs::directory_iterator(folder))
            {
                if (!_wcsicmp(entry.path().filename().c_str(), L".host"))
                    continue;
                if (!(entry.is_directory() && fs::exists(entry.path() / L"plugin.dll")) &&
                    !(entry.is_regular_file() && !_wcsicmp(entry.path().extension().c_str(), L".dll")))
                    continue;
                Item item;
                auto filename = entry.path().filename().u8string();
                item.file.assign(reinterpret_cast<const char*>(filename.data()), filename.size());
                try
                {
                    auto package = read_package(entry.path());
                    item.hash = package->hash;
                    item.manifest = package->manifest;
                    if (item.manifest.sdk != NC_SDK_VERSION)
                        item.warning = message("#NextPlugins_SdkMismatch", {item.manifest.sdk, NC_SDK_VERSION});
                }
                catch (const std::exception& e)
                {
                    item.error = error_message(e);
                    // Discovery can fail while a previously approved DLL is still
                    // running. Its locked package remains the authoritative snapshot
                    // for the current session, independently of a rescan failure.
                    auto active = std::find_if(loaded.begin(), loaded.end(), [&](const auto& p) {
                        return p->package && p->package->hash == p->item.hash && p->item.file == item.file;
                    });
                    if (active != loaded.end())
                    {
                        auto diagnostic = std::move(item.error);
                        item = (*active)->item;
                        append_message(item.warning, diagnostic);
                    }
                    else if (auto list = profile.find("plugins"))
                        for (const auto& row : list->get_array())
                            if (row.at("file") == item.file)
                            {
                                // Keep saved identity/selection visible even when no
                                // package metadata can be read. The error still blocks
                                // loading or newly enabling this unverified package.
                                item.hash = row.at("hash").get_string();
                                item.manifest.id = row.at("id").get_string();
                                item.manifest.permissions = permission_mask(row.at("permissions"));
                                break;
                            }
                }
                refresh_state(item);
                for (const auto& p : loaded)
                    if (p->item.file == item.file && p->failed)
                        append_message(item.warning, message("#NextPlugins_CallbackFailed"));
                try
                {
                    catalog_memory.resize(catalog_memory.size() + json_memory(item_json(item)));
                }
                catch (...)
                {
                    // The protected control budget keeps identity and enable/disable
                    // controls available after plugins exhaust their ordinary budget.
                    // Do not replace the row with a blank, apparently disabled item.
                    append_message(item.warning, message("#NextPlugins_ResourceLimit"));
                    catalog_memory.resize(catalog_memory.size() + json_memory(item_json(item)), true);
                }
                observed.push_back(std::move(item));
                if (observed.size() >= 256)
                    break;
            }
            std::sort(observed.begin(), observed.end(), [](const auto& a, const auto& b) {
                auto ap = position(a), bp = position(b);
                return ap == bp ? a.file < b.file : ap < bp;
            });
            std::map<std::string, int> ids;
            for (const auto& i : observed)
                if (!i.manifest.id.empty())
                    ++ids[i.manifest.id];
            for (auto& i : observed)
                if (!i.manifest.id.empty() && ids[i.manifest.id] > 1)
                    i.error = message("#NextPlugins_ErrorDuplicate");
        }
        catch (const std::exception& e)
        {
            if (previous.empty())
                throw;
            catalog_error = error_message(e);
            observed = std::move(previous);
            catalog_memory = std::move(previous_memory);
            for (auto& item : observed)
            {
                refresh_state(item);
                const auto locked = std::find_if(loaded.begin(), loaded.end(), [&](const auto& p) {
                    return p->package && p->package->hash == p->item.hash && p->item.file == item.file && p->item.hash == item.hash;
                });
                // A cached nonloaded row cannot authorize a package that may
                // have changed since it was read. Disabling remains available.
                if (locked == loaded.end())
                    item.error = catalog_error;
            }
        }
    }
    Json item_json(const Item& i)
    {
        Json result{
            {"file", i.file},
            {"hash", i.hash},
            {"id", i.manifest.id},
            {"name", i.manifest.name.empty() ? i.file : i.manifest.name},
            {"author", i.manifest.author},
            {"description", i.manifest.description},
            {"translations", i.manifest.translations},
            {"version", i.manifest.version},
            {"sdk", i.manifest.sdk},
            {"compatibility_revision", i.manifest.revision},
            {"error", i.error},
            {"warning", i.warning},
            {"approved", i.approved},
            {"permissions", permission_names(i.manifest.permissions)},
            {"consent", i.consent},
            {"enabled", i.enabled},
            {"running", i.running}
        };
        auto relations = [&](const auto& rules) {
            Json a = tao::json::empty_array;
            for (const auto& r : rules)
                a.push_back(Json{{"id", r.id}, {"version", r.range}, {"reason", r.reason}});
            return a;
        };
        result["requires"] = relations(i.manifest.required);
        result["conflicts"] = relations(i.manifest.conflicts);
        result["before"] = relations(i.manifest.before);
        result["after"] = relations(i.manifest.after);
        for (const auto& p : loaded)
            if (p->item.file == i.file)
                result["resources"] = resource_stats(p.get());
        return result;
    }
    std::vector<Item> selection(const char* raw)
    {
        auto json = parse(text(raw, 1024 * 1024));
        std::vector<Item> result;
        std::set<std::string> seen;
        for (const auto& row : json.get_array())
        {
            std::string file = row.at("file").get_string();
            if (!seen.insert(file).second)
                throw std::runtime_error(message("#NextPlugins_ErrorSelectionDuplicate"));
            auto it = std::find_if(observed.begin(), observed.end(), [&](const auto& i) { return i.file == file; });
            if (it == observed.end() || it->hash != row.at("hash").get_string())
                throw std::runtime_error(message("#NextPlugins_ErrorFilesChanged"));
            result.push_back(*it);
            result.back().enabled = row.at("enabled").get_boolean();
            if (auto consent = row.find("consent"))
                result.back().consent = consent->get_boolean();
        }
        if (result.size() != observed.size())
            throw std::runtime_error(message("#NextPlugins_ErrorDirectoryChanged"));
        return result;
    }
} // namespace plugins::runtime

using namespace plugins;
using namespace plugins::runtime;

const char* nc_runtime_catalog()
{
    static std::string output;
    try
    {
        discover();
        Json list = tao::json::empty_array;
        for (const auto& i : observed)
            list.push_back(item_json(i));
        output = tao::json::to_string(
            Json{
                {"plugins", list},
                {"safe_mode", safe},
                {"error", catalog_error.empty() ? startup_error : catalog_error},
                {"resources", resource_stats()}
            }
        );
    }
    catch (const std::exception& e)
    {
        output = tao::json::to_string(Json{{"plugins", tao::json::empty_array}, {"safe_mode", true}, {"error", error_message(e)}});
    }
    return output.c_str();
}
const char* nc_runtime_order_warnings(const char* raw)
{
    static std::string output;
    try
    {
        output = ordering_warnings(selection(raw));
    }
    catch (const std::exception& e)
    {
        output = error_message(e);
    }
    return output.c_str();
}
const char* nc_runtime_recommend(const char* raw)
{
    static std::string output;
    try
    {
        auto selected = selection(raw);
        std::string warning;
        auto indices = recommend(selected, warning);
        Json list = tao::json::empty_array;
        std::vector<Item> ordered;
        for (auto n : indices)
        {
            list.push_back(item_json(selected[n]));
            ordered.push_back(selected[n]);
        }
        output = tao::json::to_string(Json{{"plugins", list}, {"warning", warning.empty() ? ordering_warnings(ordered) : warning}});
    }
    catch (const std::exception& e)
    {
        output = tao::json::to_string(Json{{"error", error_message(e)}});
    }
    return output.c_str();
}
const char* nc_runtime_save(const char* raw)
{
    static std::string error;
    error.clear();
    try
    {
        // Re-scan immediately before approving; reject replacements since the
        // dialog opened. A final locked hash check is also made on next launch.
        discover();
        auto selected = selection(raw);
        error = validate(selected);
        if (!error.empty())
            return error.c_str();
        Json list = tao::json::empty_array;
        for (const auto& i : selected)
        {
            if (i.enabled && !i.approved && !i.consent)
                throw std::runtime_error(message("#NextPlugins_ErrorConsent"));
            list.push_back(
                Json{
                    {"file", i.file},
                    {"id", i.manifest.id},
                    {"hash", i.enabled || i.approved ? i.hash : ""},
                    {"enabled", i.enabled},
                    {"permissions", permission_names(i.manifest.permissions)},
                    {"consent_version", 1}
                }
            );
        }
        Json next{{"schema", 2}, {"plugins", list}};
        write_config(root / L"plugins" / L"profile.json", next, validate_profile);
        profile = std::move(next);
        // Saving a reviewed selection acknowledges the failed session.
        if (safe)
            recovery_acknowledge();
    }
    catch (const std::exception& e)
    {
        error = error_message(e);
    }
    return error.c_str();
}
