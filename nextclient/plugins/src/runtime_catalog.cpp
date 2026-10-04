#include "runtime_internal.h"
#include <algorithm>

namespace plugins::runtime
{
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
    void discover()
    {
        observed.clear();
        const auto folder = root / L"plugins";
        if (!fs::exists(folder))
            return;
        for (const auto& entry : fs::directory_iterator(folder))
        {
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
                if (auto a = approval(item))
                {
                    item.approved = true;
                    item.enabled = a->at("enabled").get_boolean();
                }
                for (const auto& p : loaded)
                    if (p->item.file == item.file && p->item.hash == item.hash)
                    {
                        item.running = !p->failed;
                        if (p->failed)
                            append_message(item.warning, message("#NextPlugins_CallbackFailed"));
                    }
            }
            catch (const std::exception& e)
            {
                item.error = error_message(e);
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
        output = tao::json::to_string(Json{{"plugins", list}, {"safe_mode", safe}, {"error", startup_error}});
    }
    catch (const std::exception& e)
    {
        output = tao::json::to_string(Json{{"plugins", tao::json::empty_array}, {"safe_mode", true}, {"error", error_message(e)}});
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
        for (auto n : indices)
            list.push_back(item_json(selected[n]));
        output = tao::json::to_string(Json{{"plugins", list}, {"warning", warning.empty() ? ordering_warnings(selected) : warning}});
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
        write_json(root / L"plugins" / L"profile.json", next);
        profile = std::move(next);
    }
    catch (const std::exception& e)
    {
        error = error_message(e);
    }
    return error.c_str();
}
