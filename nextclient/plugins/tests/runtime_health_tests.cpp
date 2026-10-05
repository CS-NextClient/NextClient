#include "plugin_file.h"
#include <nextclient/runtime.h>
#include <gtest/gtest.h>
#include <fstream>

using namespace plugins;
namespace fs = std::filesystem;

class RuntimeHealth : public testing::Test
{
protected:
    fs::path dir;
    void SetUp() override
    {
        dir = fs::temp_directory_path() / (L"nc-health-" + std::to_wstring(GetCurrentProcessId()));
        fs::remove_all(dir);
        fs::create_directories(dir / L"plugins");
        write_json(dir / L"plugins" / L"profile.json", Json{{"schema", 2}, {"plugins", tao::json::empty_array}});
    }
    void TearDown() override
    {
        nc_runtime_stop();
        fs::remove_all(dir);
    }
};

TEST_F(RuntimeHealth, EmptyInstallationNeverCreatesRecoveryMarker)
{
    for (int i = 0; i < 3; ++i)
    {
        nc_runtime_start(dir.c_str(), 0);
        EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
        EXPECT_TRUE(parse(nc_runtime_catalog()).at("plugins").get_array().empty());
        EXPECT_FALSE(fs::exists(dir / L"plugins" / L"session.json"));
        EXPECT_EQ(nc_runtime_recovery_pending(), 0);
        nc_runtime_stop();
    }
}

TEST_F(RuntimeHealth, EmptyRecoveryCanBeAcknowledgedWithoutSavingSelection)
{
    write_json(dir / L"plugins" / L"session.json", Json{{"phase", "running"}, {"plugins", tao::json::empty_array}});
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_TRUE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    ASSERT_EQ(nc_runtime_recovery_pending(), 1);
    EXPECT_TRUE(fs::exists(dir / L"plugins" / L"session.previous.json"));
    // Cancelling/leaving recovery does not silently acknowledge the incident.
    nc_runtime_stop();
    EXPECT_TRUE(fs::exists(dir / L"plugins" / L"session.json"));
    nc_runtime_start(dir.c_str(), 0);
    ASSERT_EQ(nc_runtime_recovery_pending(), 1);
    EXPECT_STREQ(nc_runtime_acknowledge_recovery(), "");
    EXPECT_STREQ(nc_runtime_acknowledge_recovery(), "");
    EXPECT_EQ(nc_runtime_recovery_pending(), 0);
    EXPECT_FALSE(fs::exists(dir / L"plugins" / L"session.json"));
    nc_runtime_stop();
    nc_runtime_start(dir.c_str(), 0);
    EXPECT_FALSE(parse(nc_runtime_catalog()).at("safe_mode").get_boolean());
    EXPECT_FALSE(fs::exists(dir / L"plugins" / L"session.json"));
}

TEST_F(RuntimeHealth, StatisticsDoNotRescanOrInvalidateTheDialogSelection)
{
    nc_runtime_start(dir.c_str(), 0);
    ASSERT_TRUE(parse(nc_runtime_catalog()).at("plugins").get_array().empty());
    // A rescan would observe this new invalid DLL and invalidate the empty
    // dialog selection. Reading diagnostics must leave the catalog untouched.
    std::ofstream(dir / L"plugins" / L"broken.dll") << "not a DLL";
    auto stats = parse(nc_runtime_stats());
    EXPECT_TRUE(stats.at("plugins").get_array().empty());
    EXPECT_GT(stats.at("resources").at("host_memory_limit").as<uint64_t>(), 0u);
    EXPECT_EQ(parse(nc_runtime_recommend("[]")).find("error"), nullptr);
    EXPECT_EQ(parse(nc_runtime_catalog()).at("plugins").get_array().size(), 1u);
    EXPECT_NE(parse(nc_runtime_recommend("[]")).find("error"), nullptr);
}
