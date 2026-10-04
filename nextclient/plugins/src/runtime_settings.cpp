#include "runtime_internal.h"
#include <algorithm>

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
                    Json{{"id", p->item.manifest.id}, {"name", p->item.manifest.name}, {"tabs", p->tabs}, {"controls", controls}}
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
        auto next = settings;
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
                            if (!next.find(p->item.manifest.id))
                                next[p->item.manifest.id] = tao::json::empty_object;
                            next[p->item.manifest.id][id] = value;
                            found = true;
                            if (value != setting_value(*p, c))
                                changes.push_back({p.get(), id, value});
                        }
            if (!found)
                throw std::runtime_error(message("#NextPlugins_ErrorSettingUnavailable"));
        }
        write_json(root / L"plugins" / L"settings.json", next);
        settings = std::move(next);
        for (const auto& c : changes)
            if (!c.p->failed && c.p->api.setting_changed && c.p->api.setting_changed(c.id.c_str(), c.value) != 0)
                fail(*c.p);
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
                    if (p->api.action(id) != 0)
                        fail(*p);
        }
}
