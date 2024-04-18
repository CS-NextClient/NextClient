#include "EngineCommons.h"
#include <utility>
#include <next_launcher/version.h>

std::unique_ptr<EngineCommons> EngineCommons::instance_{};

EngineCommons::EngineCommons(nitroapi::NitroApiInterface* nitro_api,
                                         std::shared_ptr<next_launcher::UserInfoClient> user_info,
                                         std::string versions,
                                         std::vector<BranchEntry> branches) :
    NitroApiHelper(nitro_api),
    user_info_(std::move(user_info)),
    versions_(std::move(versions)),
    branches_(std::move(branches))
{
    DeferUnsub(eng()->Sys_InitGame += [this](char* lpOrgCmdLine, char* pBaseDir, void* pwnd, int bIsDedicated, bool ret){
        InitializeInternal();
    });
}

EngineCommons& EngineCommons::Instance()
{
    return *instance_;
}

EngineCommons& EngineCommons::Init(nitroapi::NitroApiInterface* nitro_api,
                                               std::shared_ptr<next_launcher::UserInfoClient> user_info,
                                               std::string versions,
                                               std::vector<BranchEntry> branches)
{
    Reset();

    instance_ = std::unique_ptr<EngineCommons>(new EngineCommons(nitro_api, std::move(user_info), std::move(versions), std::move(branches)));

    return *instance_;
}

void EngineCommons::Reset()
{
    instance_ = nullptr;
}

void EngineCommons::BranchCmd()
{
    std::string branch_arg = cl_enginefunc()->Cmd_Argv(1);
    if (branch_arg.empty())
    {
        ConPrintBranchInfo();
        return;
    }

    if (!std::any_of(branches_.begin(), branches_.end(), [&branch_arg](const auto& branch_entry) { return branch_entry.name == branch_arg; })
        && branch_arg != "main")
    {
        wchar_t msg[128];

        wchar_t unicode_branch[64];
        localize_->ConvertANSIToUnicode(branch_arg.c_str(), unicode_branch, sizeof(unicode_branch));

        localize_->ConstructString(msg, sizeof(msg), localize_->Find("#GameUpdater_BranchNotFound"), 1, unicode_branch);
        game_console_next_->PrintfExWide(msg);
        return;
    }

    user_info_->SetUpdateBranch(branch_arg);

    cl_enginefunc()->pfnClientCmd("fmod stop\n");
    cl_enginefunc()->pfnClientCmd("_restart\n");
}

void EngineCommons::VersionCmd()
{
    cl_enginefunc()->Con_Printf("Build: %s\n", NEXT_CLIENT_BUILD_VERSION);
    cl_enginefunc()->Con_Printf("%s", versions_.c_str());
}

void EngineCommons::InitializeInternal()
{
    CreateInterfaceFn vgui2_factory = Sys_GetFactory("vgui2.dll");
    localize_ = (vgui2::ILocalize*)(InitializeInterface(VGUI_LOCALIZE_INTERFACE_VERSION, &vgui2_factory, 1));

    CreateInterfaceFn gameui_factory = Sys_GetFactory("cstrike\\cl_dlls\\gameui.dll");
    game_console_next_ = (IGameConsoleNext*)(InitializeInterface(GAMECONSOLE_NEXT_INTERFACE_VERSION, &gameui_factory, 1));

    cl_enginefunc()->pfnAddCommand("branch", BranchCmdStatic);
    cl_enginefunc()->pfnAddCommand("next_version", VersionCmdStatic);
    cl_enginefunc()->pfnAddCommand("test_crash", TestCrashCmdStatic);
}

void EngineCommons::ConPrintBranchInfo()
{
    wchar_t msg[512];

    wchar_t unicode_branch[64];
    localize_->ConvertANSIToUnicode(user_info_->GetUpdateBranch().c_str(), unicode_branch, sizeof(unicode_branch));

    localize_->ConstructString(msg, sizeof(msg), localize_->Find("#GameUpdater_BranchUsage"), 1, unicode_branch);
    game_console_next_->PrintfExWide(msg);

    if (branches_.empty())
    {
        game_console_next_->PrintfExWide(localize_->Find("#GameUpdater_NoBranches"));
        return;
    }

    game_console_next_->PrintfExWide(L"\n");

    for (const auto &branch: branches_)
    {
        if (!branch.visible)
            continue;

        localize_->ConvertANSIToUnicode(branch.name.c_str(), unicode_branch, sizeof(unicode_branch));

        localize_->ConstructString(msg, sizeof(msg), localize_->Find("#GameUpdater_BranchName"), 1, unicode_branch);
        game_console_next_->PrintfExWide(msg);
        if (!branch.desc.empty())
            game_console_next_->PrintfEx(" (%s)", branch.desc.c_str());
        game_console_next_->PrintfExWide(L"\n");
    }
}

void EngineCommons::BranchCmdStatic()
{
    instance_->BranchCmd();
}

void EngineCommons::VersionCmdStatic()
{
    instance_->VersionCmd();
}

void EngineCommons::TestCrashCmdStatic()
{
    int* a = nullptr;
    *a = 12;
}
