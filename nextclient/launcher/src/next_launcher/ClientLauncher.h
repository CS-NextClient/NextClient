#pragma once

#include <Windows.h>
#include <string>
#include <format>

#include <interface.h>

#include "Registry.h"
#include "CommandLine.h"
#include "Analytics.h"

#include <next_launcher/IUserInfo.h>
#include <next_launcher/IUserStorage.h>
#include <next_launcher/UserInfoClient.h>
#include <nitroapi/NitroApiInterface.h>
#include <nitro_utils/config_utils.h>
#include <next_client_mini/client_mini.h>
#include <next_engine_mini/engine_mini.h>
#include <next_gameui/IGameUINext.h>
#include <updater/json_data/BranchEntry.h>
#include <updater/UpdaterDoneStatus.h>

class ClientLauncher
{
    static constexpr char kEngineDll[] = "hw.dll";

    std::shared_ptr<next_launcher::IUserStorage> user_storage_;
    std::shared_ptr<next_launcher::IUserInfo> user_info_;
    std::shared_ptr<next_launcher::UserInfoClient> user_info_client_;
    std::shared_ptr<Analytics> analytics_;
    std::shared_ptr<nitro_utils::FileConfigProvider> config_provider_;

    std::shared_ptr<CRegistry> hl_registry_;
    std::shared_ptr<CCommandLine> cmd_line_;

    std::string sentry_init_log_;

    std::vector<BranchEntry> available_branches_;

    HINSTANCE module_instance_;
    HANDLE global_mutex_;

public:
    explicit ClientLauncher(HINSTANCE module_instance, const char* cmd_line);
    ~ClientLauncher();

    void Run();

private:
#ifdef UPDATER_ENABLE
    std::tuple<UpdaterDoneStatus, std::vector<BranchEntry>> RunUpdater();
#endif

    void RunEngine();

    void CreateConsoleWindowAndRedirectOutput();

    bool OnVideoModeFailed();
    void FixScreenResolution();

    void InitializeAnalytics();
    void UninitializeAnalytics();
    void InitializeSentry();
    void UninitializeSentry();

    void Sys_ErrorHandler(const char* error);

    static void LogLoadedModulesWarn(nitroapi::NitroApiInterface* nitro_api);
    static std::string CreateVersionsString(nitroapi::NitroApiInterface* nitro_api,
                                            EngineMiniInterface* engine_mini,
                                            ClientMiniInterface* client_mini,
                                            IGameUINext* gameui_next);

    template<class T>
    std::tuple<T*, CSysModule*> LoadModule(const char* module_name, const char* interface_version)
    {
        auto raise_error = [this](const std::string& error) {
            analytics_->SendCrashMonitoringEvent("LoadModule Error", error.c_str(), true);
            MessageBoxA(NULL, error.c_str(), "cs.exe", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
        };

        CSysModule* module = Sys_LoadModule(module_name);
        if (module == nullptr)
        {
            raise_error(std::format("Module {} not found", module_name));
            return {};
        }

        CreateInterfaceFn factory = Sys_GetFactory(module);
        if (factory == nullptr)
        {
            raise_error(std::format("Factory {} not found in {}", interface_version, module_name));
            return {};
        }

        T* interface_api = (T*)factory(interface_version, nullptr);
        if (interface_api == nullptr)
        {
            raise_error(std::format("Can't get {} from {}", interface_version, module_name));
            return {};
        }

        return { interface_api, module };
    }
};
