#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

#include <nitroapi/hook/StaticHookId.h>
#include <nitroapi/modules/sdl2/SDL2Data.h>

#include "modules/sdl2/loaded_sdl_module.h"
#include "modules/sdl2/SDL2AddressProvider.h"
#include "modules/sdl2/SDL2Module.h"

#ifndef _WIN32
// The SDK aliases the Win32 name, masking the qualified utility function.
#undef GetProcAddress
#endif

namespace
{
    class LegacyPresentationTest : public testing::Test
    {
    private:
        nitro_utils::SysModule library_{};
        std::shared_ptr<nitroapi::HookStorage> hooks_ = std::make_shared<nitroapi::HookStorage>();
        nitroapi::SDL2Module module_{hooks_, std::make_shared<nitroapi::SDL2AddressProvider>()};

    protected:
        std::shared_ptr<nitroapi::SDL2Data> data = module_.GetSDL2Data();

        void* ReadDeletedContext()
        {
            using ReadContextFn = void* (*)();
            ReadContextFn read = reinterpret_cast<ReadContextFn>(nitro_utils::GetProcAddress(library_, "PerfTest_DeletedContext"));
            return read();
        }

        void SetUp() override
        {
            library_ = nitro_utils::LoadSysModule(PERF_TEST_LEGACY_SDL_PATH);
            ASSERT_NE(library_, nullptr);
            module_.OnLibraryLoaded(library_);
        }

        void TearDown() override
        {
            module_.OnLibraryUnload();
            DeleteChain(data->SDL_DestroyWindowFunc);
            DeleteChain(data->SDL_CreateWindowFunc);
            DeleteChain(data->SDL_GetErrorFunc);
            DeleteChain(data->SDL_GL_GetProcAddress);
            DeleteChain(data->SDL_GL_SwapWindow);
            DeleteChain(data->SDL_GL_GetDrawableSize);
            DeleteChain(data->SDL_GL_GetCurrentWindow);
            DeleteChain(data->SDL_GetWindowSize);
            DeleteChain(data->SDL_GetVersion);
            DeleteChain(data->SDL_GL_CreateContext);
            DeleteChain(data->SDL_GL_DeleteContext);
            DeleteChain(data->SDL_GL_GetCurrentContext);
            nitro_utils::UnloadSysModule(library_);
        }

    private:
        template <class TResult, class... TArgs>
        void DeleteChain(nitroapi::NitroFunctionBase<TResult, TArgs...>& function)
        {
            nitroapi::HandlersChain<TResult, TArgs...>* chain =
                static_cast<nitroapi::HandlersChain<TResult, TArgs...>*>(function.GetChain());
            function.SetChain(nullptr);
            delete chain;
        }
    };
} // namespace

TEST(PresentationBoundaryTest, ExistingAbiOffsetsAndHookIdsStayStable)
{
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_DestroyWindow), 1500);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_CreateWindow), 1501);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_GetError), 1502);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_GL_GetProcAddress), 1503);
    EXPECT_EQ(int(nitroapi::StaticHookId::VGui_InitInterfacesList), 1504);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_DestroyWindowFunc), 0u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_CreateWindowFunc), 8u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GetErrorFunc), 16u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GL_GetProcAddress), 24u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GL_SwapWindow), 32u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GL_GetDrawableSize), 40u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GL_GetCurrentWindow), 48u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GetWindowSize), 56u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GetVersion), 64u);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_GL_SwapWindow), 1505);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_GetWindowSize), 1508);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_GetVersion), 1509);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GL_CreateContext), 72u);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GL_DeleteContext), 80u);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_GL_CreateContext), 1510);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_GL_DeleteContext), 1511);
    EXPECT_EQ(offsetof(nitroapi::SDL2Data, SDL_GL_GetCurrentContext), 88u);
    EXPECT_EQ(int(nitroapi::StaticHookId::SDL_GL_GetCurrentContext), 1512);
}

TEST(PresentationBoundaryTest, LoadedSdlIdentityIsBorrowed)
{
    nitro_utils::SysModule engine = nitro_utils::LoadSysModule(PERF_TEST_ENGINE_PATH);
    ASSERT_NE(engine, nullptr);
    nitro_utils::SysModule first = nitroapi::sdl2::FindLoadedModule(engine);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(nitroapi::sdl2::FindLoadedModule(engine), first);
    EXPECT_NE(nitro_utils::GetProcAddress(first, "SDL_GL_SwapWindow"), nullptr);
    EXPECT_NE(nitro_utils::GetProcAddress(first, "PerfTest_DeletedContext"), nullptr);
    nitro_utils::UnloadSysModule(engine);
}

TEST_F(LegacyPresentationTest, MissingExportsHaveNoUsableHook)
{
    EXPECT_EQ(data->SDL_GL_GetDrawableSize.GetHook(), nullptr);
    EXPECT_EQ(data->SDL_GL_GetCurrentWindow.GetHook(), nullptr);
    EXPECT_EQ(data->SDL_GL_GetCurrentContext.GetHook(), nullptr);
    ASSERT_NE(data->SDL_GL_SwapWindow.GetHook(), nullptr);
    ASSERT_NE(data->SDL_GetWindowSize.GetHook(), nullptr);
}

TEST_F(LegacyPresentationTest, WindowSizeAndVersionResolveThroughProductionRegistration)
{
    ASSERT_NE(data->SDL_GetWindowSize.GetHook(), nullptr);
    ASSERT_NE(data->SDL_GetVersion.GetHook(), nullptr);
    int width = 0;
    int height = 0;
    data->SDL_GetWindowSize(nullptr, &width, &height);
    EXPECT_EQ(width, 640);
    EXPECT_EQ(height, 480);
    unsigned char version[3]{};
    data->SDL_GetVersion(version);
    EXPECT_EQ(version[0], 2u);
    EXPECT_EQ(version[1], 0u);
    EXPECT_EQ(version[2], 0u);
}

TEST_F(LegacyPresentationTest, ContextLifecycleUsesProductionHooksAndForwardsArguments)
{
    ASSERT_NE(data->SDL_GL_CreateContext.GetHook(), nullptr);
    ASSERT_NE(data->SDL_GL_DeleteContext.GetHook(), nullptr);
    int window = 0;
    void* context = data->SDL_GL_CreateContext(&window);
    EXPECT_EQ(context, &window);
    void* forwarded = nullptr;
    std::shared_ptr<nitroapi::Unsubscriber> subscription = data->SDL_GL_DeleteContext |= [&](void* value, const auto& next) {
        forwarded = value;
        next->Invoke(value);
    };
    ASSERT_TRUE(data->SDL_GL_DeleteContext.GetHook()->IsHookActive());
    data->SDL_GL_DeleteContext.InvokeChained(context);
    subscription->Unsubscribe();
    EXPECT_EQ(forwarded, context);
    EXPECT_EQ(ReadDeletedContext(), context);
}
