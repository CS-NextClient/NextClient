#pragma once

#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "engine.h"

namespace studio_test
{
    extern std::unordered_map<std::string, std::string> g_Files;

    struct FileReadProbe
    {
        std::string path;
        std::optional<unsigned int> size_override;
        bool open_failed{};
        const std::string* opened{};
        int opens{};
        int sizes{};
        int reads{};
        int closes{};
        std::function<void()> after_read;
    };

    extern FileReadProbe* g_FileReadProbe;

    struct ModelBlock
    {
        goldsrc::studiohdr_t header{};
        goldsrc::mstudiobodyparts_t body{};
        goldsrc::mstudiomodel_t submodels[65]{};
        goldsrc::mstudiotexture_t textures[2]{};
        goldsrc::mstudioseqdesc_t sequences[3]{};
        short skins[2] = {0, 1};

        ModelBlock()
        {
            header.version = 10;
            header.length = sizeof(*this) + 4096;
            header.texturedataindex = sizeof(*this);
            header.numbones = 1;
            header.numseq = 3;
            header.seqindex = offsetof(ModelBlock, sequences);
            header.bbmin[0] = -8;
            header.bbmax[2] = 24;
            header.numbodyparts = 1;
            header.bodypartindex = offsetof(ModelBlock, body);
            body.base = 1;
            body.nummodels = 2;
            body.modelindex = offsetof(ModelBlock, submodels);
            std::strcpy(submodels[0].name, "body");
            std::strcpy(submodels[1].name, "alternate");
            header.textureindex = offsetof(ModelBlock, textures);
            header.numtextures = 2;
            header.numskinref = 1;
            header.numskinfamilies = 2;
            header.skinindex = offsetof(ModelBlock, skins);

            for (int index = 0; index < 2; index++)
            {
                std::strcpy(textures[index].name, index == 0 ? "base.bmp" : "alternate.bmp");
                textures[index].width = 16;
                textures[index].height = 16;
            }
        }
    };

    template <class Result, class... Args>
    class Chain : public nitroapi::HandlersChainInterface<Result, Args...>
    {
    public:
        void DetachHook() override {}
        bool IsHookAttached() override
        {
            return false;
        }
        uint32_t GetSubscribersCount() const override
        {
            return 0;
        }
        uint32_t GetHandlersCount() const override
        {
            return 0;
        }
        void AttachHook(nitroapi::GenericHookInterface<Result, Args...>*) override {}
        nitroapi::GenericHookInterface<Result, Args...>* GetHook() const override
        {
            return nullptr;
        }
        bool AddHandler(nitroapi::HandlerInterface<Result, Args...>*) override
        {
            return false;
        }
        bool RemoveHandler(nitroapi::HandlerInterface<Result, Args...>*) override
        {
            return false;
        }
        bool Subscribe(nitroapi::SubscriberInterface<Result, Args...>*) override
        {
            return false;
        }
        bool Unsubscribe(nitroapi::SubscriberInterface<Result, Args...>*) override
        {
            return false;
        }
    };

    template <class Result, class... Args>
    class Hook : public nitroapi::GenericHookInterface<Result, Args...>
    {
    public:
        std::function<Result(Args...)> callback;
        void UnsetCallbackAndDisableHook() override {}
        uint32_t GetTrampolineAddress() const override
        {
            return 0;
        }
        uint32_t GetOriginalFunctionAddress() const override
        {
            return 0;
        }
        bool IsHookActive() const override
        {
            return false;
        }
        void SetCallbackAndEnableHook(std::function<Result(Args...)>&&) override {}
        Result InvokeOriginal(Args... args) override
        {
            return callback(args...);
        }
        Result Invoke(Args... args) override
        {
            return callback(args...);
        }
    };

    class ModelEngine : public nitroapi::NitroApiInterface
    {
    private:
        nitroapi::EngineData data_;
        Chain<void*, goldsrc::cache_user_t*> check_chain_;
        Chain<void, goldsrc::cache_user_t*> free_chain_;

    public:
        Hook<void*, goldsrc::cache_user_t*> check;
        Hook<void, goldsrc::cache_user_t*> free;

        ModelEngine()
        {
            check.callback = [](goldsrc::cache_user_t* cache) { return cache->data; };
            free.callback = [](goldsrc::cache_user_t* cache) { cache->data = nullptr; };
            data_.Cache_Check.SetChain(&check_chain_);
            data_.Cache_Check.SetHook(&check);
            data_.Cache_Free.SetChain(&free_chain_);
            data_.Cache_Free.SetHook(&free);
        }

        void WriteLog(const char*, const char*) override {}
        void SetSEHCallback(GlobalExceptionHandlerFunc) override {}
        GlobalExceptionHandlerFunc GetSEHCallback() override
        {
            return nullptr;
        }
        bool Initialize(ICommandLine*, IFileSystem*, IRegistry*) override
        {
            return false;
        }
        bool UnInitialize() override
        {
            return true;
        }
        bool IsInitialized() override
        {
            return false;
        }
        nitroapi::ClientData* GetClientData() override
        {
            return nullptr;
        }
        nitroapi::EngineData* GetEngineData() override
        {
            return &data_;
        }
        nitroapi::WindowsData* GetWindowsData() override
        {
            return nullptr;
        }
        nitroapi::SDL2Data* GetSDL2Data() override
        {
            return nullptr;
        }
        nitroapi::BuildVersion* GetBuildVersion() override
        {
            return nullptr;
        }
        void GetVersion(char*, int) override {}
    };
} // namespace studio_test
