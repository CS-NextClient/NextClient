#include "PluginSettingsState.h"
#include "catalog.h"
#include <nextclient/runtime.h>
#include <gtest/gtest.h>
#include <windows.h>
#include <fstream>

using namespace plugins;
static_assert(sizeof(NcHost) == 156);
static_assert(sizeof(NcExtension) == 28);
static_assert(sizeof(NcPlugin) == 48);
static_assert(sizeof(NcSession) == 156);
static_assert(sizeof(NcPlayerInfo) == 280);
namespace fs = std::filesystem;
static Item item(const char* id)
{
    Item i;
    i.file = std::string(id) + ".dll";
    i.enabled = true;
    i.manifest.id = id;
    i.manifest.name = id;
    i.manifest.version = "1.2.3";
    return i;
}
TEST(PluginVersions, ComparisonRanges)
{
    EXPECT_TRUE(matches("1.2.3", ">=1.0.0 <2.0.0"));
    EXPECT_FALSE(matches("2.0.0", ">=1.0.0 <2.0.0"));
    EXPECT_TRUE(matches("1.2.3", "1.2.3"));
    EXPECT_TRUE(matches("1.2.3", "*"));
    EXPECT_THROW(matches("1.0.0", "^1.0.0"), std::exception);
    EXPECT_THROW(matches("1.0.0", "1.0"), std::exception);
    EXPECT_THROW(matches("01.0.0", "*"), std::exception);
}
TEST(PluginOrder, StableRecommendationAndHardDependency)
{
    auto a = item("a"), b = item("b"), c = item("c");
    a.manifest.required.push_back({"b", ">=1.0.0", "API provider"});
    EXPECT_FALSE(validate({a, b, c}).empty());
    EXPECT_TRUE(validate({b, a, c}).empty());
    std::string warning;
    EXPECT_EQ(recommend({a, b, c}, warning), (std::vector<size_t>{1, 0, 2}));
    EXPECT_TRUE(warning.empty());
}
TEST(PluginOrder, MissingDisabledAndWrongVersionDependenciesBlock)
{
    auto a = item("a"), b = item("b");
    a.manifest.required.push_back({"b", ">=2.0.0", "API"});
    EXPECT_FALSE(validate({b, a}).empty());
    b.enabled = false;
    EXPECT_FALSE(validate({b, a}).empty());
    EXPECT_FALSE(validate({a}).empty());
}
TEST(PluginOrder, ConflictAndDuplicateBlock)
{
    auto a = item("a"), b = item("b");
    a.manifest.conflicts.push_back({"b", "*", "incompatible"});
    EXPECT_FALSE(validate({a, b}).empty());
    b.enabled = false;
    EXPECT_TRUE(validate({a, b}).empty());
    b.manifest.id = "a";
    EXPECT_FALSE(validate({a, b}).empty());
}
TEST(PluginOrder, VersionScopedAdviceDoesNotBlockManualOrder)
{
    auto a = item("a"), b = item("b");
    a.manifest.after.push_back({"b", "<2.0.0", "compatibility"});
    EXPECT_TRUE(validate({a, b}).empty());
    EXPECT_FALSE(ordering_warnings({a, b}).empty());
    b.manifest.version = "2.0.0";
    EXPECT_TRUE(ordering_warnings({a, b}).empty());
}
TEST(PluginOrder, CyclesKeepOrderAndExplain)
{
    auto a = item("a"), b = item("b");
    a.manifest.after.push_back({"b", "*", "a"});
    b.manifest.after.push_back({"a", "*", "b"});
    std::string warning;
    EXPECT_EQ(recommend({a, b}, warning), (std::vector<size_t>{0, 1}));
    EXPECT_FALSE(warning.empty());
    a.manifest.required.push_back({"b", "*", "a"});
    b.manifest.required.push_back({"a", "*", "b"});
    EXPECT_FALSE(validate({a, b}).empty());
}

TEST(PluginOrder, UserFacingWarningsUseNamesInsteadOfInternalIds)
{
    auto a = item("internal.first"), b = item("internal.second");
    a.manifest.name = "First plugin";
    b.manifest.name = "Second plugin";
    a.manifest.after.push_back({b.manifest.id, "*", "Compatibility advice"});
    auto warning = ordering_warnings({a, b});
    EXPECT_NE(warning.find("Second plugin"), std::string::npos);
    EXPECT_EQ(warning.find("internal.second"), std::string::npos);
    a.manifest.required.push_back({b.manifest.id, "*", "Required behavior"});
    auto error = validate({a, b});
    EXPECT_NE(error.find("Second plugin"), std::string::npos);
    EXPECT_EQ(error.find(": internal.second"), std::string::npos);
}
TEST(PluginMetadata, RejectMalformedPeAndDeepJson)
{
    EXPECT_THROW(pe_manifest({}), std::exception);
    EXPECT_THROW(pe_manifest(std::vector<unsigned char>(512, 0)), std::exception);
    EXPECT_THROW(parse(std::string(30, '[') + "0" + std::string(30, ']')), std::exception);
}
TEST(PluginMetadata, IntegerBoundsRejectUnsignedValuesOutsideSignedRange)
{
    EXPECT_EQ(integer(parse("-100"), -100, -1), -100);
    EXPECT_EQ(integer(parse("-1"), -100, -1), -1);
    EXPECT_THROW(integer(parse("0"), -100, -1), std::exception);
    EXPECT_THROW(integer(parse("18446744073709551615"), -100, -1), std::exception);
    EXPECT_THROW(integer(parse("9223372036854775808"), INT64_MIN, INT64_MAX), std::exception);
    EXPECT_EQ(integer(parse("9223372036854775807"), INT64_MIN, INT64_MAX), INT64_MAX);
    EXPECT_EQ(integer(parse("0"), -1, 0), 0);
}
TEST(PluginMetadata, HashKnownVector)
{
    EXPECT_EQ(sha256({'a', 'b', 'c'}), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(PluginMetadata, RejectUnsupportedAndOverflowedAbiBeforeLoading)
{
    auto valid = parse(
        R"({"schema":1,"id":"test.metadata","name":"Test","author":"Tests","description":"Fixture","version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,"compatibility_revision":1})"
    );
    auto incompatible = valid;
    incompatible["abi"] = 3;
    EXPECT_THROW(manifest(tao::json::to_string(incompatible)), std::exception);
    incompatible["abi"] = UINT64_C(4294967297);
    EXPECT_THROW(manifest(tao::json::to_string(incompatible)), std::exception);
    incompatible = valid;
    incompatible["api"] = 3;
    EXPECT_THROW(manifest(tao::json::to_string(incompatible)), std::exception);
    incompatible = valid;
    incompatible["capabilities"] = Json::array({"unsupported"});
    EXPECT_THROW(manifest(tao::json::to_string(incompatible)), std::exception);
}

TEST(PluginMetadata, TranslationsAreOptionalAndValidated)
{
    auto metadata = parse(R"({"schema":1,"id":"test.metadata","name":"Test","author":"Tests",
        "description":"Fixture","version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,"compatibility_revision":1})");
    EXPECT_TRUE(manifest(tao::json::to_string(metadata)).translations.get_object().empty());
    metadata["translations"] = parse(R"({"ru":{"name":"Тест","description":"Описание"}})");
    auto translated = manifest(tao::json::to_string(metadata));
    EXPECT_EQ(translated.name, "Test");
    EXPECT_EQ(translated.translations.at("ru").at("name"), "Тест");
    metadata["translations"]["ru"]["name"] = std::string(129, 'a');
    EXPECT_THROW(manifest(tao::json::to_string(metadata)), std::exception);
    metadata["translations"] = parse(R"({"RU":{"name":"Test"}})");
    EXPECT_THROW(manifest(tao::json::to_string(metadata)), std::exception);
    metadata["translations"] = parse(R"({"ru":{"description":42}})");
    EXPECT_THROW(manifest(tao::json::to_string(metadata)), std::exception);
}

