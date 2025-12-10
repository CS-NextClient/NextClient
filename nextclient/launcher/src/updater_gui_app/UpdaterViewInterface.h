#pragma once

#include <functional>
#include <string>
#include <tuple>
#include "UpdaterViewState.h"

class UpdaterViewInterface
{
public:
    virtual ~UpdaterViewInterface() = default;

    virtual void OnStart() = 0;
    virtual void OnGui() = 0;
    virtual void SetState(UpdaterViewState state) = 0;
    virtual void SetError(std::string error) = 0;
    virtual void SetProgress(float progress) = 0;
    virtual void SetExitCallback(std::function<void()> callback) = 0;
    virtual std::tuple<int, int> GetWindowSize() = 0;
    virtual UpdaterViewState GetState() = 0;
};