#pragma once
#include <tao/json.hpp>
#include <map>
#include <string>

tao::json::value PluginSettings_Parse(const char* source);

// The snapshot belongs to the editing session, not to the running plugin.
// Untouched controls must never replace values changed by plugin callbacks.
class PluginSettingsSnapshot
{
public:
    PluginSettingsSnapshot() = default;
    PluginSettingsSnapshot(const tao::json::value& plugins);
    const tao::json::value* Find(const tao::json::value& spec) const;

private:
    std::map<std::pair<std::string, std::string>, tao::json::value> controls_;
};
class PluginSettingsState
{
public:
    const tao::json::value* ResetControl(const tao::json::value& spec, const PluginSettingsSnapshot& current);
    bool Collect(tao::json::value& values, const tao::json::value& spec, int value, const PluginSettingsSnapshot& current) const;

private:
    std::map<std::pair<std::string, std::string>, int> baseline_;
};
