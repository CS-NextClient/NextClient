#include "ClientLauncher.h"

#include <clocale>
#include <easylogging++.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <magic_enum.hpp>
#include <string>
#include <thread>

#ifdef SENTRY_ENABLE
#include <sentry.h>
#endif

#ifdef UPDATER_ENABLE
#include <updater/updater_gui_app.h>
#endif

#include <engine_launcher_api.h>
#include <nitroapi/NitroApiInterface.h>
#include <steam_api_proxy/next_steam_api_proxy.h>
#include <nitro_utils/config_utils.h>
#include <nitro_utils/string_utils.h>
#include <utils/platform.h>
#include <saferesult/Result.h>
#include <next_launcher/version.h>

#include "Analytics.h"
#include "DefaultUserInfo.h"
#include "exception_handler.h"
#include "RegistryUserStorage.h"
#include "EngineCommons.h"

INITIALIZE_EASYLOGGINGPP

using namespace saferesult;
namespace fs = std::filesystem;

#define ERROR_TITLE "Counter-Strike Launcher"

const int MIN_WIDTH = 640;
const int MIN_HEIGHT = 480;

const int DEFAULT_WIDTH = 800;
const int DEFAULT_HEIGHT = 600;

using namespace nitro_utils;
using namespace std::chrono_literals;

ClientLauncher::ClientLauncher(HINSTANCE module_instance, const char* cmd_line) :
    module_instance_(module_instance)
{
    next_client_version_ = { NEXT_CLIENT_BUILD_VERSION_MAJOR, NEXT_CLIENT_BUILD_VERSION_MINOR, NEXT_CLIENT_BUILD_VERSION_PATCH, NEXT_CLIENT_BUILD_VERSION_PRERELEASE };

    user_storage_ = std::make_shared<RegistryUserStorage>("Software\\Valve\\Half-Life\\nextclient");
    user_info_ = std::make_shared<DefaultUserInfo>(user_storage_);
    user_info_client_ = std::make_shared<next_launcher::UserInfoClient>(user_info_.get());
    analytics_ = std::make_shared<Analytics>();
    config_provider_ = std::make_shared<FileConfigProvider>("user_game_config.ini");

    hl_registry_ = std::make_shared<CRegistry>("Software\\Valve\\Half-Life\\Settings");
    hl_registry_->Init();

    cmd_line_ = std::make_shared<CCommandLine>();
    cmd_line_->CreateCmdLine(std::format("{} {}", cmd_line, config_provider_->get_value_string("launch_parameters", "")).c_str());

    global_mutex_ = CreateMutexA(nullptr, FALSE, "ValveHalfLifeLauncherMutex");
    g_SaveFullDumps = cmd_line_->CheckParm("-fulldump");
}

ClientLauncher::~ClientLauncher()
{
    ReleaseMutex(global_mutex_);
    CloseHandle(global_mutex_);
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

    DWORD dwMutexResult = WaitForSingleObject(global_mutex_, 0);
    if (dwMutexResult != WAIT_OBJECT_0 && dwMutexResult != WAIT_ABANDONED && !cmd_line_->CheckParm("-hijack"))
    {
        MessageBoxA(NULL, "The game could not be started because it is already running. \nIf it is not, then end the process in the task manager.", ERROR_TITLE, MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
        UninitializeAnalytics();
        UninitializeSentry();
        return;
    }

    analytics_->SendAnalyticsEvent("startup_init_post_mutex");

#ifdef UPDATER_ENABLE
    UpdaterDoneStatus updater_done = UpdaterDoneStatus::RunGame;

    if (!cmd_line_->CheckParm("-noupdate"))
    {
        auto result = RunUpdater();
        updater_done = std::get<0>(result);
        available_branches_ = std::get<1>(result);
    }

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
        cmd_line_->AppendParm("-noupdate", nullptr);

        std::string process_name = GetCurrentProcessPath().filename().replace_extension("").string() + "_new.exe";

        PROCESS_INFORMATION process_information;
        STARTUPINFOA startupinfo;
        ZeroMemory(&startupinfo, sizeof(startupinfo));
        bool result = CreateProcessA(process_name.c_str(), (char *) cmd_line_->GetCmdLine(), nullptr, nullptr, false, NORMAL_PRIORITY_CLASS, nullptr, nullptr, &startupinfo, &process_information);
        LOG_IF(!result, ERROR) << "Can't CreateProcessA for new launcher: " << process_name << ". Error: " << GetWinErrorString(GetLastError());
    }
#endif
}

