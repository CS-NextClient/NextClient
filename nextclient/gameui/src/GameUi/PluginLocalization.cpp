#include "PluginLocalization.h"
#include "PluginText.h"
#include <vgui/ILocalize.h>
#include <nitro_utils/string_utils.h>
#include <tier2/tier2.h>

std::wstring PluginWide(const std::string& text)
{
    return nitro_utils::utf8_to_wide(text);
}
std::string PluginLanguage()
{
    return PluginTokenText("#NextPlugins_Language");
}
std::string PluginTokenText(const char* token)
{
    return nitro_utils::wide_to_utf8(PluginToken(token));
}
std::string PluginDiagnostic(const std::string& diagnostic)
{
    if (diagnostic.empty())
        return {};
    try
    {
        return RenderPluginDiagnostic(tao::json::from_string(diagnostic), PluginLanguage(), [](const std::string& token) {
            return PluginTokenText(token.c_str());
        });
    }
    catch (...)
    {
        return PluginTokenText("#NextPlugins_ErrorUnexpected");
    }
}
std::wstring PluginToken(const char* token)
{
    const auto* s = g_pVGuiLocalize->Find(token);
    return s ? s : PluginWide(token);
}
std::string PluginLocalized(const tao::json::value& v, const char* en, const char* ru)
{
    bool russian = PluginLanguage() == "ru";
    auto translated = v.at(russian ? ru : en).get_string();
    return translated.empty() ? v.at(en).get_string() : translated;
}
std::string PluginTitle(const std::string& id, const std::string& text)
{
    auto token = "NextPlugin." + id;
    auto wide = PluginWide(text);
    g_pVGuiLocalize->AddString(token.c_str(), wide.data(), "nextclient_plugins");
    return "#" + token;
}
