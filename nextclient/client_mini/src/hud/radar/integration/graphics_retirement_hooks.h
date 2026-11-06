#pragma once

#include <memory>
#include <vector>

#include <nitroapi/NitroApiInterface.h>

#include "hud/radar/render/RadarDraw.h"

namespace radar::integration
{
    template <class Function>
    bool HasNativeFunction(const Function& function)
    {
        return function.GetHook() && function.GetHook()->GetOriginalFunctionAddress();
    }

    template <class Owner>
    void SubscribeGraphicsRetirement(
        Owner& owner,
        RadarDrawContext& draw,
        nitroapi::EngineData& engine,
        nitroapi::SDL2Data* sdl,
        std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& subscriptions
    )
    {
        if (HasNativeFunction(engine.GL_SetMode))
        {
            subscriptions.push_back(
                engine.GL_SetMode |=
                [&owner](void* window, HDC* dc, HGLRC* context, const char* driver, const char* command_line, const auto& next) {
                    Owner* stable_owner = &owner;
                    stable_owner->ResetGraphics();
                    return next->Invoke(window, dc, context, driver, command_line);
                }
            );
        }
        if (sdl && HasNativeFunction(sdl->SDL_GL_DeleteContext))
        {
            subscriptions.push_back(sdl->SDL_GL_DeleteContext |= [&owner, &draw](void* context, const auto& next) {
                Owner* stable_owner = &owner;
                bool owned = draw.owns_context(context);
                if (owned)
                {
                    stable_owner->ResetGraphics();
                }
                next->Invoke(context);
            });
        }
        if (sdl && HasNativeFunction(sdl->SDL_DestroyWindowFunc))
        {
            subscriptions.push_back(sdl->SDL_DestroyWindowFunc |= [&owner, &draw](void* window, const auto& next) {
                Owner* stable_owner = &owner;
                bool owned = draw.owns_window(window);
                if (owned)
                {
                    stable_owner->ResetGraphics();
                }
                next->Invoke(window);
            });
        }
    }
} // namespace radar::integration
