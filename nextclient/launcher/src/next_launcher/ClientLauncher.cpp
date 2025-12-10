#include "ClientLauncher.h"

#include <clocale>
#include <easylogging++.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <string>
#include <thread>

#ifdef SENTRY_ENABLE
#include <sentry.h>
#endif

#ifdef UPDATER_ENABLE
#include <updater_gui_app/updater_gui_app.h>
#endif

#include <engine_launcher_api.h>
#include <nitroapi/NitroApiInterface.h>
#include <nitro_utils/config_utils.h>
#include <nitro_utils/string_utils.h>
#include <utils/platform.h>
#include <next_launcher/version.h>
#include <steam_api_proxy/next_steam_api_proxy.h>

#include "Analytics.h"
#include "DefaultUserInfo.h"
#include "exception_handler.h"
#include "RegistryUserStorage.h"
#include "EngineCommons.h"

static const char* NITRO_API_LOG_TAG = "launcher";

ClientLauncher::ClientLauncher(HINSTANCE module_instance, const char* cmd_line) :
    module_instance_(module_instance)
{
    next_client_version_ = {
        NEXT_CLIENT_BUILD_VERSION_MAJOR,
        NEXT_CLIENT_BUILD_VERSION_MINOR,
        NEXT_CLIENT_BUILD_VERSION_PATCH,
        NEXT_CLIENT_BUILD_VERSION_PRERELEASE };

    user_storage_ = std::make_shared<RegistryUserStorage>(kNextClientRegistry);
    user_info_ = std::make_shared<DefaultUserInfo>(user_storage_);
    user_info_client_ = std::make_shared<next_launcher::UserInfoClient>(user_info_.get());
    backend_address_resolver_ = std::make_shared<BackendAddressResolver>(user_info_client_);
    analytics_ = std::make_shared<Analytics>(user_info_client_, backend_address_resolver_);
    config_provider_ = std::make_shared<nitro_utils::FileConfigProvider>("user_game_config.ini");

    hl_registry_ = std::make_shared<CRegistry>(kHlRegistry);
    hl_registry_->Init();

    InitializeCmdLine(cmd_line);

    global_win_mutex_ = CreateMutexA(nullptr, FALSE, "ValveHalfLifeLauncherMutex");
    g_SaveFullDumps = cmd_line_->CheckParm("-fulldump");
}

ClientLauncher::~ClientLauncher()
{
    ReleaseMutex(global_win_mutex_);
    CloseHandle(global_win_mutex_);
}

void ClientLauncher::Run()
{
    LOG(INFO) << "Branch: " << user_info_client_->GetUpdateBranch();

    if (config_provider_->get_value_int("create_console_window", 0))
        CreateConsoleWindowAndRedirectOutput();

    InitializeSentry();
    InitializeAnalytics();
    FixScreenResolution();

    analytics_->SendAnalyticsEvent("startup_init");

    if (!GlobalMutexCheck())
    {
        MessageBoxA(
            NULL,
            "The game could not be started because it is already running.\n"
            "If it is not, then end the process in the task manager.",
            kErrorTitle,
            MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);

        UninitializeAnalytics();
        UninitializeSentry();
        return;
    }

    analytics_->SendAnalyticsEvent("startup_init_post_mutex");

#ifdef UPDATER_ENABLE
    UpdaterFlags updater_flags{};
    updater_flags |= cmd_line_->CheckParm("-noupdate") ? UpdaterFlags::None : UpdaterFlags::Updater;

    auto [updater_done, available_branches] = RunUpdater(updater_flags);
    available_branches_ = available_branches;

    if (updater_done == UpdaterDoneStatus::RunGame)
        RunEngine();
#else
    RunEngine();
#endif

    UninitializeAnalytics();
    UninitializeSentry();

#ifdef UPDATER_ENABLE
    if (updater_done == UpdaterDoneStatus::RunNewGame)
    {
        RunNewGame();
    }
#endif
}

void ClientLauncher::RunNewGame()
{
    cmd_line_->AppendParm("-noupdate", nullptr);

    std::string process_name = GetCurrentProcessPath()
        .filename()
        .replace_extension("")
        .string() + "_new.exe";

    PROCESS_INFORMATION process_information;
    STARTUPINFOA startupinfo;
    ZeroMemory(&startupinfo, sizeof(startupinfo));

    bool result = CreateProcessA(
        process_name.c_str(),
        (char*) cmd_line_->GetCmdLine(),
        nullptr,
        nullptr,
        false,
        NORMAL_PRIORITY_CLASS,
        nullptr,
        nullptr,
        &startupinfo,
        &process_information);

    LOG_IF(!result, ERROR) << "Can't CreateProcessA for new launcher: " << process_name << ". Error: " << GetWinErrorString(GetLastError());
}

