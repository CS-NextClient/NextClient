#include "PluginSettingsState.h"

const tao::json::value* PluginSettingsState::ResetControl(const tao::json::value& spec, const PluginSettingsSnapshot& current)
{
    const auto key = std::make_pair(spec.at("owner").get_string(), spec.at("id").get_string());
    const auto* active = current.Find(spec);
    if (active)
        baseline_[key] = active->at("value").as<int>();
    else
        baseline_.erase(key);
    return active;
}

bool PluginSettingsState::Collect(
    tao::json::value& values,
    const tao::json::value& spec,
    int value,
    const PluginSettingsSnapshot& current
) const
{
    const auto initial = baseline_.find({spec.at("owner").get_string(), spec.at("id").get_string()});
    const auto* active = current.Find(spec);
    if (initial != baseline_.end() && active && value != initial->second && value != active->at("value").as<int>())
        values.push_back(tao::json::value{{"owner", spec.at("owner")}, {"id", spec.at("id")}, {"value", value}});
    return active != nullptr;
}

PluginSettingsSnapshot::PluginSettingsSnapshot(const tao::json::value& plugins)
{
    for (const auto& plugin : plugins.get_array())
        for (const auto& control : plugin.at("controls").get_array())
            controls_.emplace(std::make_pair(plugin.at("id").get_string(), control.at("id").get_string()), control);
}
const tao::json::value* PluginSettingsSnapshot::Find(const tao::json::value& spec) const
{
    const auto it = controls_.find({spec.at("owner").get_string(), spec.at("id").get_string()});
    return it == controls_.end() ? nullptr : &it->second;
}
