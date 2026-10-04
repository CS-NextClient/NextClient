#pragma once

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <port.h>
#endif
#include <string>
#include <format>
#include <optional>

#include <interface.h>

#include "Registry.h"
#include "CommandLine.h"
#include "Analytics.h"
#include "BackendAddressResolver.h"

#include <next_launcher/IUserInfo.h>
#include <next_launcher/IUserStorage.h>
#include <next_launcher/UserInfoClient.h>
#include <nitroapi/NitroApiInterface.h>
#include <nitro_utils/config_utils.h>
#include <next_client_mini/client_mini.h>
#include <next_engine_mini/engine_mini.h>
#include <next_gameui/IGameUiNext.h>
#include <updater_gui_app/json_data/BranchEntry.h>
#include <updater_gui_app/UpdaterDoneStatus.h>
#include <updater_gui_app/UpdaterFlags.h>
#include <updater_gui_app/UpdaterResult.h>

class ClientLauncher
{
public:
    struct NextProcess
    {
        std::string application;
        std::string command_line;
    };
    
private:
    enum class EngineSessionResult
    {
        Exit,
        Restart,
    };

#ifdef _WIN32
    static constexpr char kEngineDll[] = "hw.dll";
    static constexpr char kGameUiDll[] = "cstrike/cl_dlls/GameUI.dll";
#else
    static constexpr char kEngineDll[] = "hw.so";
    static constexpr char kGameUiDll[] = "cstrike/cl_dlls/gameui.so";
#endif
    static constexpr char kErrorTitle[] = "Counter-Strike Launcher";
    static constexpr char kNextClientRegistry[] = "Software\\Valve\\Half-Life\\nextclient";
    static constexpr char kHlRegistry[] = "Software\\Valve\\Half-Life\\Settings";
    static constexpr char kRelaunchEnvVar[] = "NEXTCLIENT_RELAUNCH";

    static constexpr char kDefaultConfigsDir[] = "platform/defaults";
    static constexpr const char* kDefaultConfigs[] = {
        "user_game_config.ini",
        "platform/config/MasterServer.vdf",
        "platform/config/backend.json",
    };
    
    const int kMinWidth = 640;
    const int kMinHeight = 480;
    const int kDefaultWidth = 800;
    const int kDefaultHeight = 600;

    NextClientVersion next_client_version_;
    std::shared_ptr<next_launcher::IUserStorage> user_storage_;
    std::shared_ptr<next_launcher::IUserInfo> user_info_;
    std::shared_ptr<next_launcher::UserInfoClient> user_info_client_;
    std::shared_ptr<Analytics> analytics_;
    std::shared_ptr<nitro_utils::FileConfigProvider> config_provider_;
    std::shared_ptr<BackendAddressResolver> backend_address_resolver_;

    std::shared_ptr<CRegistry> hl_registry_;
    std::shared_ptr<CCommandLine> cmd_line_;

    std::string sentry_init_log_;
    std::vector<BranchEntry> available_branches_;

    HINSTANCE module_instance_;
#ifdef _WIN32
    HANDLE global_win_mutex_;
#else
    int global_win_mutex_fd_ = -1;
    bool global_lock_acquired_ = false;
#endif
    bool is_relaunch_{};
    
    std::optional<NextProcess> next_process_;

public:
    explicit ClientLauncher(HINSTANCE module_instance, const char* cmd_line);
    ~ClientLauncher();

    void Run();
    const std::optional<NextProcess>& next_process() const { return next_process_; }

private:
#ifdef UPDATER_ENABLE
    UpdaterResult RunUpdater(UpdaterFlags updater_flags);
#endif
    UpdaterDoneStatus RunStartupUpdater();

    void PrepareEngineCommandLine();
    EngineSessionResult RunEngine();
    NextProcess BuildRestartProcess();
    NextProcess BuildNewGameProcess();

    void CreateConsoleWindowAndRedirectOutput();

    bool OnVideoModeFailed();
    void FixScreenResolution();
    bool GlobalMutexCheck();

    void InitializeAnalytics();
    void UninitializeAnalytics();
    void InitializeSentry();
    void UninitializeSentry();
    void HUD_InitHandler();

    void InitializeCmdLine(const char* cmd_line);
    void ModifyCmdLineAfterRestart(const char* cmd_line);
    void CheckVideoModeCrash();

    void Sys_ErrorHandler(const char* error);

    static std::string CreateVersionsString(nitroapi::NitroApiInterface* nitro_api,
                                            EngineMiniInterface* engine_mini,
                                            ClientMiniInterface* client_mini,
                                            IGameUINext* gameui_next);
    static void ProvisionDefaultConfigs();

    template<class T>
    std::tuple<T*, CSysModule*> LoadModule(const char* module_name, const char* interface_version)
    {
        auto raise_error = [this](const std::string& error) {
            analytics_->SendCrashMonitoringEvent("LoadModule Error", error.c_str(), true);
#ifdef _WIN32
            MessageBoxA(NULL, error.c_str(), kErrorTitle, MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
#else
            fprintf(stderr, "%s: %s\n", kErrorTitle, error.c_str());
#endif
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