void ClientLauncher::RunEngine()
{
    analytics_->SendAnalyticsEvent("startup_run_engine");

    char post_restart_cmd_line[4096] = { '\0' };

    PrepareEngineCommandLine();
    CheckVideoModeCrash();

    while (true)
    {
        EngineSessionResult result = RunEngineSession(post_restart_cmd_line);

        if (result == EngineSessionResult::Exit)
        {
            return;
        }
    }
}

ClientLauncher::EngineSessionResult ClientLauncher::RunEngineSession(char* post_restart_cmd_line)
{
    std::vector<std::shared_ptr<nitroapi::Unsubscriber>> unsubscribers;

    LogLoadedModules();

    auto [nitro_api, nitro_api_module] = LoadModule<nitroapi::NitroApiInterface>("nitro_api2.dll", NITROAPI_INTERFACE_VERSION);
    if (nitro_api == nullptr)
        return EngineSessionResult::Exit;

    auto [filesystem, filesystem_module] = LoadModule<IFileSystem>("filesystem_proxy.dll", FILESYSTEM_INTERFACE_VERSION);
    if (filesystem == nullptr)
        return EngineSessionResult::Exit;

    auto [engine_mini, engine_mini_module] = LoadModule<EngineMiniInterface>("next_engine_mini.dll", ENGINE_MINI_INTERFACE_VERSION);
    if (engine_mini == nullptr)
        return EngineSessionResult::Exit;

    auto [client_mini, client_mini_module] = LoadModule<ClientMiniInterface>("cstrike\\cl_dlls\\client_mini.dll", CLIENT_MINI_INTERFACE_VERSION);
    if (client_mini == nullptr)
        return EngineSessionResult::Exit;

    auto [gameui_next, gameui_next_module] = LoadModule<IGameUINext>("cstrike\\cl_dlls\\gameui.dll", GAMEUI_NEXT_INTERFACE_VERSION);
    if (gameui_next == nullptr)
        return EngineSessionResult::Exit;

    CSysModule* steam_proxy_module = Sys_LoadModule("steam_api.dll");
    if (steam_proxy_module == nullptr)
    {
        std::string error = "Module steam_api.dll not found";

        analytics_->SendCrashMonitoringEvent("LoadModule Error", error.c_str(), true);
        MessageBoxA(NULL, error.c_str(), kErrorTitle, MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
        return EngineSessionResult::Exit;
    }

    auto steam_proxy_set_seh = (NextSteamProxy_SetSEHFunc)GetProcAddress((HMODULE)steam_proxy_module, "NextSteamProxy_SetSEH");
    if (steam_proxy_set_seh == nullptr)
    {
        std::string error = "NextSteamProxy_SetSEH not found in steam_api.dll.\n"
                            "Make sure you use the steam_api.dll from NextClient and not the original steam_api.dll";

        analytics_->SendCrashMonitoringEvent("LoadModule Error", error.c_str(), true);
        MessageBoxA(NULL, error.c_str(), kErrorTitle, MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
        return EngineSessionResult::Exit;
    }
    steam_proxy_set_seh(ExceptionHandler);

    filesystem->Mount();
    filesystem->AddSearchPath("", "ROOT");

    nitro_api->WriteLog(NITRO_API_LOG_TAG, sentry_init_log_.c_str());
    nitro_api->SetSEHCallback(ExceptionHandler);
    nitro_api->Initialize(cmd_line_.get(), filesystem, hl_registry_.get());

    unsubscribers.emplace_back(nitro_api->GetEngineData()->Sys_Error |= [this](const char* error, const auto& next) {
        Sys_ErrorHandler(error);
        next->Invoke(error);
    });

    unsubscribers.emplace_back(nitro_api->GetSDL2Data()->SDL_DestroyWindowFunc += [this, nitro_api](void*) {
        SDL_DestroyWindowHandler(nitro_api);
    });

    unsubscribers.emplace_back(nitro_api->GetClientData()->HUD_Init += [this] {
        HUD_InitHandler();
    });

    auto [engine, engine_module] = LoadModule<IEngineAPI>(kEngineDll, VENGINE_LAUNCHER_API_VERSION);
    if (engine == nullptr)
        return EngineSessionResult::Exit;

    std::string versions = CreateVersionsString(nitro_api, engine_mini, client_mini, gameui_next);
    nitro_api->WriteLog(NITRO_API_LOG_TAG, versions.c_str());

    client_mini->Init(nitro_api);
    engine_mini->Init(nitro_api, next_client_version_, analytics_.get());

    LOG(INFO) << "IEngineAPI::Run";
    analytics_->AddBreadcrumb("Info", "IEngineAPI::Run");

    EngineCommons::Init(nitro_api, user_info_client_, versions, available_branches_);

    EngineRunResult engine_run_result = engine->Run(
        module_instance_,
        "",
        cmd_line_->GetCmdLine(),
        post_restart_cmd_line,
        Sys_GetFactoryThis(),
        Sys_GetFactory(filesystem_module));

    EngineCommons::Reset();

    LOG(INFO) << "IEngineAPI::Run done";
    analytics_->AddBreadcrumb("Info", std::format("IEngineAPI::Run done, result: {}", (int)engine_run_result).c_str());

    client_mini->Uninitialize();
    Sys_UnloadModule(client_mini_module);

    engine_mini->Uninitialize();
    Sys_UnloadModule(engine_mini_module);

    Sys_UnloadModule(engine_module);
    Sys_UnloadModule(gameui_next_module);

    filesystem->Unmount();
    Sys_UnloadModule(filesystem_module);

    for (auto& unsub : unsubscribers)
        unsub->Unsubscribe();

    nitro_api->UnInitialize();
    Sys_UnloadModule(nitro_api_module);

    EngineSessionResult engine_session_result{};

    switch (engine_run_result)
    {
        case ENGRUN_QUITTING:
            engine_session_result = EngineSessionResult::Exit;
            break;

        case ENGRUN_UNSUPPORTED_VIDEOMODE:
            engine_session_result = OnVideoModeFailed() ? EngineSessionResult::Exit : EngineSessionResult::Restart;
            break;

        default:
            engine_session_result = EngineSessionResult::Restart;
            break;
    }

    config_provider_->ReloadFromFile();

    ModifyCmdLineAfterRestart(post_restart_cmd_line);

    return engine_session_result;
}

void ClientLauncher::PrepareEngineCommandLine()
{
    if (!cmd_line_->CheckParm("-game"))
        cmd_line_->AppendParm("-game", "cstrike");

    // disable glBlitFramebuffer feature, because this makes blackscreen on some Nvidia GPU when enables MSSA technology
    if (!cmd_line_->CheckParm("-nodirectblit") && !cmd_line_->CheckParm("-directblit"))
        cmd_line_->AppendParm("-nodirectblit", nullptr);

    if (!cmd_line_->CheckParm("-num_edicts"))
        cmd_line_->AppendParm("-num_edicts", "4096");
}

#ifdef UPDATER_ENABLE
UpdaterResult ClientLauncher::RunUpdater(UpdaterFlags updater_flags)
{
    LOG(INFO) << "Start the Updater GUI App";
    analytics_->SendAnalyticsEvent("startup_run_updater");

    UpdaterResult result = RunUpdaterGuiApp(
        user_info_client_,
        user_storage_,
        analytics_,
        updater_flags,
        config_provider_->get_value_string("language", "english"),
        [this](NextUpdaterEvent event) {
            analytics_->SendAnalyticsLog(AnalyticsLogType::Error, std::format("updater error, state: {}, error: {}", magic_enum::enum_name(event.state), event.error_description).c_str());
        },
        backend_address_resolver_);

    LOG(INFO) << "Updater GUI App result: " << magic_enum::enum_name(result.done_status);

    return result;
}
#endif

void ClientLauncher::CreateConsoleWindowAndRedirectOutput()
{
    AllocConsole();

    CPINFOEXA cp_info;
    if (GetCPInfoExA(CP_ACP, 0, &cp_info))
        SetConsoleOutputCP(cp_info.CodePage);

    FILE* dummy_file;
    freopen_s(&dummy_file, "CONOUT$", "w", stdout);
    freopen_s(&dummy_file, "CONOUT$", "w", stderr);
}

bool ClientLauncher::OnVideoModeFailed()
{
    hl_registry_->WriteInt("ScreenBPP", 32);
    hl_registry_->WriteInt("ScreenWidth", kDefaultWidth);
    hl_registry_->WriteInt("ScreenHeight", kDefaultHeight);
    hl_registry_->WriteString("EngineDLL", "hw.dll");

    return MessageBoxA(
        NULL,
        "The specified rendering mode is not supported.\n"
        "Restart the game?",
        kErrorTitle,
        MB_OKCANCEL | MB_ICONERROR | MB_ICONQUESTION | MB_DEFAULT_DESKTOP_ONLY) == IDOK;
}

void ClientLauncher::FixScreenResolution()
{
    int w = hl_registry_->ReadInt("ScreenWidth", kDefaultWidth);
    int h = hl_registry_->ReadInt("ScreenHeight", kDefaultHeight);

    if (w < kMinWidth || h < kMinHeight)
    {
        hl_registry_->WriteInt("ScreenWidth", kDefaultWidth);
        hl_registry_->WriteInt("ScreenHeight", kDefaultHeight);
    }
}

bool ClientLauncher::GlobalMutexCheck()
{
    DWORD result = WaitForSingleObject(global_win_mutex_, 0);
    if (result != WAIT_OBJECT_0 &&
        result != WAIT_ABANDONED &&
        !cmd_line_->CheckParm("-hijack"))
    {
        return false;
    }

    return true;
}

#ifdef GAMEANALYTICS_ENABLE

void ClientLauncher::InitializeAnalytics()
{
    std::string client_uid = user_info_client_->GetClientUid();

    gameanalytics::GameAnalytics::setEnabledErrorReporting(false);
    gameanalytics::GameAnalytics::disableDeviceInfo();
    gameanalytics::GameAnalytics::configureUserId(client_uid.c_str());
    gameanalytics::GameAnalytics::configureBuild(NEXT_CLIENT_BUILD_VERSION);
    gameanalytics::GameAnalytics::initialize(GAMEANALYTICS_GAME_KEY, GAMEANALYTICS_SECRET_KEY);

    analytics_->SendBranchEvent();
    analytics_->SendPrimaryBackendEvent();
    analytics_->SendActualBackendEvent();
}

void ClientLauncher::UninitializeAnalytics()
{
    gameanalytics::GameAnalytics::onQuit();
}

#else

void ClientLauncher::InitializeAnalytics() { }
void ClientLauncher::UninitializeAnalytics() { }

#endif

#ifdef SENTRY_ENABLE

void ClientLauncher::InitializeSentry()
{
    CHAR exe_file_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_file_path, MAX_PATH);

    std::string client_uid = user_info_client_->GetClientUid();

    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, SENTRY_UPLOAD_URL);
    sentry_options_set_handler_path(options, "crashpad_handler.exe");
    sentry_options_set_database_path(options, "crashes");
    sentry_options_set_release(options, SENTRY_PROJECT_NAME "@" NEXT_CLIENT_BUILD_VERSION);
    sentry_options_set_environment(options, SENTRY_ENV);
    sentry_options_set_max_breadcrumbs(options, 100);

    sentry_options_add_attachment(options, "nitro_api.log");
    sentry_options_add_attachment(options, "setting_guard.ini");
    sentry_options_add_attachment(options, "user_game_config.ini");
    sentry_options_add_attachment(options, "launcher.log");
    sentry_options_add_attachment(options, "platform\\config\\backend.json");

    int result = sentry_init(options);

    sentry_value_t user = sentry_value_new_object();
    sentry_value_set_by_key(user, "id", sentry_value_new_string(client_uid.c_str()));
    sentry_value_set_by_key(user, "ip_address", sentry_value_new_string("{{auto}}"));
    sentry_value_set_by_key(user, "Game Dir", sentry_value_new_string(exe_file_path));
    sentry_set_user(user);

    if (result == 0)
        sentry_init_log_ = "Sentry started";
    else
        sentry_init_log_ = std::format("Sentry error: {}", GetWinErrorString(GetLastError()));

}