void ClientLauncher::RunEngine()
{
    analytics_->SendAnalyticsEvent("startup_run_engine");

    char szPostRestartCmdLineArgs[4096] = { '\0' };
    bool bRunEngine = true;

    if (!cmd_line_->CheckParm("-game"))
        cmd_line_->AppendParm("-game", "cstrike");

    // disable glBlitFramebuffer feature, because this makes blackscreen on some Nvidia GPU when enables MSSA technology
    if (!cmd_line_->CheckParm("-nodirectblit") && !cmd_line_->CheckParm("-directblit"))
        cmd_line_->AppendParm("-nodirectblit", nullptr);

    if (!cmd_line_->CheckParm("-num_edicts"))
        cmd_line_->AppendParm("-num_edicts", "4096");

    if (hl_registry_->ReadInt("CrashInitializingVideoMode", 0))
    {
        hl_registry_->WriteInt("CrashInitializingVideoMode", 0);

        if (MessageBoxA(NULL, "It looks like a previous attempt to run the game failed due to a rendering subsystem error.\nReset the game resolution settings and run the game again?",
                        ERROR_TITLE, MB_OKCANCEL | MB_ICONERROR | MB_ICONQUESTION | MB_DEFAULT_DESKTOP_ONLY) != IDOK)
            return;

        hl_registry_->WriteInt("ScreenBPP", 32);
        hl_registry_->WriteInt("ScreenHeight", DEFAULT_WIDTH);
        hl_registry_->WriteInt("ScreenWidth", DEFAULT_HEIGHT);
    }

    while (bRunEngine)
    {
        auto [nitro_api, nitro_api_module] = LoadModule<nitroapi::NitroApiInterface>("nitro_api2.dll", NITROAPI_INTERFACE_VERSION);
        if (nitro_api == nullptr)
            break;

        LogLoadedModulesWarn(nitro_api);

        auto [filesystem, filesystem_module] = LoadModule<IFileSystem>("filesystem_proxy.dll", FILESYSTEM_INTERFACE_VERSION);
        if (filesystem == nullptr)
            break;

        auto [engine_mini, engine_mini_module] = LoadModule<EngineMiniInterface>("next_engine_mini.dll", ENGINE_MINI_INTERFACE_VERSION);
        if (engine_mini == nullptr)
            break;

        auto [client_mini, client_mini_module] = LoadModule<ClientMiniInterface>("cstrike\\cl_dlls\\client_mini.dll", CLIENT_MINI_INTERFACE_VERSION);
        if (client_mini == nullptr)
            break;

        auto [gameui_next, gameui_next_module] = LoadModule<IGameUINext>("cstrike\\cl_dlls\\gameui.dll", GAMEUI_NEXT_INTERFACE_VERSION);
        if (gameui_next == nullptr)
            break;

        CSysModule* steam_proxy_module = Sys_LoadModule("steam_api.dll");
        if (steam_proxy_module == nullptr)
        {
            std::string error = "Module steam_api.dll not found";
            analytics_->SendCrashMonitoringEvent("LoadModule Error", error.c_str(), true);
            MessageBoxA(NULL, error.c_str(), ERROR_TITLE, MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
            break;
        }

        auto steam_proxy_set_seh = (NextSteamProxy_SetSEHFunc)GetProcAddress((HMODULE)steam_proxy_module, "NextSteamProxy_SetSEH");
        if (steam_proxy_set_seh == nullptr)
        {
            std::string error = "NextSteamProxy_SetSEH not found in steam_api.dll.\nMake sure you use the steam_api.dll from NextClient and not the original steam_api.dll";
            analytics_->SendCrashMonitoringEvent("LoadModule Error", error.c_str(), true);
            MessageBoxA(NULL, error.c_str(), ERROR_TITLE, MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
            break;
        }
        steam_proxy_set_seh(ExceptionHandler);

        filesystem->Mount();
        filesystem->AddSearchPath("", "ROOT");

        nitro_api->WriteLog(LOG_TAG, sentry_init_log_.c_str());
        nitro_api->SetSEHCallback(ExceptionHandler);
        nitro_api->Initialize(cmd_line_.get(), filesystem, hl_registry_.get());

        std::vector<std::shared_ptr<nitroapi::Unsubscriber>> unsubscribers;

        unsubscribers.emplace_back(nitro_api->GetEngineData()->Sys_Error |= [this](const char* error, const auto& next) { Sys_ErrorHandler(error); next->Invoke(error); });

        auto [engine, engine_module] = LoadModule<IEngineAPI>(kEngineDll, VENGINE_LAUNCHER_API_VERSION);
        if (engine == nullptr)
            break;

        std::string versions = CreateVersionsString(nitro_api, engine_mini, client_mini, gameui_next);
        nitro_api->WriteLog(LOG_TAG, versions.c_str());

        client_mini->Init(nitro_api);
        engine_mini->Init(nitro_api, next_client_version_, analytics_.get());

        //hangs on exit fix
        unsubscribers.emplace_back(nitro_api->GetSDL2Data()->SDL_DestroyWindowFunc += [this, napi = nitro_api](void* window)
        {
            nitroapi::IEngine* eng = napi->GetEngineData()->eng;
            if (eng->GetQuitting() == nitroapi::IEngine::QUIT_NOTQUITTING)
            {
                eng->SetQuitting(nitroapi::IEngine::QUIT_TODESKTOP);
                napi->WriteLog(LOG_TAG, "[Repair] Hangs on exit");
                analytics_->SendCrashMonitoringEvent("launcher", "hangs_on_exit", false);
            }
        });

        unsubscribers.emplace_back(nitro_api->GetClientData()->HUD_Init += [this]() { analytics_->SendAnalyticsEvent("startup_hud_init_post"); });

        LOG(INFO) << "IEngineAPI::Run";
        analytics_->AddBreadcrumb("Info", "IEngineAPI::Run");

        EngineCommons::Init(nitro_api, user_info_client_, versions, available_branches_);

        EngineRunResult eRunResult = engine->Run(
            module_instance_,
            "",
            cmd_line_->GetCmdLine(),
            szPostRestartCmdLineArgs,
            Sys_GetFactoryThis(),
            Sys_GetFactory(filesystem_module));

        EngineCommons::Reset();

        LOG(INFO) << "IEngineAPI::Run done";
        analytics_->AddBreadcrumb("Info", std::format("IEngineAPI::Run done, result: {}", (int)eRunResult).c_str());

        Sys_UnloadModule(steam_proxy_module);

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

        switch (eRunResult)
        {
            case ENGRUN_QUITTING:
            {
                bRunEngine = false;
                break;
            }
            case ENGRUN_UNSUPPORTED_VIDEOMODE:
            {
                bRunEngine = OnVideoModeFailed();
                break;
            }
            default:
            {
                bRunEngine = true;
                break;
            }
        }

        static const char* szRemoveParams[] = { "-sw", "-startwindowed", "-windowed", "-window", "-full", "-fullscreen", "-soft", "-software", "-gl", "-w", "-width", "-h", "-height", "+connect" };

        for (int i = 0; i < sizeof(szRemoveParams) / sizeof(szRemoveParams[0]); i++)
            cmd_line_->RemoveParm(szRemoveParams[i]);
        cmd_line_->SetParm("-novid", nullptr);

        if (strstr(szPostRestartCmdLineArgs, "-game"))
            cmd_line_->RemoveParm("-game");

        if (strstr(szPostRestartCmdLineArgs, "+load"))
            cmd_line_->RemoveParm("+load");

        cmd_line_->AppendParm(szPostRestartCmdLineArgs, nullptr);

#ifdef UPDATER_ENABLE
        if (bRunEngine && !cmd_line_->CheckParm("-noupdate"))
        {
            auto result = RunUpdater();
            UpdaterDoneStatus updater_done = std::get<0>(result);
            available_branches_ = std::get<1>(result);

            // in case of RunNewGame just closing the game, because need more complicated logic to support this
            bRunEngine = updater_done == UpdaterDoneStatus::RunGame;
        }
#endif
    }
}

#ifdef UPDATER_ENABLE
std::tuple<UpdaterDoneStatus, std::vector<BranchEntry>> ClientLauncher::RunUpdater()
{
    LOG(INFO) << "Run updater";
    analytics_->SendAnalyticsEvent("startup_run_updater");

    auto result = RunUpdaterGuiApp(
        UPDATER_SERVICE_URL,
        user_info_client_,
        el::Loggers::getLogger(ELPP_DEFAULT_LOGGER),
        config_provider_->get_value_string("language", "english"),
        [this](NextUpdaterEvent event) {
            analytics_->SendAnalyticsLog(AnalyticsLogType::Error, std::format("updater error, state: {}, error: {}", magic_enum::enum_name(event.state), event.error_description).c_str());
        });

    LOG(INFO) << "Updater done: " << magic_enum::enum_name(std::get<0>(result));

    return result;
}
#endif

ResultT<bool> FinishLauncherUpdate()
{
    auto current_path = GetCurrentProcessPath();

    std::string filename = current_path.filename().string();

    if (filename.ends_with("_new.exe"))
    {
        std::string new_filename = filename;
        nitro_utils::replace_all(new_filename, "_new.exe", ".exe");

        auto copy_to_path = current_path;
        copy_to_path.replace_filename(new_filename);

        // do a few tries to copy cs_new.exe to cs.exe because cs.exe may not be closed immediately after it runs new_cs.exe
        std::error_code ec_copy_file;
        for (int i = 0; i < 5; i++)
        {
            if (fs::copy_file(current_path, copy_to_path, std::filesystem::copy_options::overwrite_existing, ec_copy_file))
                break;

            std::this_thread::sleep_for(200ms);
        }

        if (ec_copy_file)
            return ResultError(std::format("Can't copy {} to {}: {}", current_path.string(), copy_to_path.string(), ec_copy_file.message()));
    }
    else
    {
        std::string filename_to_delete = current_path.filename().replace_extension("").string() + "_new.exe";

        std::error_code ec_remove_file;
        if (!fs::remove(filename_to_delete, ec_remove_file))
        {
            if (ec_remove_file)
                return ResultError(std::format("Can't delete {}: {}", filename_to_delete, ec_remove_file.message()));

            return false;
        }
    }

    return true;
}

void ClientLauncher::CreateConsoleWindowAndRedirectOutput()
{
    AllocConsole();

    CPINFOEXA cp_info;
    if (GetCPInfoExA(CP_ACP, 0, &cp_info))
        SetConsoleOutputCP(cp_info.CodePage);

    FILE* dummy_file;
    freopen_s(&dummy_file, "CONOUT$", "w", stdout);
    freopen_s(&dummy_file, "CONOUT$", "w", stderr);
    //freopen_s(&dummy_file, "CONIN$", "r", stdin);
}

bool ClientLauncher::OnVideoModeFailed()
{
    hl_registry_->WriteInt("ScreenBPP", 32);
    hl_registry_->WriteInt("ScreenWidth", DEFAULT_WIDTH);
    hl_registry_->WriteInt("ScreenHeight", DEFAULT_HEIGHT);
    hl_registry_->WriteString("EngineDLL", "hw.dll");

    return MessageBoxA(NULL, "The specified rendering mode is not supported.\nRestart the game?", ERROR_TITLE, MB_OKCANCEL | MB_ICONERROR | MB_ICONQUESTION | MB_DEFAULT_DESKTOP_ONLY) == IDOK;
}

void ClientLauncher::FixScreenResolution()
{
    int w = hl_registry_->ReadInt("ScreenWidth", DEFAULT_WIDTH);
    int h = hl_registry_->ReadInt("ScreenHeight", DEFAULT_HEIGHT);

    if (w < MIN_WIDTH || h < MIN_HEIGHT)
    {
        hl_registry_->WriteInt("ScreenWidth", DEFAULT_WIDTH);
        hl_registry_->WriteInt("ScreenHeight", DEFAULT_HEIGHT);
    }
}

#ifdef GAMEANALYTICS_ENABLE

void ClientLauncher::InitializeAnalytics()
{

    std::string client_uid = user_info_client_->GetClientUid();
    std::string branch = user_info_client_->GetUpdateBranch();

    gameanalytics::StringVector dimensions01;
    dimensions01.add(branch.c_str());

    gameanalytics::GameAnalytics::setEnabledErrorReporting(false);
    gameanalytics::GameAnalytics::disableDeviceInfo();
    gameanalytics::GameAnalytics::configureAvailableCustomDimensions01(dimensions01);
    gameanalytics::GameAnalytics::configureUserId(client_uid.c_str());
    gameanalytics::GameAnalytics::configureBuild(NEXT_CLIENT_BUILD_VERSION);
    gameanalytics::GameAnalytics::initialize(GAMEANALYTICS_GAME_KEY, GAMEANALYTICS_SECRET_KEY);

    gameanalytics::GameAnalytics::setCustomDimension01(branch.c_str());
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

void ClientLauncher::Sys_ErrorHandler(const char* error)
{
    analytics_->SendCrashMonitoringEvent("Sys_Error", error, true);
}

void ClientLauncher::LogLoadedModulesWarn(nitroapi::NitroApiInterface* nitro_api)
{
    auto write_line = [nitro_api](const char* module) {
        if (GetModuleHandleA(module) != nullptr)
            nitro_api->WriteLog(LOG_TAG, std::format("Suspicious state, the module is already in memory: {}", module).c_str());
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

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR lpCmdLine, _In_ int nCmdShow)
{
    el::Configurations defaultConf;
    defaultConf.setToDefault();
    defaultConf.set(el::Level::Global, el::ConfigurationType::MaxLogFileSize, std::to_string(MAX_LOGFILE_SIZE));
    el::Loggers::reconfigureLogger(ELPP_DEFAULT_LOGGER, defaultConf);

    LOG(INFO) << "-----------------------------------------------";
    LOG(INFO) << "Start " << GetCurrentProcessPath().filename();
    LOG(INFO) << "Version: " << NEXT_CLIENT_BUILD_VERSION;

    setlocale(LC_CTYPE, "");
    setlocale(LC_TIME, "");
    setlocale(LC_COLLATE, "");
    setlocale(LC_MONETARY, "");

#ifdef UPDATER_ENABLE
    LOG(INFO) << "Finishing launcher update";
    ResultT<bool> finish_update_result = FinishLauncherUpdate();
    if (finish_update_result.has_error())
        LOG(WARNING) << "Finish launcher update error: " << finish_update_result.get_error();
    else
        LOG(INFO) << "Finishing launcher update: " << (*finish_update_result ? "done" : "nothing to do");
#endif

    {
        auto launcher = std::make_unique<ClientLauncher>(hInstance, GetCommandLineA());
        launcher->Run();
    }

    LOG(INFO) << "Exit";
    return EXIT_SUCCESS;
}
