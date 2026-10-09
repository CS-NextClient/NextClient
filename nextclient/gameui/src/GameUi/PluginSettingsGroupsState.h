#pragma once

#include <string>
#include <vector>

#include <tao/json.hpp>

#include "PluginSettingsState.h"

class PluginSettingsGroupsState
{
public:
    struct Group
    {
        std::string owner;
        tao::json::value metadata;
        tao::json::value controls;
        bool available{};
    };

    explicit PluginSettingsGroupsState(const tao::json::value& plugins);
    void BeginSession(const PluginSettingsSnapshot& current);
    bool RefreshAvailability(const PluginSettingsSnapshot& current);
    void Toggle(const std::string& owner);
    bool has_available_controls() const;
    bool is_expanded(const std::string& owner) const;
    const std::vector<Group>& groups() const;

private:
    std::vector<Group> groups_;
    std::string expanded_;
};
