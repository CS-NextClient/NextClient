#include "PluginText.h"

const char* PluginPermissionToken(const std::string& permission)
{
    if (permission == "ui.settings")
        return "#NextPlugins_PermissionSettings";
    if (permission == "ui.draw")
        return "#NextPlugins_PermissionDraw";
    if (permission == "ui.hide")
        return "#NextPlugins_PermissionHide";
    if (permission == "player.write")
        return "#NextPlugins_PermissionPlayer";
    if (permission == "cvars.read")
        return "#NextPlugins_PermissionCvarRead";
    if (permission == "cvars.write")
        return "#NextPlugins_PermissionCvarWrite";
    if (permission == "audio.play")
        return "#NextPlugins_PermissionAudio";
    if (permission == "cvars.create")
        return "#NextPlugins_PermissionCvarCreate";
    if (permission == "chat.read")
        return "#NextPlugins_PermissionChatRead";
    if (permission == "chat.send")
        return "#NextPlugins_PermissionChatSend";
    if (permission == "chat.print")
        return "#NextPlugins_PermissionChatPrint";
    if (permission == "connection.connect")
        return "#NextPlugins_PermissionConnect";
    if (permission == "connection.disconnect")
        return "#NextPlugins_PermissionDisconnect";
    if (permission == "messages.read")
        return "#NextPlugins_PermissionMessageRead";
    if (permission == "messages.filter")
        return "#NextPlugins_PermissionMessageFilter";
    if (permission == "ui.windows")
        return "#NextPlugins_PermissionWindows";
    if (permission == "ui.input")
        return "#NextPlugins_PermissionInput";
    if (permission == "services.call")
        return "#NextPlugins_PermissionServices";
    return "#NextPlugins_ErrorPermission";
}
std::string PluginPermissionText(const tao::json::value& plugin, const PluginTokenLookup& lookup)
{
    std::string result = lookup("#NextPlugins_RequiredPermissions");
    const auto& permissions = plugin.at("permissions").get_array();
    if (permissions.empty())
        result += "\n" + lookup("#NextPlugins_PermissionNone");
    for (const auto& permission : permissions)
        result += "\n- " + lookup(PluginPermissionToken(permission.get_string()));
    return result;
}
std::string PluginConsentText(const tao::json::value& plugin, const std::string& language, const PluginTokenLookup& lookup)
{
    return PluginMetadataText(plugin, "name", language) + "\n" + lookup("#NextPlugins_Author") + ": " + plugin.at("author").get_string() +
           "\n\n" + PluginPermissionText(plugin, lookup) + "\n\n" + lookup("#NextPlugins_Trust");
}
bool AcceptPluginConsent(tao::json::value& plugins, const std::string& file, const std::string& hash)
{
    for (auto& plugin : plugins.get_array())
        if (plugin.at("file") == file && plugin.at("hash") == hash && plugin.at("error") == "")
        {
            plugin["consent"] = true;
            plugin["enabled"] = true;
            return true;
        }
    return false;
}

namespace
{
    const tao::json::value* Translation(const tao::json::value& translations, const std::string& language, const char* field = nullptr)
    {
        for (const auto& tag : {language, language.substr(0, language.find('-'))})
        {
            const auto* translated = translations.find(tag);
            if (translated && field)
                translated = translated->find(field);
            if (translated && !translated->get_string().empty())
                return translated;
        }
        return nullptr;
    }
} // namespace

std::string PluginMetadataText(const tao::json::value& plugin, const char* field, const std::string& language)
{
    if (auto translations = plugin.find("translations"))
        if (auto translated = Translation(*translations, language, field))
            return translated->get_string();
    return plugin.at(field).get_string();
}

std::string RenderPluginDiagnostic(const tao::json::value& value, const std::string& language, const PluginTokenLookup& lookup)
{
    if (value.is_string())
        return value.get_string();
    if (value.is_array())
    {
        std::string result;
        for (const auto& item : value.get_array())
        {
            if (!result.empty())
                result += '\n';
            result += RenderPluginDiagnostic(item, language, lookup);
        }
        return result;
    }
    if (auto text = value.find("text"))
    {
        if (auto translations = value.find("translations"))
            if (auto translated = Translation(*translations, language))
                return translated->get_string();
        return text->get_string();
    }
    const auto format = lookup(value.at("token").get_string());
    const auto& args = value.at("args").get_array();
    std::string result;
    // Scan only the template: plugin text containing %s1 stays literal.
    for (size_t i = 0; i < format.size();)
    {
        if (i + 2 < format.size() && format[i] == '%' && format[i + 1] == 's' && format[i + 2] >= '1' && format[i + 2] <= '9')
        {
            const size_t index = static_cast<size_t>(format[i + 2] - '1');
            if (index < args.size())
            {
                result += RenderPluginDiagnostic(args[index], language, lookup);
                i += 3;
                continue;
            }
        }
        result += format[i++];
    }
    return result;
}
