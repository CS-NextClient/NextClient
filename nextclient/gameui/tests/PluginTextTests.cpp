#include <gtest/gtest.h>
#include "GameUi/PluginText.h"
#include <fstream>
#include <map>
#include <nitro_utils/string_utils.h>
#include <regex>

namespace
{
    std::map<std::string, std::string> ReadTokens(const char* language)
    {
        const auto path = std::string(NEXTCLIENT_ASSETS_DIR) + "/cstrike/resource/nextclient_" + language + ".txt";
        std::ifstream stream(path, std::ios::binary);
        std::string bytes{std::istreambuf_iterator<char>(stream), {}};
        std::wstring wide(reinterpret_cast<const wchar_t*>(bytes.data()), bytes.size() / sizeof(wchar_t));
        const auto source = nitro_utils::wide_to_utf8(wide);
        const std::regex token(R"token("(NextPlugins_[^"]+)"\s+"([^"]*)")token");
        std::map<std::string, std::string> tokens;
        for (std::sregex_iterator it(source.begin(), source.end(), token), end; it != end; ++it)
            tokens["#" + (*it)[1].str()] = (*it)[2].str();
        return tokens;
    }
} // namespace

TEST(PluginText, MetadataUsesLanguageThenBaseLanguageThenOriginalField)
{
    auto plugin = tao::json::from_string(R"({"name":"Movement","description":"Original description",
        "translations":{"ru":{"name":"Движение"},"ru-ru":{"description":"Описание"}}})");
    EXPECT_EQ(PluginMetadataText(plugin, "name", "ru"), "Движение");
    EXPECT_EQ(PluginMetadataText(plugin, "name", "ru-ru"), "Движение");
    EXPECT_EQ(PluginMetadataText(plugin, "description", "ru-ru"), "Описание");
    EXPECT_EQ(PluginMetadataText(plugin, "description", "ru"), "Original description");
    EXPECT_EQ(PluginMetadataText(plugin, "name", "de"), "Movement");
    plugin["translations"]["ru"]["name"] = "";
    EXPECT_EQ(PluginMetadataText(plugin, "name", "ru"), "Movement");
    plugin.erase("translations");
    EXPECT_EQ(PluginMetadataText(plugin, "name", "ru"), "Movement");
}

TEST(PluginText, DiagnosticsLocalizeNestedNamesAndPreserveLiteralPluginText)
{
    auto diagnostic = tao::json::from_string(R"({"token":"#NextPlugins_DiagnosticContext","args":[
        {"text":"Movement","translations":{"ru":"Движение"}},
        {"token":"#NextPlugins_ErrorConflict","args":["Example %s2", "Reason %s1"]}]})");
    const auto russian = ReadTokens("russian");
    const auto english = ReadTokens("english");
    EXPECT_EQ(
        RenderPluginDiagnostic(diagnostic, "ru", [&](const auto& token) { return russian.at(token); }),
        "Движение: Конфликтует с Example %s2: Reason %s1"
    );
    EXPECT_EQ(
        RenderPluginDiagnostic(diagnostic, "en", [&](const auto& token) { return english.at(token); }),
        "Movement: Conflicts with Example %s2: Reason %s1"
    );
}

TEST(PluginText, MultipleDiagnosticsUseTranslatedResources)
{
    auto diagnostic = tao::json::from_string(R"([
        {"token":"#NextPlugins_SdkMismatch","args":["1.1.0","1.0.0"]},
        {"token":"#NextPlugins_CallbackFailed","args":[]}])");
    for (const auto* language : {"english", "russian"})
    {
        const auto tokens = ReadTokens(language);
        const auto text = RenderPluginDiagnostic(diagnostic, language, [&](const auto& token) { return tokens.at(token); });
        EXPECT_NE(text.find("1.1.0"), std::string::npos);
        EXPECT_NE(text.find('\n'), std::string::npos);
        EXPECT_EQ(text.find("%s"), std::string::npos);
        EXPECT_NE(text.find(tokens.at("#NextPlugins_CallbackFailed")), std::string::npos);
    }
}

TEST(PluginText, ConsentShowsLocalizedIdentityAndEveryRequiredPermission)
{
    const auto plugin = tao::json::from_string(R"({"name":"Movement","author":"Example author","description":"Test",
        "translations":{"ru":{"name":"Движение"}},
        "permissions":["ui.settings","ui.draw","ui.hide","player.write","cvars.read","cvars.write","audio.play",
            "cvars.create","chat.read","chat.send","connection.connect","connection.disconnect","chat.print",
            "messages.read","messages.filter","ui.windows","ui.input","services.call"]})");
    for (const auto* language : {"english", "russian"})
    {
        const auto tokens = ReadTokens(language);
        const auto text = PluginConsentText(plugin, language == std::string("russian") ? "ru" : "en", [&](const auto& token) {
            return tokens.at(token);
        });
        EXPECT_NE(text.find(language == std::string("russian") ? "Движение" : "Movement"), std::string::npos);
        EXPECT_NE(text.find("Example author"), std::string::npos);
        for (const auto& permission : plugin.at("permissions").get_array())
            EXPECT_NE(text.find(tokens.at(PluginPermissionToken(permission.get_string()))), std::string::npos);
    }
}
TEST(PluginText, ConsentOnlyEnablesTheExactReviewedBinary)
{
    auto rows = tao::json::from_string(R"([{"file":"example.dll","hash":"new","error":"","consent":false,"enabled":false}])");
    EXPECT_FALSE(AcceptPluginConsent(rows, "example.dll", "old"));
    EXPECT_FALSE(rows.at(0).at("enabled").get_boolean());
    EXPECT_TRUE(AcceptPluginConsent(rows, "example.dll", "new"));
    EXPECT_TRUE(rows.at(0).at("enabled").get_boolean());
    EXPECT_TRUE(rows.at(0).at("consent").get_boolean());
    rows.at(0)["enabled"] = false;
    rows.at(0)["error"] = "blocked";
    EXPECT_FALSE(AcceptPluginConsent(rows, "example.dll", "new"));
    EXPECT_FALSE(rows.at(0).at("enabled").get_boolean());
}
