#pragma once
#include <functional>
#include <memory>
#include "GuiAppInterface.h"

void RunGuiAppImpl(
    const std::function<GuiAppStartUpInfo()>& on_start,
    const std::function<void(GuiAppState&)>& on_update,
    const std::function<void()>& on_exit
);

template <class TResult>
TResult RunGuiApp(std::unique_ptr<GuiAppInterface<TResult>> gui_app)
{
    TResult result{};

    RunGuiAppImpl(
        [&] { return gui_app->OnStart(); },
        [&](GuiAppState& state) { gui_app->OnUpdate(state); },
        [&]() mutable
        {
            result = gui_app->OnExit();
            gui_app = nullptr;
        }
    );

    return result;
}