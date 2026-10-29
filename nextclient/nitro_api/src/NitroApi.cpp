#include "NitroApi.h"

#include <ranges>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <easylogging++.h>

#include "nitroapi/handler_helper/Handler.h"
#include "FrameLifecycle.h"
#include "modules/windows/WindowsModule.h"
#include "modules/engine/EngineAddressProvider8684Windows.h"
#include "modules/engine/EngineAddressProvider8684Linux.h"
#include "modules/client/ClientAddressProvider8684Windows.h"
#include "modules/client/ClientAddressProvider8684Linux.h"
#include "modules/sdl2/SDL2AddressProvider.h"
#include "modules/sdl2/loaded_sdl_module.h"

#include <ncl_perf/profiler.h>

#ifndef NITRO_API_STATIC
INITIALIZE_EASYLOGGINGPP
#endif

namespace nitroapi
{
    static nitroapi::NitroApi g_NitroApi;
    NitroApiInterface* GetNitroApi() { return &g_NitroApi; }

    static GlobalExceptionHandlerFunc g_WriteMiniDumpCallback = nullptr;

#ifndef NITRO_API_STATIC
    EXPOSE_SINGLE_INTERFACE_GLOBALVAR(NitroApi, NitroApiInterface, NITROAPI_INTERFACE_VERSION, g_NitroApi);
    static IBaseInterface* CreateFrameLifecycleInterface()
    {
        return reinterpret_cast<IBaseInterface*>(nitroapi::FrameLifecycle_GetInterface());
    }
    EXPOSE_INTERFACE_FN(CreateFrameLifecycleInterface, FrameLifecycleInterface, NITRO_FRAME_LIFECYCLE_INTERFACE_VERSION);
#endif

    NitroApi::~NitroApi()
    {
        g_WriteMiniDumpCallback = nullptr;
    }

    void NitroApi::WriteLog(const char* tag, const char* message)
    {
        el::Loggers::getLogger(tag)->info(message);
    }

    void NitroApi::SetSEHCallback(GlobalExceptionHandlerFunc callback)
    {
        g_WriteMiniDumpCallback = callback;
    }

    GlobalExceptionHandlerFunc NitroApi::GetSEHCallback()
    {
        return g_WriteMiniDumpCallback;
    }

    bool NitroApi::Initialize(ICommandLine *command_line, IFileSystem* file_system, IRegistry *registry)
    {
        if (is_initialized_)
            return true;

        LOG(INFO) << "---------- Initialize | Begin ----------";

        hook_storage_ = std::make_shared<HookStorage>();

        auto windows_module = std::make_unique<WindowsModule>(hook_storage_, nullptr);
        windows_data_ = windows_module->GetWindowsData();
        modules_.emplace_back("<exe>", std::move(windows_module));

        RetrieveEngineBuildVersion();

        std::shared_ptr<AddressProviderBase> engine_provider = GetEngineAddressProvider();
        if (engine_provider != nullptr)
        {
            std::string engine_path = IsWindows() ? "hw.dll" : "hw.so";
            auto engine_module = std::make_unique<EngineModule>(hook_storage_, engine_provider, this, command_line, file_system, registry);
            engine_data_ = engine_module->GetEngineData();
            modules_.emplace_back(engine_path, std::move(engine_module));
        }
        else
        {
            LOG(INFO) << "Can't get engine address provider, engine hooks will not work";
        }

        std::shared_ptr<AddressProviderBase> client_provider = GetClientAddressProvider();
        if (client_provider != nullptr)
        {
            std::string client_path = IsWindows() ? "cstrike\\cl_dlls\\client.dll" : "cstrike/cl_dlls/client.so";
            auto client_module = std::make_unique<ClientModule>(hook_storage_, client_provider);
            client_data_ = client_module->GetClientData();
            modules_.emplace_back(client_path, std::move(client_module));
        }
        else
        {
            LOG(INFO) << "Can't get client address provider, client hooks will not work";
        }

        for (auto& [path, module]: modules_)
        {
            InvokeLibraryLoaded(path, module);
        }

        nitro_utils::SysModule engine_handle = nullptr;
        for (const auto& [path, module] : modules_)
        {
            if (path == "hw.dll" || path == "hw.so")
            {
                engine_handle = module.module_handle;
                break;
            }
        }

        nitro_utils::SysModule sdl_handle = nitroapi::sdl2::FindLoadedModule(engine_handle);
        if (sdl_handle)
        {
            auto sdl2_module = std::make_unique<SDL2Module>(hook_storage_, std::make_shared<SDL2AddressProvider>());
            sdl2_data_ = sdl2_module->GetSDL2Data();
            sdl2_module->OnLibraryLoaded(sdl_handle);
            modules_.emplace_back("<loaded SDL2>", std::move(sdl2_module));
            ModuleHookData& module = std::get<1>(modules_.back());
            module.module_handle = sdl_handle;
            module.borrowed = true;
        }

#if USE_PROFILER
        engine_data_->Host_FilterTime += [](float delta, int result) {
            if (result)
            {
                NCL_PROFILE_VALUE("Simulation delta ms", double(delta) * 1000.0);
            }
        };
#endif

        LOG(INFO) << "---------- Initialize | End ----------";

        is_initialized_ = true;
        return true;
    }

