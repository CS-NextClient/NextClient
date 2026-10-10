#include "runtime_internal.h"
#include <algorithm>

namespace plugins::runtime
{
    void validate_settings(const Json& value)
    {
        if (!value.is_object() || value.get_object().size() > 4096)
            throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
        for (const auto& [id, setting] : value.get_object())
        {
            if (!valid_id(id))
                throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
            integer(setting, INT32_MIN, INT32_MAX);
        }
    }
    fs::path settings_path(const Loaded& p)
    {
        return root / L"plugins" / L".host" / L"settings" / ("plugin-" + p.item.manifest.id + ".json");
    }
    void migrate_settings()
    {
        const auto legacy = root / L"plugins" / L"settings.json";
        if (!fs::exists(legacy))
            return;
        try
        {
            // Also recover files written just above the old aggregate reader limit.
            auto value = parse(read_text(legacy, 16 * 1024 * 1024), 16 * 1024 * 1024);
            for (const auto& [id, data] : value.get_object())
            {
                if (!valid_id(id))
                    continue;
                const auto path = root / L"plugins" / L".host" / L"settings" / ("plugin-" + id + ".json");
                if (fs::exists(path))
                    continue; // Resume an interrupted migration without replacing newer edits.
                try
                {
                    validate_settings(data);
                    parse(tao::json::to_string(data));
                }
                catch (const std::exception&)
                {
                    append_message(startup_error, message("#NextPlugins_SettingsRecovery", {id}));
                    continue;
                }
                write_config(path, data, validate_settings);
            }
            auto backup = legacy;
            backup += L".migrated.bak";
            if (!fs::exists(backup))
                fs::copy_file(legacy, backup);
            fs::remove(legacy);
        }
        catch (const std::exception&)
        {
            append_message(startup_error, message("#NextPlugins_SettingsRecovery", {"settings.json"}));
        }
    }
    void load_settings(Loaded& p)
    {
        const auto path = settings_path(p);
        for (const auto* suffix : {L"", L".bak"})
        {
            auto candidate = path;
            candidate += suffix;
            if (!fs::exists(candidate))
                continue;
            try
            {
                auto value = parse(read_text(candidate));
                validate_settings(value);
                replace_json(p.values, p.values_memory, std::move(value));
                return;
            }
            catch (const std::exception&)
            {
                append_message(startup_error, message("#NextPlugins_SettingsRecovery", {p.item.manifest.id}));
            }
        }
    }
    void save_settings(Loaded& p, Json value)
    {
        validate_settings(value);
        if (value == p.values)
            return;
        Budget candidate(json_memory(value));
        write_config(settings_path(p), value, validate_settings);
        p.values = std::move(value);
        p.values_memory = std::move(candidate);
    }
} // namespace plugins::runtime

using namespace plugins;
using namespace plugins::runtime;

const char* nc_runtime_ui()
{
    static std::string output;
    try
    {
        Json list = tao::json::empty_array;
        for (const auto& p : loaded)
            if (!p->failed)
            {
                auto controls = p->controls;
                for (auto& c : controls.get_array())
                    c["value"] = setting_value(*p, c);
                list.push_back(
                    Json{
                        {"id", p->item.manifest.id},
                        {"name", p->item.manifest.name},
                        {"translations", p->item.manifest.translations},
                        {"tabs", p->tabs},
                        {"controls", controls}
                    }
                );
            }
        output = tao::json::to_string(list);
    }
    catch (...)
    {
        output = "[]";
    }
    return output.c_str();
}
const char* nc_runtime_settings(const char* raw)
{
    static std::string error;
    error.clear();
    try
    {
        auto values = parse(text(raw, 1024 * 1024));
        std::map<Loaded*, Json> next;
        struct Change
        {
            Loaded* p;
            std::string id;
            int32_t value;
        };
        std::vector<Change> changes;
        for (const auto& v : values.get_array())
        {
            // A callback may retire a plugin after Options captured its controls.
            // Ignore only known retired owners; unknown controls still fail validation.
            bool retired =
                std::any_of(loaded.begin(), loaded.end(), [&](const auto& p) { return p->failed && v.at("owner") == p->item.manifest.id; });
            if (retired)
                continue;
            bool found = false;
            for (auto& p : loaded)
                if (!p->failed && v.at("owner") == p->item.manifest.id)
                    for (const auto& c : p->controls.get_array())
                        if (v.at("id") == c.at("id") && c.at("kind").as<unsigned>() != NC_BUTTON)
                        {
                            auto value = static_cast<int32_t>(integer(v.at("value"), INT32_MIN, INT32_MAX));
                            if (value < c.at("min").as<int32_t>() || value > c.at("max").as<int32_t>())
                                throw std::runtime_error(message("#NextPlugins_ErrorSettingRange"));
                            auto id = c.at("id").get_string();
                            next.try_emplace(p.get(), p->values);
                            next.at(p.get())[id] = value;
                            found = true;
                            if (value != setting_value(*p, c))
                                changes.push_back({p.get(), id, value});
                        }
            if (!found)
                throw std::runtime_error(message("#NextPlugins_ErrorSettingUnavailable"));
        }
        // Validate every candidate before the first commit. Each owner's file is
        // atomic; a disk error for one owner cannot corrupt another owner's file.
        for (const auto& [p, value] : next)
        {
            validate_settings(value);
            parse(tao::json::to_string(value));
        }
        for (auto& [p, value] : next)
        {
            if (p->failed)
                continue;
            save_settings(*p, std::move(value));
            // Notify each committed owner even if a later owner's disk write fails.
            for (const auto& c : changes)
                if (c.p == p && !p->failed && p->api.setting_changed &&
                    invoke(*p, CallbackCategory::Setting, [&] { return p->api.setting_changed(c.id.c_str(), c.value); }) != 0)
                    fail(*p);
        }
    }
    catch (const std::exception& e)
    {
        error = error_message(e);
    }
    return error.c_str();
}
void nc_runtime_action(const char* owner, const char* id)
{
    for (auto& p : loaded)
        if (!p->failed && p->item.manifest.id == owner && p->api.action)
        {
            for (const auto& c : p->controls.get_array())
                if (c.at("id") == id && c.at("kind").as<unsigned>() == NC_BUTTON)
                {
                    if (invoke(*p, CallbackCategory::Action, [&] { return p->api.action(id); }) != 0)
                        fail(*p);
                    return;
                }
        }
}
