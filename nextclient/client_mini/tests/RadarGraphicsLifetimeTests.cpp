#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "chain/HandlersChain.h"
#include "hud/radar/integration/graphics_retirement_hooks.h"
#include "RadarGlContext.h"
#include "RadarRecordingMonitor.h"

INITIALIZE_EASYLOGGINGPP

namespace
{
    template <class Result, class... Args>
    class NativeHook : public nitroapi::GenericHookInterface<Result, Args...>
    {
        std::function<Result(Args...)> callback_;

    public:
        std::function<Result(Args...)> original;
        void UnsetCallbackAndDisableHook() override
        {
            callback_ = {};
        }
        uint32_t GetTrampolineAddress() const override
        {
            return 1;
        }
        uint32_t GetOriginalFunctionAddress() const override
        {
            return 1;
        }
        bool IsHookActive() const override
        {
            return true;
        }
        void SetCallbackAndEnableHook(std::function<Result(Args...)>&& callback) override
        {
            callback_ = std::move(callback);
        }
        Result InvokeOriginal(Args... args) override
        {
            return original(args...);
        }
        Result Invoke(Args... args) override
        {
            return callback_ ? callback_(args...) : original(args...);
        }
    };

    struct RetirementOwner
    {
        RadarDrawContext& draw;
        std::vector<std::string> events;
        std::vector<std::shared_ptr<nitroapi::Unsubscriber>> subscriptions;
        void* current_context{};
        void* current_window{};
        bool detach_on_reset{};

        void Stop()
        {
            for (const auto& subscription : subscriptions)
            {
                subscription->Unsubscribe();
            }
            subscriptions.clear();
        }

        void ResetGraphics()
        {
            events.push_back("reset");
            draw.ResetGl(draw.is_context(current_context, current_window));
            if (detach_on_reset)
            {
                Stop();
            }
        }
    };
} // namespace

TEST(RadarGraphicsLifetime, RetirementFiltersForeignTokensInvalidatesBeforeForwardingAndDetaches)
{
    RadarRecordingMonitor monitor;
    radar::integration::Performance performance{&monitor};
    RadarDrawContext draw{performance};
    auto engine = std::make_unique<nitroapi::EngineData>();
    nitroapi::SDL2Data sdl;
    nitroapi::HandlersChain<goldsrc::qboolean, void*, HDC*, HGLRC*, const char*, const char*> mode_chain;
    NativeHook<goldsrc::qboolean, void*, HDC*, HGLRC*, const char*, const char*> mode_hook;
    nitroapi::HandlersChain<void, void*> context_chain;
    NativeHook<void, void*> context_hook;
    nitroapi::HandlersChain<void, void*> window_chain;
    NativeHook<void, void*> window_hook;
    engine->GL_SetMode.SetChain(&mode_chain);
    engine->GL_SetMode.SetHook(&mode_hook);
    sdl.SDL_GL_DeleteContext.SetChain(&context_chain);
    sdl.SDL_GL_DeleteContext.SetHook(&context_hook);
    sdl.SDL_DestroyWindowFunc.SetChain(&window_chain);
    sdl.SDL_DestroyWindowFunc.SetHook(&window_hook);
    int context_token = 1;
    int window_token = 2;
    int foreign_token = 3;
    RetirementOwner owner{draw};
    owner.current_context = &foreign_token;
    owner.current_window = &foreign_token;
    mode_hook.original = [&](void*, HDC*, HGLRC*, const char*, const char*) {
        EXPECT_FALSE(draw.owns_context(&context_token));
        owner.events.push_back("mode");
        return 7;
    };
    context_hook.original = [&](void* token) {
        if (token == &context_token)
        {
            EXPECT_FALSE(draw.owns_context(token));
        }
        owner.events.push_back("context");
    };
    window_hook.original = [&](void* token) {
        if (token == &window_token)
        {
            EXPECT_FALSE(draw.owns_window(token));
        }
        owner.events.push_back("window");
    };
    radar::integration::SubscribeGraphicsRetirement(owner, draw, *engine, &sdl, owner.subscriptions);
    draw.Prepare(&context_token, &window_token);
    context_hook.Invoke(&foreign_token);
    window_hook.Invoke(&foreign_token);
    EXPECT_EQ(owner.events, (std::vector<std::string>{"context", "window"}));
    EXPECT_TRUE(draw.is_context(&context_token, &window_token));

    context_hook.Invoke(&context_token);
    EXPECT_EQ(owner.events, (std::vector<std::string>{"context", "window", "reset", "context"}));
    draw.Prepare(&context_token, &window_token);
    window_hook.Invoke(&window_token);
    EXPECT_EQ(owner.events[4], "reset");
    EXPECT_EQ(owner.events[5], "window");
    draw.Prepare(&context_token, &window_token);
    EXPECT_EQ(mode_hook.Invoke(nullptr, nullptr, nullptr, "driver", "command"), 7);
    EXPECT_EQ(owner.events[6], "reset");
    EXPECT_EQ(owner.events[7], "mode");

    draw.Prepare(&context_token, &window_token);
    owner.detach_on_reset = true;
    context_hook.Invoke(&context_token);
    EXPECT_EQ(owner.events[8], "reset");
    EXPECT_EQ(owner.events[9], "context");
    EXPECT_TRUE(owner.subscriptions.empty());
    EXPECT_EQ(mode_chain.GetHandlersCount(), 0u);
    EXPECT_EQ(context_chain.GetHandlersCount(), 0u);
    EXPECT_EQ(window_chain.GetHandlersCount(), 0u);
    draw.Prepare(&context_token, &window_token);
    context_hook.Invoke(&foreign_token);
    EXPECT_TRUE(draw.is_context(&context_token, &window_token));
    EXPECT_EQ(owner.events.back(), "context");
    draw.ResetGl(false);
}
