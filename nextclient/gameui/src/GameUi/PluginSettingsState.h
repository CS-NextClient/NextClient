#pragma once
#include <tao/json.hpp>
#include <map>
#include <string>

// The snapshot belongs to the editing session, not to the running plugin.
// Untouched controls must never replace values changed by plugin callbacks.
class PluginSettingsState
{
public:
    void ResetControl(const tao::json::value& spec, const tao::json::value& current);
    void Collect(tao::json::value& values, const tao::json::value& spec, int value, const tao::json::value& current) const;

private:
    std::map<std::pair<std::string, std::string>, int> baseline_;
};

const tao::json::value* FindPluginControl(const tao::json::value& plugins, const tao::json::value& spec);
