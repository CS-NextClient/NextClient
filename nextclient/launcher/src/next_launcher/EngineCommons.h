#pragma once

#include <memory>
#include <next_gameui/IGameConsoleNext.h>
#include <next_launcher/UserInfoClient.h>
#include <nitroapi/NitroApiInterface.h>
#include <nitroapi/NitroApiHelper.h>
#include <vgui/ILocalize.h>
#include <updater/json_data/BranchEntry.h>

class EngineCommons : public nitroapi::NitroApiHelper
{
    vgui2::ILocalize* localize_{};
    IGameConsoleNext* game_console_next_{};
    std::shared_ptr<next_launcher::UserInfoClient> user_info_{};
    std::string versions_{};

    std::vector<BranchEntry> branches_{};

    static std::unique_ptr<EngineCommons> instance_;

private:
    explicit EngineCommons(nitroapi::NitroApiInterface* nitro_api, std::shared_ptr<next_launcher::UserInfoClient> user_info, std::string versions, std::vector<BranchEntry> branches);

public:
    void BranchCmd();
    void VersionCmd();

    static EngineCommons& Instance();
    static EngineCommons& Init(nitroapi::NitroApiInterface* nitro_api, std::shared_ptr<next_launcher::UserInfoClient> user_info, std::string versions, std::vector<BranchEntry> branches);
    static void Reset();

private:
    void InitializeInternal();
    void ConPrintBranchInfo();

    static void BranchCmdStatic();
    static void VersionCmdStatic();
    static void TestCrashCmdStatic();
};
