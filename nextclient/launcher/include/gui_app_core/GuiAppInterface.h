#pragma once

#include <string>

struct GuiAppStartUpInfo
{
    int window_width{};
    int window_height{};
    std::string window_name;
};

class GuiAppInterface
{
public:
    virtual ~GuiAppInterface() = default;

    virtual GuiAppStartUpInfo OnStart() = 0;
    // Return true while your application is running, return false when you want to exit.
    virtual bool OnUpdate() = 0;
    virtual void OnExit() = 0;
};