class Runtime : public ::testing::Test
{
protected:
    fs::path dir;
    void SetUp() override
    {
        dir = fs::temp_directory_path() /
              (L"nextclient-plugin-tests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        fs::create_directories(dir / L"plugins");
        fs::copy_file(PLUGIN_FIXTURE_PATH, dir / L"plugins" / L"settings.dll");
        nc_runtime_start(dir.c_str(), 0);
    }
    void TearDown() override
    {
        nc_runtime_stop();
        nc_runtime_bind_client(nullptr);
        fs::remove_all(dir);
        for (auto key :
             {L"NEXTCLIENT_PROBE_MARKER", L"NEXTCLIENT_PROBE_FAIL_LOAD", L"NEXTCLIENT_PROBE_FAIL_COMMAND", L"NEXTCLIENT_PROBE_SESSION"})
            SetEnvironmentVariableW(key, nullptr);
        SetEnvironmentVariableW(L"NEXTCLIENT_PERMISSIONS_FAIL", nullptr);
        SetEnvironmentVariableW(L"NEXTCLIENT_FRAME_FAIL", nullptr);
    }
    Json catalog()
    {
        return parse(nc_runtime_catalog()).at("plugins");
    }
    void enable()
    {
        auto rows = catalog();
        ASSERT_EQ(rows.get_array().size(), 1u);
        ASSERT_EQ(rows.at(0).at("error"), "");
        rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
        ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    }
    void restart(int safe = 0)
    {
        nc_runtime_stop();
        nc_runtime_start(dir.c_str(), safe);
    }
    NcCommand move(uint32_t flags)
    {
        NcCommand c{sizeof(NcCommand), 3, {10, 20, 30}, 10, 20, 30};
        NcPlayer p{sizeof(NcPlayer), flags, 0.01f};
        nc_runtime_command(&c, &p);
        return c;
    }
};
TEST_F(Runtime, DiscoveryDoesNotLoadAndSaveRequiresRestart)
{
    auto rows = catalog();
    EXPECT_FALSE(rows.at(0).at("approved").get_boolean());
    EXPECT_FALSE(rows.at(0).at("running").get_boolean());
    EXPECT_EQ(parse(nc_runtime_ui()).get_array().size(), 0u);
    enable();
    EXPECT_EQ(parse(nc_runtime_ui()).get_array().size(), 0u);
    restart();
    EXPECT_TRUE(catalog().at(0).at("running").get_boolean());
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("tabs").at(0).at("id"), "tools");
}

TEST_F(Runtime, PluginSettingsNeedNoPermissionsAndPersistWithLocalizedMetadata)
{
    nc_runtime_stop();
    fs::copy_file(PLUGIN_SHARED_SETTINGS_PATH, dir / L"plugins" / L"settings.dll", fs::copy_options::overwrite_existing);
    nc_runtime_start(dir.c_str(), 0);
    enable();
    restart();
    const auto ui = parse(nc_runtime_ui());
    ASSERT_EQ(ui.get_array().size(), 1u);
    EXPECT_TRUE(ui.at(0).at("tabs").get_array().empty());
    EXPECT_TRUE(catalog().at(0).at("permissions").get_array().empty());
    ASSERT_EQ(ui.at(0).at("controls").get_array().size(), 4u);
    for (const auto& control : ui.at(0).at("controls").get_array())
    {
        EXPECT_EQ(control.at("tab"), NC_PLUGIN_SETTINGS_TAB);
    }
    EXPECT_EQ(ui.at(0).at("translations").at("ru").at("name"), "Настройки");
    const auto late =
        reinterpret_cast<int32_t(NC_CALL*)()>(GetProcAddress(GetModuleHandleW(L"settings.dll"), "nc_register_late_shared_control"));
    ASSERT_NE(late, nullptr);
    EXPECT_EQ(late(), 0);
    ASSERT_STREQ(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":1}])"), "");
    EXPECT_EQ(move(NC_PLAYER_VALID).buttons, 3u);
    restart();
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);
    nc_runtime_action("test.settings", "reset");
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 0);
}

TEST_F(Runtime, PluginSettingsAreScopedToTheirOwnerWithoutPermissions)
{
    nc_runtime_stop();
    fs::copy_file(PLUGIN_SHARED_SETTINGS_PATH, dir / L"plugins" / L"settings.dll", fs::copy_options::overwrite_existing);
    fs::copy_file(PLUGIN_OTHER_SETTINGS_PATH, dir / L"plugins" / L"other-settings.dll");
    nc_runtime_start(dir.c_str(), 0);
    auto rows = catalog();
    ASSERT_EQ(rows.get_array().size(), 2u);
    for (auto& row : rows.get_array())
    {
        EXPECT_TRUE(row.at("permissions").get_array().empty());
        row["enabled"] = row["consent"] = true;
    }
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    restart();
    const Json running = catalog();
    for (const auto& row : running.get_array())
    {
        ASSERT_TRUE(row.at("running").get_boolean());
    }
    const auto read = [](const wchar_t* module, const char* id, int32_t fallback) {
        const auto function =
            reinterpret_cast<int32_t(NC_CALL*)(const char*, int32_t)>(GetProcAddress(GetModuleHandleW(module), "nc_read_own_setting"));
        EXPECT_NE(function, nullptr);
        return function ? function(id, fallback) : INT32_MIN;
    };
    const auto write = [](const wchar_t* module, const char* id, int32_t value) {
        const auto function =
            reinterpret_cast<int32_t(NC_CALL*)(const char*, int32_t)>(GetProcAddress(GetModuleHandleW(module), "nc_write_own_setting"));
        EXPECT_NE(function, nullptr);
        return function ? function(id, value) : -1;
    };
    EXPECT_EQ(read(L"settings.dll", "enabled", -1), 0);
    EXPECT_EQ(read(L"other-settings.dll", "enabled", -1), 0);
    EXPECT_EQ(read(L"settings.dll", "private", -1), -1);
    EXPECT_EQ(read(L"other-settings.dll", "private", -1), 7);
    EXPECT_EQ(write(L"settings.dll", "private", 0), 0);
    EXPECT_EQ(read(L"settings.dll", "test.settings.other.enabled", -1), -1);
    EXPECT_EQ(write(L"settings.dll", "test.settings.other.enabled", 1), 0);
    ASSERT_STREQ(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":1}])"), "");
    EXPECT_EQ(read(L"settings.dll", "enabled", -1), 1);
    EXPECT_EQ(read(L"other-settings.dll", "enabled", -1), 0);
    EXPECT_EQ(write(L"other-settings.dll", "enabled", 1), 1);
    EXPECT_EQ(write(L"settings.dll", "enabled", 0), 1);
    EXPECT_EQ(read(L"settings.dll", "enabled", -1), 0);
    EXPECT_EQ(read(L"other-settings.dll", "enabled", -1), 1);
    restart();
    EXPECT_EQ(read(L"settings.dll", "enabled", -1), 0);
    EXPECT_EQ(read(L"other-settings.dll", "enabled", -1), 1);
    EXPECT_EQ(read(L"settings.dll", "private", -1), -1);
    EXPECT_EQ(read(L"other-settings.dll", "private", -1), 7);
}

TEST_F(Runtime, ExistingCustomPluginsTabStillRegisters)
{
    nc_runtime_stop();
    fs::copy_file(PLUGIN_CUSTOM_SETTINGS_PATH, dir / L"plugins" / L"settings.dll", fs::copy_options::overwrite_existing);
    nc_runtime_start(dir.c_str(), 0);
    enable();
    restart();
    const auto ui = parse(nc_runtime_ui());
    ASSERT_EQ(ui.get_array().size(), 1u);
    EXPECT_EQ(ui.at(0).at("tabs").at(0).at("id"), "plugins");
    EXPECT_EQ(ui.at(0).at("controls").at(0).at("tab"), "plugins");
    ASSERT_EQ(ui.at(0).at("controls").get_array().size(), 2u);
    EXPECT_EQ(ui.at(0).at("controls").at(1).at("tab"), NC_PLUGIN_SETTINGS_TAB);
}

