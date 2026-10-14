#include "PluginSettingsGroupsState.h"

#include <algorithm>

PluginSettingsGroupsState::PluginSettingsGroupsState(const tao::json::value& plugins)
{
    for (const auto& plugin : plugins.get_array())
    {
        const auto& tabs = plugin.at("tabs").get_array();
        const bool custom_plugins_tab = std::any_of(tabs.begin(), tabs.end(), [](const auto& tab) { return tab.at("id") == "plugins"; });
        tao::json::value controls = tao::json::empty_array;
        for (const auto& control : plugin.at("controls").get_array())
        {
            if (control.at("tab") == "" || (control.at("tab") == "plugins" && !custom_plugins_tab))
            {
                tao::json::value owned = control;
                owned["owner"] = plugin.at("id");
                controls.push_back(std::move(owned));
            }
        }
        if (!controls.get_array().empty())
        {
            tao::json::value metadata{{"name", plugin.at("name")}};
            if (const auto* translations = plugin.find("translations"))
            {
                metadata["translations"] = *translations;
            }
            groups_.push_back({plugin.at("id").get_string(), std::move(metadata), std::move(controls)});
        }
    }
    BeginSession(PluginSettingsSnapshot(plugins));
}

void PluginSettingsGroupsState::BeginSession(const PluginSettingsSnapshot& current)
{
    RefreshAvailability(current);
    expanded_.clear();
    for (const Group& group : groups_)
    {
        if (group.available)
        {
            expanded_ = group.owner;
            break;
        }
    }
}

bool PluginSettingsGroupsState::RefreshAvailability(const PluginSettingsSnapshot& current)
{
    bool changed = false;
    for (Group& group : groups_)
    {
        const auto& controls = group.controls.get_array();
        const bool available =
            std::any_of(controls.begin(), controls.end(), [&](const auto& control) { return current.Find(control) != nullptr; });
        changed |= group.available != available;
        group.available = available;
        if (!available && expanded_ == group.owner)
        {
            expanded_.clear();
        }
    }
    return changed;
}

void PluginSettingsGroupsState::Toggle(const std::string& owner)
{
    for (const Group& group : groups_)
    {
        if (group.owner == owner && group.available)
        {
            expanded_ = expanded_ == owner ? std::string{} : owner;
            return;
        }
    }
}

bool PluginSettingsGroupsState::has_available_controls() const
{
    return std::any_of(groups_.begin(), groups_.end(), [](const Group& group) { return group.available; });
}

bool PluginSettingsGroupsState::is_expanded(const std::string& owner) const
{
    return expanded_ == owner;
}

const std::vector<PluginSettingsGroupsState::Group>& PluginSettingsGroupsState::groups() const
{
    return groups_;
}
