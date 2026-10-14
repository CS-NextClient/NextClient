#pragma once
#include <tao/json.hpp>
#include <string>

std::wstring PluginWide(const std::string& text);
std::wstring PluginToken(const char* token);
std::string PluginTokenText(const char* token);
std::string PluginLanguage();
std::string PluginDiagnostic(const std::string& diagnostic);
std::string PluginLocalized(const tao::json::value& object, const char* en = "en", const char* ru = "ru");
std::string PluginTitle(const std::string& id, const std::string& text);
