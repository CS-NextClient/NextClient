#pragma once
#include <tao/json.hpp>
#include <functional>
#include <string>

using PluginTokenLookup = std::function<std::string(const std::string&)>;

std::string PluginMetadataText(const tao::json::value& plugin, const char* field, const std::string& language);
std::string RenderPluginDiagnostic(const tao::json::value& value, const std::string& language, const PluginTokenLookup& lookup);
const char* PluginPermissionToken(const std::string& permission);
std::string PluginPermissionText(const tao::json::value& plugin, const PluginTokenLookup& lookup);
std::string PluginConsentText(const tao::json::value& plugin, const std::string& language, const PluginTokenLookup& lookup);
bool AcceptPluginConsent(tao::json::value& plugins, const std::string& file, const std::string& hash);