    bool NitroApi::UnInitialize()
    {
        if (!is_initialized_)
            return true;

        is_initialized_ = false;

        nitroapi::FrameLifecycle_ClearObservers();

        LOG(INFO) << "---------- UnInitialize | Begin ----------";

        for (auto& [path, module]: modules_ | std::views::reverse)
            InvokeLibraryUnloading(path, module);

        modules_.clear();
        hook_storage_.reset();

        LOG(INFO) << "---------- UnInitialize | End ----------";

        return true;
    }

    void NitroApi::InvokeLibraryLoaded(const std::string& module_path, ModuleHookData& module_hook_data)
    {
        nitro_utils::SysModule hModule;

        if (module_path == "<exe>")
            hModule = nitro_utils::GetSysModule(nullptr);
        else
            hModule = nitro_utils::LoadSysModule(module_path.c_str());

        if (hModule == nullptr)
        {
            LOG(INFO) << "Module loading " << module_path << " failed (nitro_utils::GetSysModule/nitro_utils::LoadSysModule returns null)";
            return;
        }

        LOG(INFO) << "Module loading " << module_path << " | Begin";

        module_hook_data.module_handle = hModule;
        module_hook_data.module->OnLibraryLoaded(hModule);

        LOG(INFO) << "Module loading " << module_path << " | End";
    }

    void NitroApi::InvokeLibraryUnloading(const std::string& module_path, ModuleHookData& module_hook_data)
    {
        LOG(INFO) << "Module unloading " << module_path << " | Begin";

        module_hook_data.module->OnLibraryUnload();

        if (module_path != "<exe>" && !module_hook_data.borrowed)
            nitro_utils::UnloadSysModule(module_hook_data.module_handle);

        module_hook_data.module_handle = nullptr;

        LOG(INFO) << "Module unloading " << module_path << " | End";
    }

    void NitroApi::GetVersion(char* buffer, int size)
    {
        if (buffer != nullptr)
            V_strncpy(buffer, NITROAPI_INTERFACE_VERSION ", " __DATE__ " " __TIME__, size);
    }


    void NitroApi::RetrieveEngineBuildVersion()
    {
        std::string engine_path = IsWindows() ? "hw.dll" : "hw.so";

        std::ifstream engine_file(engine_path, std::ifstream::ate | std::ifstream::binary);
        if (!engine_file.is_open())
        {
            LOG(INFO) << "Can't open " << engine_path << " to retrieve engine build version";
            return;
        }

        size_t engine_size = engine_file.tellg();
        if (engine_size == 1641376 || engine_size == 9330714)
            build_version_ = BuildVersion{8684};
        else
            LOG(INFO) << "Unsupported engine version";
    }

    std::shared_ptr<AddressProviderBase> NitroApi::GetEngineAddressProvider()
    {
        if (build_version_.build_number != 8684)
            return nullptr;

        std::shared_ptr<AddressProviderBase> address_provider;

        if (IsWindows())
            address_provider = std::make_shared<EngineAddressProvider8684Windows>();
        else
            address_provider = std::make_shared<EngineAddressProvider8684Linux>();

        return address_provider;
    }

    std::shared_ptr<AddressProviderBase> NitroApi::GetClientAddressProvider()
    {
        if (build_version_.build_number != 8684)
            return nullptr;

        std::shared_ptr<AddressProviderBase> address_provider;

        if (IsWindows())
            address_provider = std::make_shared<ClientAddressProvider8684Windows>();
        else
            address_provider = std::make_shared<ClientAddressProvider8684Linux>();

        return address_provider;
    }

#ifdef _WIN32
    LONG WINAPI GlobalExceptionHandlerWin32(EXCEPTION_POINTERS* exception_pointers)
    {
        if (g_WriteMiniDumpCallback != nullptr)
            g_WriteMiniDumpCallback(exception_pointers);

        return EXCEPTION_CONTINUE_SEARCH;
    }
#endif
}
