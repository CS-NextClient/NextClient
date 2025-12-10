#pragma once
#include <chrono>
#include <regex>

#include <gui_app_core/GuiAppInterface.h>
#include <updater_gui_app/HttpServiceInterface.h>
#include <updater_gui_app/UpdaterDoneStatus.h>
#include <updater_gui_app/json_data/BranchEntry.h>
#include <updater_gui_app/NextUpdater/NextUpdaterEvent.h>
#include <next_engine_mini/AnalyticsInterface.h>
#include <next_launcher/IUserStorage.h>
#include <utils/stopwatch.hpp>
#include <taskcoro/TaskCoro.h>
#include <taskcoro/impl/SynchronizationContextManual.h>

#include "UpdaterViewInterface.h"
#include "updater_gui_app/UpdaterResult.h"
#include "updater_gui_app/UpdaterFlags.h"
#include "NextUpdater/NextUpdater.h"

class Updater : public GuiAppInterface<UpdaterResult>
{
    static constexpr std::chrono::seconds kWindowShowDelay{2};
    static constexpr std::chrono::seconds kSpecificStatesTimeout{10};
    static constexpr char kInstallFolder[] = "";
    static constexpr char kBackupFolder[] = "update\\backup_v3";

    std::shared_ptr<AnalyticsInterface> analytics_;
    std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver_;
    std::shared_ptr<next_launcher::IUserStorage> user_storage_;
    std::shared_ptr<next_launcher::UserInfoClient> user_info_;
    std::string language_;
    UpdaterFlags flags_;
    std::function<void(NextUpdaterEvent)> error_event_callback_;

    std::shared_ptr<taskcoro::CancellationToken> ct_;
    std::shared_ptr<taskcoro::SynchronizationContext> ui_thread_ctx_;
    std::shared_ptr<taskcoro::SynchronizationContextManual> ui_thread_manual_sync_ctx_;
    concurrencpp::result<UpdaterResult> working_thread_task_;
    std::shared_ptr<HttpServiceInterface> http_service_;
    std::shared_ptr<UpdaterViewInterface> view_;
    std::unique_ptr<NextUpdater> next_updater_;

    sw::stopwatch state_timeout_;
    sw::stopwatch show_window_delay_timeout_;

public:
    explicit Updater(std::shared_ptr<AnalyticsInterface> analytics,
                     std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver,
                     std::shared_ptr<next_launcher::IUserStorage> user_storage,
                     std::shared_ptr<next_launcher::UserInfoClient> user_info,
                     const std::string& language,
                     UpdaterFlags flags,
                     std::function<void(NextUpdaterEvent)> error_event_callback);
    ~Updater() override;

    GuiAppStartUpInfo OnStart() override;
    void OnUpdate(GuiAppState& state) override;
    UpdaterResult OnExit() override;

private:
    void ViewExitHandler();
    void CancelUpdate(UpdaterDoneStatus done_status);
    void CheckStateTimeout();
    bool IsWindowShouldBeVisible();

    concurrencpp::result<UpdaterResult> RunWorkingThread();
    concurrencpp::result<bool> ProcessNextUpdater(UpdaterResult& status_out);
    concurrencpp::result<void> ProcessBranchList(UpdaterResult& status_out);
    void OnUpdaterEvent(const NextUpdaterEvent& event);
    void CancelRequests();
    concurrencpp::result<std::vector<BranchEntry>> RequestBranchList();
    void SetViewStateThreadSafe(UpdaterViewState state, bool start_timeout);
    void ResetViewStateTimeoutThreadSafe();

    static UpdaterDoneStatus GetDoneStatusByUpdaterResult(NextUpdaterResult result);
    static UpdaterViewState GetViewStateByUpdaterState(NextUpdaterState next_updater_state);
};