void ClientLauncher::UninitializeSentry()
{
    sentry_close();
}

#else

void ClientLauncher::InitializeSentry() { }
void ClientLauncher::UninitializeSentry() { }

#endif

void ClientLauncher::SDL_DestroyWindowHandler(nitroapi::NitroApiInterface* nitro_api)
{
    nitroapi::IEngine* eng = nitro_api->GetEngineData()->eng;
    if (eng->GetQuitting() == nitroapi::IEngine::QUIT_NOTQUITTING)
    {
        eng->SetQuitting(nitroapi::IEngine::QUIT_TODESKTOP);
        nitro_api->WriteLog(NITRO_API_LOG_TAG, "[Repair] Hangs on exit");
        analytics_->SendCrashMonitoringEvent("launcher", "hangs_on_exit", false);
    }
}

void ClientLauncher::HUD_InitHandler()
{
    analytics_->SendAnalyticsEvent("startup_hud_init_post");
}

void ClientLauncher::InitializeCmdLine(const char* cmd_line)
{
    std::string launch_parameters = config_provider_->get_value_string("launch_parameters", "");

    cmd_line_ = std::make_shared<CCommandLine>();
    cmd_line_->CreateCmdLine(std::format("{} {}", cmd_line, launch_parameters).c_str());

    if (config_provider_->get_value_int("stretch_aspect", 0))
    {
        cmd_line_->AppendParm("-stretchaspect", nullptr);
    }
}

