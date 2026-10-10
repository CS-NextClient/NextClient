#pragma once

#include <optional>
#include <string_view>

#include <tao/json.hpp>

namespace vgui2
{
    class Panel;
}

const char* PluginControls_SettingKind(unsigned kind);
tao::json::value PluginControls_SettingsSpec(const tao::json::value& spec);
vgui2::Panel* PluginControls_Create(vgui2::Panel* parent, const tao::json::value& spec, vgui2::Panel* target, const char* command);
void PluginControls_SetValue(vgui2::Panel* widget, std::string_view kind, const tao::json::value& value);
std::optional<tao::json::value> PluginControls_Read(vgui2::Panel* widget, std::string_view kind);