TEST_F(Runtime, CatalogIncludesTranslationsWithoutLoadingThePlugin)
{
    const auto rows = catalog();
    EXPECT_FALSE(rows.at(0).at("running").get_boolean());
    EXPECT_EQ(rows.at(0).at("name"), "Settings fixture");
    EXPECT_EQ(rows.at(0).at("translations").at("ru").at("name"), "Настройки");
    EXPECT_FALSE(rows.at(0).at("translations").at("ru").at("description").get_string().empty());
}
TEST_F(Runtime, FixtureSettingsPersistenceAndMovement)
{
    enable();
    restart();
    constexpr uint32_t flags = NC_PLAYER_VALID | NC_PLAYER_ACTIVE | NC_PLAYER_CAN_JUMP;
    EXPECT_EQ(move(flags).buttons, 3u);
    ASSERT_STREQ(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":1}])"), "");
    EXPECT_EQ(move(flags).buttons, 1u);
    EXPECT_EQ(move(flags | NC_PLAYER_GROUNDED).buttons, 1u);
    EXPECT_EQ(move(flags | NC_PLAYER_GROUNDED | NC_PLAYER_JUMP_HELD).buttons, 1u);
    EXPECT_EQ(move(flags & ~NC_PLAYER_ACTIVE).buttons, 1u);
    EXPECT_EQ(move(flags & ~NC_PLAYER_CAN_JUMP).buttons, 1u);
    EXPECT_EQ(move(0).buttons, 3u);
    EXPECT_EQ(move(flags).forward_move, 10);
    EXPECT_EQ(move(flags).view_angles[2], 30);
    restart();
    EXPECT_EQ(move(flags).buttons, 1u);
}
TEST_F(Runtime, SafeModeSkipsApprovedPlugins)
{
    enable();
    restart(1);
    EXPECT_EQ(parse(nc_runtime_ui()).get_array().size(), 0u);
    EXPECT_TRUE(catalog().at(0).at("enabled").get_boolean());
}

TEST_F(Runtime, OversizedLegacySettingsMigrateWithoutDisablingPlugins)
{
    enable();
    nc_runtime_stop();
    const auto legacy = dir / L"plugins" / L"settings.json";
    std::string data = R"({"test.settings":{"enabled":1},"broken":{"value":"invalid"}})";
    data.resize(1048600, ' ');
    {
        std::ofstream file(legacy, std::ios::binary);
        file << data;
    }
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    EXPECT_TRUE(catalog().at(0).at("running").get_boolean());
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);
    EXPECT_EQ(fs::file_size(dir / L"plugins" / L"settings.json.migrated.bak"), 1048600u);
    ASSERT_STREQ(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":0}])"), "");
    restart();
    EXPECT_TRUE(catalog().at(0).at("running").get_boolean());
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 0);
}

TEST_F(Runtime, CorruptOwnerSettingsRecoverFromBackupWithoutSafeMode)
{
    enable();
    restart();
    ASSERT_STREQ(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":1}])"), "");
    ASSERT_STREQ(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":0}])"), "");
    nc_runtime_stop();
    const auto file = dir / L"plugins" / L".host" / L"settings" / L"plugin-test.settings.json";
    {
        std::ofstream stream(file);
        stream << R"({"enabled":"not an integer"})";
    }
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    EXPECT_TRUE(catalog().at(0).at("running").get_boolean());
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);
    ASSERT_STREQ(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":0}])"), "");
    EXPECT_TRUE(fs::exists(file.wstring() + L".damaged"));
}

TEST_F(Runtime, UncleanSessionDefaultsToSafeModeAndDisablingAllowsNormalRestart)
{
    enable();
    restart();
    nc_runtime_stop();
    {
        std::ofstream stream(dir / L"plugins" / L"session.json");
        stream << R"({"phase":"initializing","suspected":"test.settings"})";
    }
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_TRUE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    EXPECT_FALSE(catalog().at(0).at("running").get_boolean());
    EXPECT_TRUE(fs::exists(dir / L"plugins" / L"session.previous.json"));
    auto rows = catalog();
    rows.at(0)["enabled"] = false;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    EXPECT_FALSE(fs::exists(dir / L"plugins" / L"session.json"));
    restart();
    EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    EXPECT_FALSE(catalog().at(0).at("enabled").get_boolean());
    enable();
    restart();
    EXPECT_TRUE(catalog().at(0).at("running").get_boolean());
}

TEST_F(Runtime, LegacyCheckpointHintDoesNotPreventLoadingCurrentSettings)
{
    enable();
    restart();
    ASSERT_STREQ(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":1}])"), "");
    nc_runtime_stop();
    const auto file = dir / L"plugins" / L"profile.json";
    Json saved;
    {
        std::ifstream stream(file);
        saved = parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    }
    saved["settings_slot"] = "a";
    {
        std::ofstream stream(file);
        stream << tao::json::to_string(saved);
    }
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    EXPECT_TRUE(catalog().at(0).at("running").get_boolean());
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);
}