void ClientLauncher::ModifyCmdLineAfterRestart(const char* cmd_line)
{
    static const char* kRemoveParams[] = {
        "-sw",
        "-startwindowed",
        "-windowed",
        "-window",
        "-full",
        "-fullscreen",
        "-soft",
        "-software",
        "-gl",
        "-w",
        "-width",
        "-h",
        "-height",
        "+connect"
    };

    for (const auto& param : kRemoveParams)
    {
        cmd_line_->RemoveParm(param);
    }

    if (strstr(cmd_line, "-game"))
    {
        cmd_line_->RemoveParm("-game");
    }

    if (strstr(cmd_line, "+load"))
    {
        cmd_line_->RemoveParm("+load");
    }

    if (config_provider_->get_value_int("stretch_aspect", 0))
    {
        cmd_line_->AppendParm("-stretchaspect", nullptr);
    }
    else
    {
        cmd_line_->RemoveParm("-stretchaspect");
    }

    cmd_line_->SetParm("-novid", nullptr);

    cmd_line_->AppendParm(cmd_line, nullptr);
}

void ClientLauncher::CheckVideoModeCrash()
{
    if (hl_registry_->ReadInt("CrashInitializingVideoMode", 0) == 0)
    {
        return;
    }

    hl_registry_->WriteInt("CrashInitializingVideoMode", 0);

    if (MessageBoxA(
        NULL,
        "It looks like a previous attempt to run the game failed due to a rendering subsystem error.\n"
        "Reset the game resolution settings and run the game again?",
        kErrorTitle,
        MB_OKCANCEL | MB_ICONERROR | MB_ICONQUESTION | MB_DEFAULT_DESKTOP_ONLY) != IDOK)
    {
        return;
    }

    hl_registry_->WriteInt("ScreenBPP", 32);
    hl_registry_->WriteInt("ScreenHeight", kDefaultWidth);
    hl_registry_->WriteInt("ScreenWidth", kDefaultHeight);
}

