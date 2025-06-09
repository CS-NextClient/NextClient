#pragma once
#include <chrono>

#include <utils/SynchronizationContext.h>
#include <gui_app_core/GuiAppInterface.h>
#include <updater/HttpServiceInterface.h>
#include <updater/UpdaterDoneStatus.h>
#include <updater/json_data/BranchEntry.h>
#include <updater/NextUpdater/NextUpdaterEvent.h>
#include <utils/stopwatch.hpp>
#include "UpdaterViewInterface.h"
#include "updater/UpdaterResult.h"
#include "updater/UpdaterFlags.h"
#include "NextUpdater/NextUpdater.h"

class Updater : public GuiAppInterface
{
    enum class RequestRemoteConfigErrorType
    {
        ConnectionError,
        NetworkError,
        DeserializationError,
    };

    static constexpr std::chrono::seconds kWindowShowDelay{2};
    static constexpr std::chrono::seconds kSpecificStatesTimeout{10};
    static constexpr char kInstallFolder[] = "";
    static constexpr char kBackupFolder[] = "update\\backup_v3";

    std::shared_ptr<HttpServiceInterface> http_service_;
    std::shared_ptr<UpdaterViewInterface> view_;
    std::shared_ptr<SynchronizationContext> sync_ctx_;
    std::function<void(NextUpdaterEvent)> error_event_callback_;
    el::Logger* logger_;
    NextUpdater next_updater_;
    UpdaterFlags flags_;
    std::vector<BranchEntry> branches_{};

    std::optional<UpdaterDoneStatus> done_status_{};
    bool is_updater_thread_done_{};
    std::atomic_bool is_canceled_{};

    std::thread working_thread_;
    std::mutex next_updater_mutex_;

    sw::stopwatch state_timeout_;
    sw::stopwatch show_window_delay_timeout_;

public:
    explicit Updater(std::shared_ptr<HttpServiceInterface> http_service,
                     std::shared_ptr<UpdaterViewInterface> updater_view,
                     el::Logger* logger,
                     const std::function<void(NextUpdaterEvent)>& error_event_callback,
                     UpdaterFlags flags);
    ~Updater() override;

    GuiAppStartUpInfo OnStart() override;
    void OnUpdate(GuiAppState& state) override;
    void OnExit() override;

    // valid only after Updater has finished its work
    UpdaterResult GetResult();
    std::vector<BranchEntry> Branches();

private:
    void ViewExitHandler();
    void CancelUpdate(UpdaterDoneStatus done_status);

    void RunWorkingThread();
    void OnUpdaterEvent(const NextUpdaterEvent& event);
    void WaitForUpdaterThread();
    void RequestBranchList();

    static UpdaterDoneStatus GetDoneStatusByUpdaterResult(NextUpdaterResult result);
    static UpdaterViewState GetViewStateByUpdaterState(NextUpdaterState next_updater_state);
};