TEST_F(Runtime, CrashContextRecordsMetadataAndMarkerOutlivesDllDestruction)
{
    fs::copy_file(PLUGIN_PROBE_PATH, dir / L"plugins" / L"probe.dll");
    auto rows = catalog();
    for (auto& row : rows.get_array())
        row["enabled"] = row["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    restart();
    auto marker = dir / L"plugins" / L"session.json";
    SetEnvironmentVariableW(L"NEXTCLIENT_PROBE_SESSION", marker.c_str());
    {
        std::ifstream stream(marker);
        auto session = parse(std::string(std::istreambuf_iterator<char>(stream), {}));
        EXPECT_EQ(session.at("phase"), "running");
        ASSERT_EQ(session.at("plugins").get_array().size(), 2u);
        for (size_t i = 0; i < 2; ++i)
        {
            EXPECT_EQ(session.at("plugins").at(i).at("id"), rows.at(i).at("id"));
            EXPECT_EQ(session.at("plugins").at(i).at("version"), rows.at(i).at("version"));
            EXPECT_EQ(session.at("plugins").at(i).at("hash"), rows.at(i).at("hash"));
            EXPECT_EQ(session.at("plugins").at(i).at("order"), i);
        }
    }
    move(0);
    {
        std::ifstream stream(dir / L"plugins" / L"session-trace.txt");
        const std::string trace(std::istreambuf_iterator<char>(stream), {});
        EXPECT_NE(trace.find("\"category\":\"command\""), std::string::npos);
        EXPECT_NE(trace.find("\"id\":\"test.probe\""), std::string::npos);
    }
    nc_runtime_stop();
    EXPECT_FALSE(fs::exists(marker));
    std::ifstream observed(marker.wstring() + L".observed");
    EXPECT_EQ(observed.get(), '1');
    const auto unload = parse(std::string(std::istreambuf_iterator<char>(observed), {}));
    EXPECT_EQ(unload.at("active"), true);
    EXPECT_EQ(unload.at("id"), "test.probe");
    EXPECT_EQ(unload.at("category"), "module_unload");
    SetEnvironmentVariableW(L"NEXTCLIENT_PROBE_SESSION", nullptr);
}

TEST_F(Runtime, UnicodeDllFilenameLoadsAfterApproval)
{
    nc_runtime_stop();
    fs::rename(dir / L"plugins" / L"settings.dll", dir / L"plugins" / L"\u0440\u0430\u0441\u043f\u0440\u044b\u0436\u043a\u0430.dll");
    nc_runtime_start(dir.c_str(), 0);
    enable();
    restart();
    EXPECT_TRUE(catalog().at(0).at("running").get_boolean());
}
TEST_F(Runtime, LongUnicodeDllFilenameRoundTripsProfileWithoutSafeMode)
{
    nc_runtime_stop();
    const auto filename = std::wstring(100, L'\u754c') + L".dll";
    fs::rename(dir / L"plugins" / L"settings.dll", dir / L"plugins" / filename);
    nc_runtime_start(dir.c_str(), 0);
    auto rows = catalog();
    ASSERT_EQ(rows.get_array().size(), 1u);
    EXPECT_GT(rows.at(0).at("file").get_string().size(), 256u);
    enable();
    restart();
    EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    rows = catalog();
    EXPECT_TRUE(rows.at(0).at("approved").get_boolean());
    EXPECT_TRUE(rows.at(0).at("running").get_boolean());
    rows.at(0)["enabled"] = false;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    restart();
    EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    EXPECT_FALSE(catalog().at(0).at("enabled").get_boolean());
}
TEST_F(Runtime, UnreadableApprovedPackageRetainsSavedIdentityAndCanBeDisabled)
{
    enable();
    auto before = catalog().at(0);
    nc_runtime_stop();
    {
        std::ofstream broken(dir / L"plugins" / L"settings.dll", std::ios::binary | std::ios::trunc);
        broken << "invalid DLL";
    }
    nc_runtime_start(dir.c_str(), 0);
    auto rows = catalog();
    ASSERT_EQ(rows.get_array().size(), 1u);
    EXPECT_NE(rows.at(0).at("error"), "");
    for (const auto* key : {"id", "hash", "approved", "enabled", "permissions"})
        EXPECT_EQ(rows.at(0).at(key), before.at(key)) << key;
    EXPECT_FALSE(rows.at(0).at("running").get_boolean());
    rows.at(0)["enabled"] = false;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    restart();
    EXPECT_FALSE(catalog().at(0).at("enabled").get_boolean());
}
TEST_F(Runtime, ThreeLoadedPluginsRetainCatalogAndCanBeDisabledUnderMemoryPressure)
{
    nc_runtime_stop();
    std::ifstream fixture(PLUGIN_GAME_ALL_PATH, std::ios::binary);
    std::string original{std::istreambuf_iterator<char>(fixture), {}};
    const auto identity = original.find("\"id\":\"test.game\"");
    ASSERT_NE(identity, std::string::npos);
    // Package rescans need more temporary memory than the protected control
    // reserve once ordinary plugin allocations approach the shared limit.
    original.resize(3 * 1024 * 1024, '\0');
    for (int i = 0; i < 3; ++i)
    {
        auto bytes = original;
        bytes.replace(identity + 6, 9, "test.p00" + std::to_string(i));
        std::ofstream file(dir / L"plugins" / ("pressure-" + std::to_string(i) + ".dll"), std::ios::binary);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    nc_runtime_start(dir.c_str(), 0);
    auto selected = catalog();
    ASSERT_EQ(selected.get_array().size(), 4u);
    for (auto& row : selected.get_array())
        row["enabled"] = row["consent"] = row.at("file") != "settings.dll";
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(selected).c_str()), "");
    restart();
    auto before = catalog();
    for (size_t i = 0; i < 3; ++i)
        ASSERT_TRUE(before.at(i).at("running").get_boolean());
    ASSERT_EQ(before.at(3).at("file"), "settings.dll");
    ASSERT_FALSE(before.at(3).at("running").get_boolean());
    size_t accepted{};
    for (int i = 0; i < 3; ++i)
    {
        const auto filename = L"pressure-" + std::to_wstring(i) + L".dll";
        const auto module = GetModuleHandleW(filename.c_str());
        ASSERT_NE(module, nullptr);
        const auto getter = reinterpret_cast<const NcHost*(NC_CALL*)()>(GetProcAddress(module, "nc_test_host"));
        ASSERT_NE(getter, nullptr);
        const auto* host = getter();
        ASSERT_NE(host, nullptr);
        ASSERT_TRUE(host->subscribe_event(host->context, "player.health", 1));
        const auto* services = host->query_interface(host->context, "nextclient.services", 1);
        ASSERT_NE(services, nullptr);
        const auto request =
            tao::json::to_string(Json{{"name", "test.p00" + std::to_string(i) + "/test"}, {"version", 1}, {"method", "echo"}, {"data", 0}});
        for (int n = 0; n < 64; ++n)
        {
            const auto handle = services->call(host->context, "request", request.c_str());
            ASSERT_NE(handle, 0u);
            const auto size = services->read_result(host->context, handle, nullptr, 0);
            ASSERT_GT(size, 0u);
            std::string result(size, '\0');
            ASSERT_EQ(services->read_result(host->context, handle, result.data(), size), size);
            services->release_result(host->context, handle);
            result.resize(size - 1);
            if (!parse(result).at("ok").get_boolean())
                break;
            ++accepted;
        }
    }
    ASSERT_GT(accepted, 64u);
    const auto payload = "\"" + std::string(60000, 'x') + "\"";
    for (int i = 0; i < 24; ++i)
        nc_runtime_event("player.health", payload.c_str());
    const auto catalog_under_pressure = parse(nc_runtime_catalog());
    auto rows = catalog_under_pressure.at("plugins");
    ASSERT_EQ(rows.get_array().size(), 4u);
    EXPECT_GT(catalog_under_pressure.at("resources").at("memory_rejections").as<uint64_t>(), 0u);
    for (size_t i = 0; i < 3; ++i)
    {
        for (const auto* key : {"file", "id", "hash", "approved", "enabled", "running", "permissions"})
            EXPECT_EQ(rows.at(i).at(key), before.at(i).at(key)) << key;
        // Cached packages need no large read buffer to retain valid metadata.
        EXPECT_EQ(rows.at(i).at("error"), "");
    }
    // New metadata-heavy packages can exceed even the protected catalog reserve.
    // The previous snapshot must survive, but cannot authorize stale nonloaded
    // packages while the required rescan is unavailable.
    auto metadata = parse(R"({"schema":1,"id":"test.extra","name":"Extra","author":"Tests",
        "description":"Fixture","version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,"compatibility_revision":1})");
    metadata["translations"] = tao::json::empty_object;
    for (int i = 0; i < 14; ++i)
        metadata["translations"]["language" + std::to_string(i)] = Json{{"description", std::string(4096, 'x')}};
    IMAGE_DOS_HEADER dos{};
    dos.e_magic = IMAGE_DOS_SIGNATURE;
    dos.e_lfanew = sizeof(dos);
    IMAGE_NT_HEADERS32 nt{};
    nt.Signature = IMAGE_NT_SIGNATURE;
    nt.FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt.FileHeader.Characteristics = IMAGE_FILE_DLL;
    nt.FileHeader.NumberOfSections = 1;
    nt.FileHeader.SizeOfOptionalHeader = sizeof(nt.OptionalHeader);
    nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    IMAGE_SECTION_HEADER section{};
    std::memcpy(section.Name, ".nclmeta", 8);
    section.PointerToRawData = 512;
    section.SizeOfRawData = 65536;
    std::string bytes(section.PointerToRawData + section.SizeOfRawData, '\0');
    std::memcpy(bytes.data(), &dos, sizeof(dos));
    std::memcpy(bytes.data() + dos.e_lfanew, &nt, sizeof(nt));
    std::memcpy(bytes.data() + dos.e_lfanew + sizeof(nt), &section, sizeof(section));
    for (int i = 0; i < 252; ++i)
    {
        metadata["id"] = "test.extra" + std::to_string(i);
        const auto json = tao::json::to_string(metadata);
        ASSERT_LT(json.size(), section.SizeOfRawData);
        std::memcpy(bytes.data() + section.PointerToRawData, json.c_str(), json.size() + 1);
        std::ofstream file(dir / L"plugins" / ("zz-extra-" + std::to_string(i) + ".dll"), std::ios::binary);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    {
        std::ofstream changed(dir / L"plugins" / L"settings.dll", std::ios::binary | std::ios::app);
        changed.put('\0');
    }
    const auto exhausted = parse(nc_runtime_catalog());
    EXPECT_NE(exhausted.at("error"), "");
    rows = exhausted.at("plugins");
    ASSERT_EQ(rows.get_array().size(), 4u);
    EXPECT_EQ(rows.at(3).at("hash"), before.at(3).at("hash"));
    EXPECT_NE(rows.at(3).at("error"), "");
    rows.at(3)["enabled"] = rows.at(3)["consent"] = true;
    EXPECT_STRNE(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    rows.at(3)["enabled"] = false;
    rows.at(0)["enabled"] = false;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    rows = catalog();
    EXPECT_FALSE(rows.at(0).at("enabled").get_boolean());
    for (size_t i = 0; i < 3; ++i)
        EXPECT_TRUE(rows.at(i).at("running").get_boolean());
    for (int i = 0; i < 252; ++i)
        fs::remove(dir / L"plugins" / ("zz-extra-" + std::to_string(i) + ".dll"));
    restart();
    rows = catalog();
    EXPECT_FALSE(rows.at(0).at("running").get_boolean());
    EXPECT_TRUE(rows.at(1).at("running").get_boolean());
    EXPECT_TRUE(rows.at(2).at("running").get_boolean());
}
TEST_F(Runtime, PackageAssetsAreLockedAndChangesRevokeWholePackageApproval)
{
    nc_runtime_stop();
    const auto folder = dir / L"plugins" / L"settings";
    fs::create_directory(folder);
    fs::rename(dir / L"plugins" / L"settings.dll", folder / L"plugin.dll");
    {
        std::ofstream asset(folder / L"data.txt");
        asset << "approved";
    }
    nc_runtime_start(dir.c_str(), 0);
    enable();
    restart();
    ASSERT_TRUE(catalog().at(0).at("running").get_boolean());
    const auto asset =
        CreateFileW((folder / L"data.txt").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    EXPECT_FALSE(MoveFileExW(folder.c_str(), (dir / L"plugins" / L"renamed-package").c_str(), 0));
    EXPECT_EQ(asset, INVALID_HANDLE_VALUE);
    if (asset != INVALID_HANDLE_VALUE)
        CloseHandle(asset);
    nc_runtime_stop();
    {
        std::ofstream changed(folder / L"data.txt");
        changed << "changed";
    }
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(catalog().at(0).at("approved").get_boolean());
    EXPECT_FALSE(catalog().at(0).at("running").get_boolean());
}
TEST_F(Runtime, ApprovedCompanionsLoadInDependencyOrderAndDiscoveryDoesNotExecuteThem)
{
    nc_runtime_stop();
    fs::remove(dir / L"plugins" / L"settings.dll");
    const auto folder = dir / L"plugins" / L"package";
    fs::create_directory(folder);
    fs::copy_file(PLUGIN_PACKAGE_PATH, folder / L"plugin.dll");
    fs::copy_file(PLUGIN_COMPANION_PATH, folder / L"plugin-companion.dll");
    const auto marker = dir / L"marker";
    SetEnvironmentVariableW(L"NEXTCLIENT_PROBE_MARKER", marker.c_str());
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(fs::exists(marker));
    enable();
    EXPECT_FALSE(fs::exists(marker));
    restart();
    EXPECT_TRUE(catalog().at(0).at("running").get_boolean());
    EXPECT_TRUE(fs::exists(marker));
    EXPECT_NE(GetModuleHandleW(L"plugin-companion.dll"), nullptr);
    nc_runtime_stop();
    EXPECT_EQ(GetModuleHandleW(L"plugin-companion.dll"), nullptr);
}
TEST_F(Runtime, MissingCompanionReportsItsNameAndTranslatableReason)
{
    nc_runtime_stop();
    fs::remove(dir / L"plugins" / L"settings.dll");
    const auto folder = dir / L"plugins" / L"package";
    fs::create_directory(folder);
    fs::copy_file(PLUGIN_PACKAGE_PATH, folder / L"plugin.dll");
    nc_runtime_start(dir.c_str(), 0);
    enable();
    restart();
    const auto state = parse(nc_runtime_catalog());
    const auto row = state.at("plugins").at(0);
    EXPECT_FALSE(row.at("running").get_boolean());
    const auto error = state.at("error").get_string();
    EXPECT_NE(error.find("#NextPlugins_ErrorSystemImport"), std::string::npos);
    EXPECT_NE(error.find("plugin-companion.dll"), std::string::npos);
    EXPECT_EQ(error.find("#NextPlugins_ErrorUnexpected"), std::string::npos);
}
TEST_F(Runtime, RecommendationWarningsDescribeTheReturnedOrder)
{
    nc_runtime_stop();
    fs::copy_file(PLUGIN_PROBE_PATH, dir / L"plugins" / L"z-probe.dll");
    // Reuse the metadata-only discovery fixture with a soft ordering rule.
    std::ifstream source(PLUGIN_DEPENDENT_PATH, std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(source), {});
    const auto rule = bytes.find("\"requires\"");
    ASSERT_NE(rule, std::string::npos);
    bytes.replace(rule, 10, "\"after\"   ");
    {
        std::ofstream target(dir / L"plugins" / L"a-dependent.dll", std::ios::binary);
        target.write(bytes.data(), bytes.size());
    }
    nc_runtime_start(dir.c_str(), 0);
    auto rows = catalog();
    for (auto& row : rows.get_array())
        row["enabled"] = true;
    const auto before = tao::json::to_string(rows);
    EXPECT_STRNE(nc_runtime_order_warnings(before.c_str()), "");
    const auto result = parse(nc_runtime_recommend(before.c_str()));
    ASSERT_NE(result.find("plugins"), nullptr);
    EXPECT_EQ(result.at("warning"), "");
    EXPECT_STREQ(nc_runtime_order_warnings(tao::json::to_string(result.at("plugins")).c_str()), "");
}
TEST_F(Runtime, ChangedBinaryRevokesApproval)
{
    enable();
    nc_runtime_stop();
    {
        std::ofstream append(dir / L"plugins" / L"settings.dll", std::ios::binary | std::ios::app);
        append.put('\0');
    }
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(catalog().at(0).at("approved").get_boolean());
    EXPECT_FALSE(catalog().at(0).at("running").get_boolean());
}
TEST_F(Runtime, StaleDialogCannotApproveReplacedDll)
{
    auto rows = catalog();
    rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
    {
        std::ofstream append(dir / L"plugins" / L"settings.dll", std::ios::binary | std::ios::app);
        append.put('\0');
    }
    EXPECT_STRNE(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
}
TEST_F(Runtime, LoadedDllCannotBeReplaced)
{
    enable();
    restart();
    HANDLE file =
        CreateFileW((dir / L"plugins" / L"settings.dll").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    EXPECT_EQ(file, INVALID_HANDLE_VALUE);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
}
TEST_F(Runtime, InvalidSettingDoesNotCommit)
{
    enable();
    restart();
    EXPECT_STRNE(nc_runtime_settings(R"([{"owner":"test.settings","id":"enabled","value":2}])"), "");
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 0);
}
TEST_F(Runtime, DuplicateBinaryIdsBlockEvenWhenOnlyOneSelected)
{
    fs::copy_file(PLUGIN_FIXTURE_PATH, dir / L"plugins" / L"duplicate.dll");
    auto rows = catalog();
    rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
    EXPECT_STRNE(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
}
TEST(PluginPermissions, UnknownAndDuplicatePermissionsAreRejected)
{
    EXPECT_EQ(permission_mask(Json::array({"ui.settings", "player.write"})), NC_PERMISSION_UI_SETTINGS | NC_PERMISSION_PLAYER_WRITE);
    EXPECT_THROW(permission_mask(Json::array({"cvars.read", "cvars.read"})), std::exception);
    EXPECT_THROW(permission_mask(Json::array({"future.permission"})), std::exception);
    EXPECT_EQ(permission_mask(permission_names(262143)), 262143u);
}
TEST_F(Runtime, FirstEnableRequiresConsentAndStoresExactGrants)
{
    auto rows = catalog();
    rows.at(0)["enabled"] = true;
    EXPECT_STRNE(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    EXPECT_FALSE(fs::exists(dir / L"plugins" / L"profile.json"));
    rows.at(0)["consent"] = true;
    rows.at(0)["permissions"] = tao::json::empty_array; // UI cannot reduce the manifest's required grants.
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    std::ifstream stream(dir / L"plugins" / L"profile.json");
    const auto saved = parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    stream.close();
    EXPECT_EQ(permission_mask(saved.at("plugins").at(0).at("permissions")), NC_PERMISSION_UI_SETTINGS | NC_PERMISSION_PLAYER_WRITE);
    restart();
    rows = catalog();
    EXPECT_TRUE(rows.at(0).at("approved").get_boolean());
    EXPECT_FALSE(rows.at(0).at("consent").get_boolean());
    rows.at(0)["enabled"] = false;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    restart();
    rows = catalog();
    rows.at(0)["enabled"] = true; // The same approved DLL does not need repeated consent.
    EXPECT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
}
TEST_F(Runtime, MissingPermissionGrantsInvalidateApproval)
{
    enable();
    nc_runtime_stop();
    const auto path = dir / L"plugins" / L"profile.json";
    Json profile;
    {
        std::ifstream stream(path);
        profile = parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    }
    profile.at("plugins").at(0).erase("permissions");
    {
        std::ofstream stream(path);
        stream << tao::json::to_string(profile);
    }
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(catalog().at(0).at("approved").get_boolean());
    EXPECT_FALSE(catalog().at(0).at("running").get_boolean());
}

namespace
{
    int cvar_reads{}, cvar_writes{}, draw_calls{};
    std::string last_cvar_value;
    std::vector<std::string> draw_order, sounds, console_output;
    std::vector<std::string> registered_commands;
    const NcClientServices test_services{
        [](const char* name) -> int32_t {
            registered_commands.emplace_back(name);
            return 1;
        },
        [](NcPlayerState* value) -> int32_t {
            value->health = 73;
            return 1;
        },
        [](int32_t index, NcEntity* value) -> int32_t {
            value->position[0] = 123;
            return index == 2;
        },
        [](int32_t id, NcWeapon* value) -> int32_t {
            value->clip = 19;
            return id == 7;
        },
        [](const char*, char* buffer, uint32_t capacity) -> uint32_t {
            ++cvar_reads;
            if (buffer && capacity >= 4)
                std::memcpy(buffer, "250", 4);
            return 4;
        },
        [](const char*, const char* value) -> int32_t {
            ++cvar_writes;
            last_cvar_value = value;
            return 1;
        },
        [](int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgba) {
            ++draw_calls;
            draw_order.emplace_back("rect");
            EXPECT_EQ(x, 5);
            EXPECT_EQ(y, 6);
            EXPECT_EQ(w, 7);
            EXPECT_EQ(h, 8);
            EXPECT_EQ(rgba, 0x12345678u);
        },
        [](NcSession* value) -> int32_t {
            value->width = 800;
            return 1;
        },
        [](int32_t index, NcPlayerInfo* value) -> int32_t {
            value->ping = 25;
            std::memcpy(value->name, "Alex", 5);
            return index == 2;
        },
        [](const float* world, float* screen) -> int32_t {
            EXPECT_EQ(world[0], 1);
            screen[0] = 400;
            screen[1] = 300;
            return 1;
        },
        [](const char* text, int32_t* width, int32_t* height) -> int32_t {
            EXPECT_STREQ(text, "text");
            *width = 32;
            *height = 12;
            return 1;
        },
        [](int32_t x, int32_t y, const char* text, uint32_t rgb) {
            EXPECT_EQ(x, 9);
            EXPECT_EQ(y, 10);
            EXPECT_STREQ(text, "text");
            EXPECT_EQ(rgb, 0x123456u);
            draw_order.emplace_back("text");
        },
        [](const char* path, float volume) {
            EXPECT_EQ(volume, 0.5f);
            sounds.emplace_back(path);
        },
        [](const char* text) { console_output.emplace_back(text); }
    };
    uint32_t PermissionResults(const char* symbol = "nc_permission_results")
    {
        auto module = GetModuleHandleW(L"permissions.dll");
        if (!module)
            return UINT32_MAX;
        auto result = reinterpret_cast<uint32_t(NC_CALL*)()>(GetProcAddress(module, symbol));
        return result ? result() : UINT32_MAX;
    }
} // namespace
class PermissionRuntime : public Runtime
{
protected:
    void install(const char* fixture)
    {
        nc_runtime_stop();
        cvar_reads = cvar_writes = draw_calls = 0;
        registered_commands.clear();
        last_cvar_value.clear();
        draw_order.clear();
        sounds.clear();
        console_output.clear();
        fs::remove(dir / L"plugins" / L"settings.dll");
        fs::copy_file(fixture, dir / L"plugins" / L"permissions.dll");
        nc_runtime_bind_client(&test_services);
        nc_runtime_start(dir.c_str(), 0);
        enable();
        restart();
        ASSERT_TRUE(catalog().at(0).at("running").get_boolean());
    }
    int setting_count()
    {
        std::ifstream stream(dir / L"plugins" / L".host" / L"settings" / L"plugin-test.permissions.json");
        return parse(std::string(std::istreambuf_iterator<char>(stream), {})).at("count").as<int>();
    }
};
TEST_F(PermissionRuntime, SafeApisWorkAndImpactfulApisAreDeniedByDefault)
{
    install(PLUGIN_DENIED_PATH);
    EXPECT_EQ(PermissionResults(), 31u);
    EXPECT_EQ(PermissionResults("nc_extension_results"), 47u);
    EXPECT_TRUE(sounds.empty());
    EXPECT_EQ(console_output, (std::vector<std::string>{"value: %s %n\n"}));
    EXPECT_EQ(move(0).buttons, 3u);
    EXPECT_EQ(cvar_reads, 0);
    EXPECT_EQ(cvar_writes, 0);
    EXPECT_FALSE(nc_runtime_ui_hidden(NC_UI_CROSSHAIR));
    EXPECT_TRUE(parse(nc_runtime_ui()).at(0).at("tabs").get_array().empty());
    EXPECT_EQ(PermissionResults("nc_shared_control_result"), 1u);
    EXPECT_EQ(PermissionResults("nc_native_control_result"), 1u);
    EXPECT_EQ(PermissionResults("nc_builtin_control_result"), 0u);
    EXPECT_EQ(PermissionResults("nc_custom_control_result"), 0u);
    ASSERT_EQ(parse(nc_runtime_ui()).at(0).at("controls").get_array().size(), 2u);
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("tab"), "plugins");
    EXPECT_EQ(registered_commands, (std::vector<std::string>{"nc.test.permissions.inspect"}));
    const char* args[]{"nc.test.permissions.inspect", "hello"};
    nc_runtime_console(2, args);
    EXPECT_EQ(setting_count(), 2);
    const NcDrawContext frame{sizeof(NcDrawContext), 800, 600, 0, 1};
    nc_runtime_draw(&frame);
    EXPECT_EQ(draw_calls, 0);
}
TEST_F(PermissionRuntime, GrantedApisWorkOnlyInTheirValidContexts)
{
    install(PLUGIN_PERMISSION_PATH);
    EXPECT_EQ(PermissionResults("nc_shared_control_result"), 1u);
    EXPECT_EQ(PermissionResults("nc_native_control_result"), 1u);
    EXPECT_EQ(PermissionResults("nc_builtin_control_result"), 1u);
    EXPECT_EQ(PermissionResults("nc_custom_control_result"), 1u);
    EXPECT_EQ(PermissionResults("nc_extension_results"), 127u);
    EXPECT_EQ(sounds, (std::vector<std::string>{"buttons/blip1.wav"}));
    EXPECT_EQ(console_output, (std::vector<std::string>{"value: %s %n\n"}));
    EXPECT_EQ(PermissionResults(), 511u); // No out-of-frame draw or worker-thread access.
    EXPECT_EQ(move(0).buttons, 11u);
    EXPECT_EQ(cvar_reads, 2);
    EXPECT_EQ(cvar_writes, 1);
    EXPECT_EQ(last_cvar_value, ";quit\n");
    EXPECT_TRUE(nc_runtime_ui_hidden(NC_UI_CROSSHAIR));
    EXPECT_FALSE(nc_runtime_ui_hidden(NC_UI_HUD));
    const NcDrawContext frame{sizeof(NcDrawContext), 800, 600, 0, 1};
    nc_runtime_draw(&frame);
    EXPECT_EQ(draw_calls, 2);
    EXPECT_EQ(draw_order, (std::vector<std::string>{"rect", "text", "rect"}));
    EXPECT_EQ(PermissionResults("nc_extension_results"), 127u);
    for (auto element : {NC_UI_HEALTH, NC_UI_RADAR, NC_UI_DEATH_NOTICES})
        EXPECT_TRUE(nc_runtime_ui_hidden(element));
    SetEnvironmentVariableW(L"NEXTCLIENT_PERMISSIONS_FAIL", L"1");
    nc_runtime_draw(&frame);
    EXPECT_EQ(draw_order, (std::vector<std::string>{"rect", "text", "rect"}));
    for (auto element : {NC_UI_HEALTH, NC_UI_RADAR, NC_UI_DEATH_NOTICES})
        EXPECT_FALSE(nc_runtime_ui_hidden(element));
    EXPECT_EQ(draw_calls, 2); // Failed callback's queued drawing is discarded.
    EXPECT_FALSE(nc_runtime_ui_hidden(NC_UI_CROSSHAIR));
    const char* args[]{"nc.test.permissions.inspect", "hello"};
    nc_runtime_console(2, args);
    EXPECT_EQ(setting_count(), 1); // Failed plugins cannot handle commands.
}
TEST_F(PermissionRuntime, ReadPermissionDoesNotGrantWritesOrUiChanges)
{
    install(PLUGIN_READ_ONLY_PATH);
    EXPECT_EQ(PermissionResults(), 95u);
    EXPECT_EQ(cvar_reads, 2);
    EXPECT_EQ(cvar_writes, 0);
    EXPECT_EQ(move(0).buttons, 3u);
    EXPECT_FALSE(nc_runtime_ui_hidden(NC_UI_CROSSHAIR));
}
TEST_F(PermissionRuntime, WritePermissionDoesNotGrantReadAccess)
{
    install(PLUGIN_WRITE_ONLY_PATH);
    EXPECT_EQ(PermissionResults(), 159u);
    EXPECT_EQ(cvar_reads, 0);
    EXPECT_EQ(cvar_writes, 1);
    EXPECT_EQ(move(0).buttons, 3u);
    EXPECT_FALSE(nc_runtime_ui_hidden(NC_UI_CROSSHAIR));
}
TEST_F(PermissionRuntime, DeferredClientBindingRegistersCommandsAndShutdownRestoresUi)
{
    install(PLUGIN_PERMISSION_PATH);
    nc_runtime_bind_client(nullptr);
    restart();
    registered_commands.clear();
    nc_runtime_bind_client(&test_services);
    EXPECT_EQ(registered_commands, (std::vector<std::string>{"nc.test.permissions.inspect"}));
    nc_runtime_stop();
    EXPECT_FALSE(nc_runtime_ui_hidden(NC_UI_CROSSHAIR));
    const NcDrawContext frame{sizeof(NcDrawContext), 800, 600, 0, 1};
    nc_runtime_draw(&frame);
    EXPECT_EQ(draw_calls, 0);
}
TEST_F(Runtime, MetadataDiscoveryNeverRunsDllMain)
{
    fs::copy_file(PLUGIN_PROBE_PATH, dir / L"plugins" / L"probe.dll");
    SetEnvironmentVariableW(L"NEXTCLIENT_PROBE_MARKER", (dir / L"marker").c_str());
    auto rows = catalog();
    EXPECT_FALSE(fs::exists(dir / L"marker"));
    for (auto& r : rows.get_array())
        r["enabled"] = r["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    EXPECT_FALSE(fs::exists(dir / L"marker"));
    restart();
    EXPECT_TRUE(fs::exists(dir / L"marker"));
}
TEST_F(Runtime, FailedInitializationRollsBackUiAndBlocksDependents)
{
    fs::copy_file(PLUGIN_PROBE_PATH, dir / L"plugins" / L"a-probe.dll");
    fs::copy_file(PLUGIN_DEPENDENT_PATH, dir / L"plugins" / L"b-dependent.dll");
    auto rows = catalog();
    for (auto& r : rows.get_array())
        r["enabled"] = r["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    SetEnvironmentVariableW(L"NEXTCLIENT_PROBE_FAIL_LOAD", L"1");
    restart();
    auto ui = parse(nc_runtime_ui());
    ASSERT_EQ(ui.get_array().size(), 1u);
    EXPECT_EQ(ui.at(0).at("id"), "test.settings");
}
TEST_F(Runtime, CallbackFailureRevertsCommandAndRetiresDependents)
{
    fs::copy_file(PLUGIN_PROBE_PATH, dir / L"plugins" / L"a-probe.dll");
    fs::copy_file(PLUGIN_DEPENDENT_PATH, dir / L"plugins" / L"b-dependent.dll");
    auto rows = catalog();
    for (auto& r : rows.get_array())
        r["enabled"] = r["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    restart();
    auto ui = parse(nc_runtime_ui());
    ASSERT_EQ(ui.get_array().size(), 3u);
    EXPECT_EQ(ui.at(0).at("controls").get_array().size(), 4u);
    EXPECT_EQ(ui.at(0).at("controls").at(0).at("tab"), "mouse");
    SetEnvironmentVariableW(L"NEXTCLIENT_PROBE_FAIL_COMMAND", L"1");
    EXPECT_EQ(move(0).buttons, 3u);
    EXPECT_EQ(parse(nc_runtime_ui()).get_array().size(), 1u);
    SetEnvironmentVariableW(L"NEXTCLIENT_PROBE_FAIL_COMMAND", nullptr);
    EXPECT_EQ(move(0).buttons, 3u);
}

TEST_F(Runtime, CachedRetiredControlsDoNotBlockOtherSettings)
{
    fs::copy_file(PLUGIN_PROBE_PATH, dir / L"plugins" / L"a-probe.dll");
    fs::copy_file(PLUGIN_DEPENDENT_PATH, dir / L"plugins" / L"b-dependent.dll");
    auto rows = catalog();
    for (auto& row : rows.get_array())
        row["enabled"] = row["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    restart();
    auto cached = parse(R"([
        {"owner":"test.probe","id":"enabled","value":1},
        {"owner":"test.dependent","id":"enabled","value":1},
        {"owner":"test.settings","id":"enabled","value":1}])");
    SetEnvironmentVariableW(L"NEXTCLIENT_PROBE_FAIL_COMMAND", L"1");
    move(0);
    ASSERT_STREQ(nc_runtime_settings(tao::json::to_string(cached).c_str()), "");
    EXPECT_EQ(move(NC_PLAYER_VALID | NC_PLAYER_ACTIVE | NC_PLAYER_CAN_JUMP).buttons, 1u);
    EXPECT_FALSE(fs::exists(dir / L"plugins" / L".host" / L"settings" / L"plugin-test.probe.json"));
    EXPECT_FALSE(fs::exists(dir / L"plugins" / L".host" / L"settings" / L"plugin-test.dependent.json"));
    EXPECT_STRNE(nc_runtime_settings(R"([{"owner":"unknown","id":"enabled","value":1}])"), "");
}

TEST_F(PermissionRuntime, AudioPermissionDoesNotGrantUiChanges)
{
    install(PLUGIN_AUDIO_ONLY_PATH);
    EXPECT_EQ(PermissionResults("nc_extension_results"), 63u);
    EXPECT_EQ(sounds.size(), 1u);
    EXPECT_EQ(console_output, (std::vector<std::string>{"value: %s %n\n"}));
    EXPECT_FALSE(nc_runtime_ui_hidden(NC_UI_HEALTH));
}
TEST_F(PermissionRuntime, ConsoleOutputNeedsNoPermissionAndDoesNotGrantAudioOrUi)
{
    install(PLUGIN_DENIED_PATH);
    EXPECT_EQ(PermissionResults("nc_extension_results"), 47u);
    EXPECT_TRUE(sounds.empty());
    EXPECT_EQ(console_output.size(), 1u);
    EXPECT_FALSE(nc_runtime_ui_hidden(NC_UI_RADAR));
}
TEST_F(PermissionRuntime, FrameCallbackNeedsNoPermissionAndStopsAfterFailure)
{
    install(PLUGIN_DENIED_PATH);
    NcSession context{};
    context.size = sizeof(context);
    context.width = 800;
    nc_runtime_frame(&context);
    EXPECT_EQ(PermissionResults("nc_frame_calls"), 1u);
    context.size = 0;
    nc_runtime_frame(&context);
    EXPECT_EQ(PermissionResults("nc_frame_calls"), 1u);
    context.size = sizeof(context);
    SetEnvironmentVariableW(L"NEXTCLIENT_FRAME_FAIL", L"1");
    nc_runtime_frame(&context);
    nc_runtime_frame(&context);
    EXPECT_EQ(PermissionResults("nc_frame_calls"), 2u);
    EXPECT_FALSE(catalog().at(0).at("running").get_boolean());
}
TEST_F(PermissionRuntime, FrameFailureRestoresAllRequestedHudElements)
{
    install(PLUGIN_PERMISSION_PATH);
    NcSession context{};
    context.size = sizeof(context);
    context.width = 800;
    SetEnvironmentVariableW(L"NEXTCLIENT_FRAME_FAIL", L"1");
    nc_runtime_frame(&context);
    for (auto element : {NC_UI_CROSSHAIR, NC_UI_HEALTH, NC_UI_RADAR, NC_UI_DEATH_NOTICES})
        EXPECT_FALSE(nc_runtime_ui_hidden(element));
    const NcDrawContext draw{sizeof(NcDrawContext), 800, 600, 0, 1};
    nc_runtime_draw(&draw);
    EXPECT_TRUE(draw_order.empty());
}

TEST_F(PermissionRuntime, ConsoleOutputValidatesTextAndRejectsUnavailableOrRetiredClients)
{
    install(PLUGIN_DENIED_PATH);
    auto print = reinterpret_cast<int32_t(NC_CALL*)(const char*)>(GetProcAddress(GetModuleHandleW(L"permissions.dll"), "nc_print_again"));
    ASSERT_NE(print, nullptr);
    console_output.clear();
    EXPECT_EQ(print(nullptr), 0);
    EXPECT_EQ(print(std::string(4097, 'x').c_str()), 0);
    EXPECT_TRUE(console_output.empty());
    EXPECT_EQ(print(std::string(4096, 'x').c_str()), 1);
    ASSERT_EQ(console_output.size(), 1u);
    EXPECT_EQ(console_output[0], std::string(4096, 'x') + "\n");
    nc_runtime_bind_client(nullptr);
    EXPECT_EQ(print("client unavailable"), 0);
    nc_runtime_bind_client(&test_services);
    NcSession frame{};
    frame.size = sizeof(frame);
    frame.width = 800;
    SetEnvironmentVariableW(L"NEXTCLIENT_FRAME_FAIL", L"1");
    nc_runtime_frame(&frame);
    EXPECT_EQ(print("retired"), 0);
    EXPECT_EQ(console_output.size(), 1u);
}

TEST_F(Runtime, UntouchedOptionsDoNotUndoPluginCommandsAndApplyRefreshesBaseline)
{
    enable();
    restart();
    PluginSettingsState editor;
    const auto opened = parse(nc_runtime_ui());
    auto spec = opened.at(0).at("controls").at(0);
    spec["owner"] = "test.settings";
    editor.ResetControl(spec, opened);
    const char* args[]{"nc.test.settings.toggle"};
    nc_runtime_console(1, args);
    ASSERT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);
    Json values = tao::json::empty_array;
    editor.Collect(values, spec, 0, parse(nc_runtime_ui()));
    EXPECT_TRUE(values.get_array().empty());
    ASSERT_STREQ(nc_runtime_settings(tao::json::to_string(values).c_str()), "");
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);

    // Apply/Reset refreshes the widget and baseline from the live value.
    editor.ResetControl(spec, parse(nc_runtime_ui()));
    editor.Collect(values, spec, 0, parse(nc_runtime_ui()));
    ASSERT_EQ(values.get_array().size(), 1u);
    ASSERT_STREQ(nc_runtime_settings(tao::json::to_string(values).c_str()), "");
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 0);
    editor.ResetControl(spec, parse(nc_runtime_ui()));
    values = tao::json::empty_array;
    nc_runtime_console(1, args);
    editor.Collect(values, spec, 0, parse(nc_runtime_ui()));
    EXPECT_TRUE(values.get_array().empty());
    EXPECT_EQ(parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);
}
TEST(PluginSettingsState, OnlyChangedAvailableControlsAreSubmitted)
{
    auto baseline = parse(R"([{"id":"one","controls":[{"id":"slider","value":10},{"id":"choice","value":0}]},
        {"id":"two","controls":[{"id":"slider","value":20}]}])");
    auto current = baseline;
    current.at(0).at("controls").at(0)["value"] = 11;
    current.at(0).at("controls").at(1)["value"] = 1;
    PluginSettingsState editor;
    Json values = tao::json::empty_array;
    const Json slider{{"owner", "one"}, {"id", "slider"}};
    editor.ResetControl(slider, baseline);
    editor.ResetControl(Json{{"owner", "one"}, {"id", "choice"}}, baseline);
    editor.Collect(values, slider, 10, current); // Untouched or reverted to baseline.
    editor.Collect(values, Json{{"owner", "one"}, {"id", "choice"}}, 1, current); // Already applied by an action.
    editor.Collect(values, Json{{"owner", "gone"}, {"id", "slider"}}, 30, current);
    EXPECT_TRUE(values.get_array().empty());
    editor.Collect(values, slider, 12, current); // Explicit edit takes precedence.
    ASSERT_EQ(values.get_array().size(), 1u);
    EXPECT_EQ(values.at(0).at("value"), 12);
    EXPECT_EQ(values.at(0).at("owner"), "one");
    current = tao::json::empty_array;
    editor.Collect(values, slider, 13, current); // Retired owner.
    EXPECT_EQ(values.get_array().size(), 1u);
}

TEST(PluginSettingsState, RetiredOwnersCannotSupplyCachedControls)
{
    auto plugins = tao::json::from_string(R"([
        {"id":"active","controls":[{"id":"enabled","value":1}]}
    ])");
    auto spec = tao::json::from_string(R"({"owner":"retired","id":"enabled"})");
    EXPECT_EQ(PluginSettingsSnapshot(plugins).Find(spec), nullptr);
    spec["owner"] = "active";
    ASSERT_NE(PluginSettingsSnapshot(plugins).Find(spec), nullptr);
    EXPECT_EQ(PluginSettingsSnapshot(plugins).Find(spec)->at("value"), 1);
    spec["id"] = "removed";
    EXPECT_EQ(PluginSettingsSnapshot(plugins).Find(spec), nullptr);
}

TEST(PluginSettingsDecode, ContainsInvalidUtf8AndSchemaFailures)
{
    EXPECT_TRUE(PluginSettings_Parse("[").get_array().empty());
    EXPECT_TRUE(PluginSettings_Parse("{}").get_array().empty());
    EXPECT_TRUE(PluginSettings_Parse(R"([{"id":"broken","controls":42}])").get_array().empty());
    const std::string malformed =
        std::string(R"([{"id":"broken","tabs":[{"id":"a","en":")") + char(0xe9) + R"(","ru":""}],"controls":[]}])";
    EXPECT_TRUE(PluginSettings_Parse(malformed.c_str()).get_array().empty());
    EXPECT_EQ(PluginSettings_Parse(R"([{"id":"valid","tabs":[],"controls":[]}])").get_array().size(), 1u);
}
TEST(PluginOrder, InvalidMetadataCannotParticipateInVersionChecks)
{
    Item a = item("a"), b = item("b");
    b.manifest.version.clear();
    b.error = message("#NextPlugins_ErrorLoadDll");
    a.manifest.conflicts.push_back({"b", ">=1.0.0", "conflict"});
    a.manifest.before.push_back({"b", ">=1.0.0", "order"});
    EXPECT_NO_THROW(EXPECT_FALSE(validate({a, b}).empty()));
    EXPECT_TRUE(ordering_warnings({a, b}).empty());
    std::string warning;
    EXPECT_NO_THROW(recommend({a, b}, warning));
}
TEST_F(Runtime, DamagedConflictTargetDoesNotDisableUnrelatedPlugins)
{
    nc_runtime_stop();
    fs::copy_file(PLUGIN_PROBE_PATH, dir / L"plugins" / L"probe.dll");
    std::ifstream source(PLUGIN_DEPENDENT_PATH, std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(source), {});
    const std::string original = pe_manifest(std::vector<unsigned char>(bytes.begin(), bytes.end()));
    Json metadata = parse(original);
    metadata.erase("requires");
    metadata["description"] = "";
    metadata["conflicts"] = Json::array({Json{{"id", "test.probe"}, {"version", ">=2.0.0"}, {"reason", "test"}}});
    std::string replacement = tao::json::to_string(metadata);
    ASSERT_LE(replacement.size(), original.size());
    replacement.resize(original.size(), ' ');
    bytes.replace(bytes.find(original), original.size(), replacement);
    {
        std::ofstream target(dir / L"plugins" / L"dependent.dll", std::ios::binary);
        target.write(bytes.data(), bytes.size());
    }
    nc_runtime_start(dir.c_str(), 0);
    Json rows = catalog();
    for (auto& row : rows.get_array())
    {
        row["enabled"] = row["consent"] = true;
    }
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    nc_runtime_stop();
    {
        std::ofstream damaged(dir / L"plugins" / L"probe.dll", std::ios::binary | std::ios::trunc);
        damaged << "damaged";
    }
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    for (const auto& row : catalog().get_array())
    {
        if (row.at("id") == "test.probe")
        {
            EXPECT_FALSE(row.at("error").get_string().empty());
            EXPECT_FALSE(row.at("running").get_boolean());
        }
        else
        {
            EXPECT_TRUE(row.at("running").get_boolean());
        }
    }
}
TEST_F(Runtime, CatalogReusesMetadataButApprovalStillRequiresLockedRead)
{
    Json rows = catalog();
    HANDLE writer = CreateFileW(
        (dir / L"plugins" / L"settings.dll").c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr
    );
    ASSERT_NE(writer, INVALID_HANDLE_VALUE);
    const Json cached = catalog();
    EXPECT_EQ(cached.at(0).at("error"), "");
    EXPECT_EQ(cached.at(0).at("hash"), rows.at(0).at("hash"));
    rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
    EXPECT_STRNE(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    CloseHandle(writer);
}
TEST_F(Runtime, ApprovalBypassesCacheForSameSizeSameTimestampReplacement)
{
    Json rows = catalog();
    const fs::path file = dir / L"plugins" / L"settings.dll";
    const auto timestamp = fs::last_write_time(file);
    {
        std::fstream output(file, std::ios::binary | std::ios::in | std::ios::out);
        output.seekp(0);
        output.put('X');
    }
    fs::last_write_time(file, timestamp);
    rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
    EXPECT_STRNE(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
}