void ClientLauncher::Sys_ErrorHandler(const char* error)
{
    analytics_->SendCrashMonitoringEvent("Sys_Error", error, true);
}

void ClientLauncher::LogLoadedModules()
{
    auto write_line = [](const char* module)
    {
        if (GetModuleHandleA(module) != nullptr)
        {
            LOG(WARNING) << "Module " << module << " already loaded.";
        }
    };

    write_line("hw.dll");
    write_line("cstrike\\cl_dlls\\client.dll");
    write_line("cstrike\\cl_dlls\\gameui.dll");
    write_line("cstrike\\cl_dlls\\client_mini.dll");
    write_line("cstrike\\dlls\\mp.dll");
    write_line("filesystem_proxy.dll");
    write_line("next_engine_mini.dll");
    write_line("platform\\steam\\steam_api_orig.dll");
    write_line("steam_api_orig.dll");
}

std::string ClientLauncher::CreateVersionsString(nitroapi::NitroApiInterface* nitro_api,
                                                 EngineMiniInterface* engine_mini,
                                                 ClientMiniInterface* client_mini,
                                                 IGameUINext* gameui_next)
{
    char nitroapi_version[64]; nitro_api->GetVersion(nitroapi_version, sizeof(nitroapi_version));
    char engine_mini_version[64]; engine_mini->GetVersion(engine_mini_version, sizeof(engine_mini_version));
    char client_mini_version[64]; client_mini->GetVersion(client_mini_version, sizeof(client_mini_version));
    char gameui_version[64]; gameui_next->GetVersion(nullptr, nullptr, nullptr, gameui_version, sizeof(gameui_version));

    char versions[512];
    V_snprintf(versions,
               sizeof(versions),
               "nitro_api: %s\nLauncher: %s\nnext_engine_mini: %s\nclient_mini: %s\nGameUI: %s\n",
               nitroapi_version, LAUNCHER_VERSION, engine_mini_version, client_mini_version, gameui_version);

    return versions;
}
