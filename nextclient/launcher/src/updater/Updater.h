#pragma once
#include <utils/SynchronizationContext.h>
#include <gui_app_core/GuiAppInterface.h>
#include <updater/HttpServiceInterface.h>
#include <updater/UpdaterDoneStatus.h>
#include <updater/json_data/BranchEntry.h>
#include <updater/NextUpdater/NextUpdaterEvent.h>
#include "UpdaterViewInterface.h"
#include "NextUpdater/NextUpdater.h"

class Updater : public GuiAppInterface
{
    std::shared_ptr<HttpServiceInterface> http_service_;
    std::shared_ptr<UpdaterViewInterface> updater_view_;
    std::shared_ptr<SynchronizationContext> sync_ctx_;
    std::function<void(NextUpdaterEvent)> error_event_callback_;

    std::atomic_bool is_next_updater_done_ = false;
    NextUpdaterResult next_updater_result_;
    std::optional<UpdaterDoneStatus> done_status_;

    bool is_branch_list_request_done_ = false;
    std::vector<BranchEntry> branches_;

    std::thread updating_thread_;
    std::mutex next_updater_mutex_;

    NextUpdater next_updater_;

public:
    explicit Updater(std::shared_ptr<HttpServiceInterface> http_service, std::shared_ptr<UpdaterViewInterface> updater_view, el::Logger* logger, const std::function<void(NextUpdaterEvent)>& error_event_callback = nullptr);
    ~Updater() override;

    GuiAppStartUpInfo OnStart() override;
    bool OnUpdate() override;
    void OnExit() override;

    // valid only after Updater has finished its work
    UpdaterDoneStatus DoneStatus();
    std::vector<BranchEntry> Branches();

private:
    void OnViewExit();

    void RunNextUpdaterThreaded();
    void OnUpdaterEvent(const NextUpdaterEvent& event);
    static UpdaterDoneStatus GetDoneStatusByUpdaterResult(NextUpdaterResult next_updater_result);

    void BranchListResponse(const HttpResponse& response);
};
