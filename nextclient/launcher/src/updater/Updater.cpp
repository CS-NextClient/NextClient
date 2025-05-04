#include "Updater.h"
#include <easylogging++.h>
#include <magic_enum/magic_enum.hpp>
#include <updater/json_data/BranchEntry.h>

#define INSTALL_FOLDER ""
#define BACKUP_FOLDER "update\\backup_v3"

Updater::Updater(std::shared_ptr<HttpServiceInterface> http_service, std::shared_ptr<UpdaterViewInterface> updater_view, el::Logger* logger, const std::function<void(NextUpdaterEvent)>& error_event_callback) :
    http_service_(std::move(http_service)),
    updater_view_(std::move(updater_view)),
    error_event_callback_(std::move(error_event_callback)),
    sync_ctx_(std::make_shared<SynchronizationContext>()),
    next_updater_(INSTALL_FOLDER, BACKUP_FOLDER, logger, http_service_, [this](const NextUpdaterEvent& event) { OnUpdaterEvent(event); })
{
    http_service_->PostAsync("branch_list", "null", [this](const HttpResponse& response) { BranchListResponse(response); });
    updater_view_->SetExitCallback([this]() { OnViewExit(); });
}

Updater::~Updater()
{
    if (updating_thread_.joinable())
        updating_thread_.join();
}

GuiAppStartUpInfo Updater::OnStart()
{
    updating_thread_ = std::thread(&Updater::RunNextUpdaterThreaded, this);

    auto [window_width, window_height] = updater_view_->GetWindowSize();
    return GuiAppStartUpInfo(window_width, window_height, "Counter-Strike");
}

bool Updater::OnUpdate()
{
    http_service_->Update();
    sync_ctx_->RunCallbacks();
    updater_view_->OnGui();

    if (is_next_updater_done_ && !done_status_.has_value())
        done_status_ = GetDoneStatusByUpdaterResult(next_updater_result_);

    return !done_status_.has_value() && !is_branch_list_request_done_;
}

void Updater::OnExit()
{
    if (!done_status_.has_value())
        done_status_ = UpdaterDoneStatus::RunGame;
}

UpdaterDoneStatus Updater::DoneStatus()
{
    return *done_status_;
}

std::vector<BranchEntry> Updater::Branches()
{
    return branches_;
}

void Updater::OnViewExit()
{
    done_status_ = UpdaterDoneStatus::Exit;
    next_updater_.Cancel();
}

void Updater::RunNextUpdaterThreaded()
{
    next_updater_result_ = next_updater_.Start();
    // Ensure that the next_updater_.Start() operation has completely finished before setting is_next_updater_done_ to true.
    is_next_updater_done_.store(true, std::memory_order_release);
}

void Updater::OnUpdaterEvent(const NextUpdaterEvent& event)
{
    if (event.flags & NextUpdaterEventFlags::StateChanged)
        sync_ctx_->Execute([this, event]() { updater_view_->SetState(event.state); });

    if (event.flags & NextUpdaterEventFlags::ProgressChanged)
        sync_ctx_->Execute([this, event]() { updater_view_->SetProgress(event.state_progress); });

    if (event.flags & NextUpdaterEventFlags::ErrorChanged)
    {
        sync_ctx_->Execute([this, event]() {
            updater_view_->SetError(event.error_description);

            if (error_event_callback_ != nullptr)
                error_event_callback_(event);
        });
    }
}

UpdaterDoneStatus Updater::GetDoneStatusByUpdaterResult(NextUpdaterResult next_updater_result)
{
    return next_updater_result == NextUpdaterResult::UpdatedIncludingLauncher ? UpdaterDoneStatus::RunNewGame : UpdaterDoneStatus::RunGame;
}

void Updater::BranchListResponse(const HttpResponse& response)
{
    is_branch_list_request_done_ = true;

    if (response.error)
        return;

    UpdaterJsonValue v;
    try
    {
        v = tao::json::basic_from_string<UpdaterJsonTraits>(response.data);
        branches_ = v.as<std::vector<BranchEntry>>();
    }
    catch (...)
    { }

}
