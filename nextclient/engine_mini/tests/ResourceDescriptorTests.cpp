#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "engine.h"
#include "client/cl_private_resources.h"
#include "resource/ResourceDescriptor.h"

namespace
{
    std::string g_GameDirectory;

    const char* GameDirectory()
    {
        return g_GameDirectory.c_str();
    }

    class ResourceDescriptorTest : public testing::Test
    {
    private:
        goldsrc::client_static_t client_{};
        goldsrc::cvar_t quiet_{};
        goldsrc::client_static_t* saved_client_{};
        goldsrc::cvar_t* saved_developer_{};
        decltype(gEngfuncs.pfnGetGameDirectory) saved_game_directory_{};

    protected:
        std::filesystem::path root_;

        void SetUp() override
        {
            saved_client_ = cls;
            saved_developer_ = developer;
            saved_game_directory_ = gEngfuncs.pfnGetGameDirectory;
            cls = &client_;
            developer = &quiet_;
            gEngfuncs.pfnGetGameDirectory = GameDirectory;

            root_ = std::filesystem::temp_directory_path() /
                    ("nextclient-resource-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(root_);
            g_GameDirectory = (root_ / "cstrike").string();
        }

        void TearDown() override
        {
            cls = saved_client_;
            developer = saved_developer_;
            gEngfuncs.pfnGetGameDirectory = saved_game_directory_;

            std::error_code error;
            std::filesystem::remove_all(root_, error);
        }
    };
} // namespace

TEST_F(ResourceDescriptorTest, PrivateFolderDropsTheTrailingSpaceInTheConnectedServerName)
{
    std::strcpy(cls->servername, "127.0.0.1:27020 ");

    EXPECT_EQ(PrivateRes_GetPrivateFolder(), "127_0_0_1_27020");

    ResourceDescriptor descriptor("materials/test_nextclient/material_effects.mat", "server/preset.mat", 0, 0, true);
    std::filesystem::path expected = root_ / "cstrike_downloads_private/127_0_0_1_27020/materials/test_nextclient/material_effects.mat";

    EXPECT_EQ(std::filesystem::path(descriptor.get_save_path()), expected);
}

TEST_F(ResourceDescriptorTest, PrivateFolderTrimsWhitespaceBeforeAddingTheDefaultPort)
{
    std::strcpy(cls->servername, "\tEXAMPLE.COM \r\n");

    EXPECT_EQ(PrivateRes_GetPrivateFolder(), "example_com_27015");
}

TEST_F(ResourceDescriptorTest, SaveCreatesMissingParentDirectoriesAndWritesTheCompletePayload)
{
    ResourceDescriptor descriptor("materials/test/preset.mat");
    const std::string payload = "material data\n";

    ASSERT_TRUE(descriptor.SaveToFile(payload.data(), static_cast<int>(payload.size())));

    std::ifstream file(root_ / "cstrike_downloads/materials/test/preset.mat", std::ios::binary);
    std::string saved((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_EQ(saved, payload);
}

TEST_F(ResourceDescriptorTest, SaveReturnsFalseWithoutThrowingWhenAParentIsAFile)
{
    std::filesystem::create_directory(root_ / "cstrike_downloads");
    const std::filesystem::path blocker = root_ / "cstrike_downloads/blocked";
    {
        std::ofstream file(blocker, std::ios::binary);
        file << "retain";
    }

    ResourceDescriptor descriptor("blocked/preset.mat");
    bool saved = true;

    EXPECT_NO_THROW(saved = descriptor.SaveToFile("data", 4));
    EXPECT_FALSE(saved);
    EXPECT_TRUE(std::filesystem::is_regular_file(blocker));
}
