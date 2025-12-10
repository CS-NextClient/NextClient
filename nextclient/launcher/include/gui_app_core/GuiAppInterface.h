#pragma once
#include <string>

struct GuiAppStartUpInfo
{
    int window_width{};
    int window_height{};
    std::string window_name{};
    bool window_state_hidden{};
};

struct GuiAppState
{
    bool should_exit{};
    bool windows_state_hidden{};
};

template <class TResult>
class GuiAppInterface
{
public:
    virtual ~GuiAppInterface() = default;

    virtual GuiAppStartUpInfo OnStart() = 0;
    virtual void OnUpdate(GuiAppState& state) = 0;
    virtual TResult OnExit() = 0;
};
