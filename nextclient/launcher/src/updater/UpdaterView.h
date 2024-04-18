#pragma once

#include <string>
#include <unordered_map>
#include "UpdaterViewInterface.h"

class UpdaterView : public UpdaterViewInterface
{
    const int kWindowWidth = 500;
    const int kWindowHeight = 140;

    std::string language_;
    std::string error_message_;

    NextUpdaterState state_ = NextUpdaterState::Initialization;
    float progress_ = 0;

    std::function<void()> exit_callback_;

    static const std::unordered_map<NextUpdaterState, std::string> state_string_en_;
    static const std::unordered_map<NextUpdaterState, std::string> state_string_ru_;
    static const std::unordered_map<NextUpdaterState, float> state_progress_;

public:
    explicit UpdaterView(std::string language);

    void OnGui() override;
    void SetState(NextUpdaterState state) override;
    void SetError(std::string error) override;
    void SetProgress(float progress) override;
    void SetExitCallback(std::function<void()> callback) override;
    std::tuple<int, int> GetWindowSize() override;

private:
    float GetOverallProgress();
};

