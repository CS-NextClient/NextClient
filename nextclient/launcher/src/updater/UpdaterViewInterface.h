#pragma once

#include <functional>
#include <string>
#include <tuple>
#include <updater/NextUpdater/NextUpdaterEvent.h>

class UpdaterViewInterface
{
public:
    virtual ~UpdaterViewInterface() = default;

    virtual void OnGui() = 0;
    virtual void SetState(NextUpdaterState state) = 0;
    virtual void SetError(std::string error) = 0;
    virtual void SetProgress(float progress) = 0;
    virtual void SetExitCallback(std::function<void()> callback) = 0;
    virtual std::tuple<int, int> GetWindowSize() = 0;
};