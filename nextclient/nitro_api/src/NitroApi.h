#pragma once

#include <unordered_map>
#include <string>
#include <nitro_utils/platform.h>
#include <nitroapi/NitroApiInterface.h>
#include "HookStorage.h"
#include "modules/ModuleBase.h"
#include "modules/engine/EngineModule.h"
#include "modules/client/ClientModule.h"
#include "modules/sdl2/SDL2Module.h"


namespace nitroapi
{
    class NitroApi : public NitroApiInterface
    {
        struct ModuleHookData
        {
            nitro_utils::SysModule module_handle = nullptr;
            std::unique_ptr<ModuleBase> module = nullptr;

            explicit ModuleHookData(std::unique_ptr<ModuleBase>&& module) :
                module(std::move(module))
            { }
        };

        bool is_initialized_ = false;

        std::shared_ptr<HookStorage> hook_storage_{};
        std::vector<std::tuple<std::string, ModuleHookData>> modules_{};
        std::shared_ptr<ClientData> client_data_{};
        std::shared_ptr<EngineData> engine_data_{};
        std::shared_ptr<WindowsData> windows_data_{};
        std::shared_ptr<SDL2Data> sdl2_data_{};
        BuildVersion build_version_{};

    public:
        ~NitroApi() override;

        void WriteLog(const char* tag, const char* message) override;
        void SetSEHCallback(GlobalExceptionHandlerFunc callback) override;
        GlobalExceptionHandlerFunc GetSEHCallback() override;

        bool Initialize(ICommandLine *command_line, IFileSystem* file_system, IRegistry *registry) override;
        bool UnInitialize() override;
        bool IsInitialized() override { return is_initialized_; };

        ClientData* GetClientData() override { return client_data_.get(); }
        EngineData* GetEngineData() override { return engine_data_.get(); }
        WindowsData* GetWindowsData() override { return windows_data_.get(); }
        SDL2Data* GetSDL2Data() override { return sdl2_data_.get(); }
        BuildVersion* GetBuildVersion() override { return &build_version_; }

        void GetVersion(char* buffer, int size) override;

    private:
        void InvokeLibraryLoaded(const std::string& module_path, ModuleHookData& module_hook_data);
        static void InvokeLibraryUnloading(const std::string& module_path, ModuleHookData& module_hook_data);

        void SetupProfiler();
        void RetrieveEngineBuildVersion();
        std::shared_ptr<AddressProviderBase> GetEngineAddressProvider();
        std::shared_ptr<AddressProviderBase> GetClientAddressProvider();
    };
}
