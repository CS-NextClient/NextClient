#include "PluginSettingsState.h"

void PluginSettingsState::ResetControl(const tao::json::value& spec, const tao::json::value& current)
{
    const auto key = std::make_pair(spec.at("owner").get_string(), spec.at("id").get_string());
    if (const auto* active = FindPluginControl(current, spec))
        baseline_[key] = active->at("value").as<int>();
    else
        baseline_.erase(key);
}

void PluginSettingsState::Collect(tao::json::value& values, const tao::json::value& spec, int value, const tao::json::value& current) const
{
    const auto initial = baseline_.find({spec.at("owner").get_string(), spec.at("id").get_string()});
    const auto* active = FindPluginControl(current, spec);
    if (initial != baseline_.end() && active && value != initial->second && value != active->at("value").as<int>())
        values.push_back(tao::json::value{{"owner", spec.at("owner")}, {"id", spec.at("id")}, {"value", value}});
}

const tao::json::value* FindPluginControl(const tao::json::value& plugins, const tao::json::value& spec)
{
    for (const auto& plugin : plugins.get_array())
        if (plugin.at("id") == spec.at("owner"))
            for (const auto& control : plugin.at("controls").get_array())
                if (control.at("id") == spec.at("id"))
                    return &control;
    return nullptr;
}
