#include "Updater.h"
#include <easylogging++.h>
#include <updater/json_data/BranchEntry.h>

#define LOG_TAG "[Updater GUI App] "

Updater::Updater(std::shared_ptr<HttpServiceInterface> http_service,
                 std::shared_ptr<UpdaterViewInterface> updater_view,
                 el::Logger* logger,
                 const std::function<void(NextUpdaterEvent)>& error_event_callback,
                 UpdaterFlags flags)
    : http_service_(std::move(http_service)),
      view_(std::move(updater_view)),
      logger_(logger),
      error_event_callback_(std::move(error_event_callback)),
      sync_ctx_(std::make_shared<SynchronizationContext>()),
      next_updater_(kInstallFolder, kBackupFolder, logger_, http_service_, [this](const NextUpdaterEvent& event) { OnUpdaterEvent(event); }),
      flags_(flags)
{
    view_->SetExitCallback([this] { ViewExitHandler(); });
}

Updater::~Updater()
{
    WaitForUpdaterThread();
}

GuiAppStartUpInfo Updater::OnStart()
{
    view_->OnStart();
    working_thread_ = std::thread(&Updater::RunWorkingThread, this);

    show_window_delay_timeout_.start();

    auto [window_width, window_height] = view_->GetWindowSize();

    GuiAppStartUpInfo start_up_info
    {
        window_width,
        window_height,
        std::string("Counter-Strike"),
        true,
    };

    return start_up_info;
}

void Updater::OnUpdate(GuiAppState& state)
{
    sync_ctx_->RunCallbacks();
    view_->OnGui();

    if (state.windows_state_hidden &&
        (next_updater_.get_state() == NextUpdaterState::Downloading || show_window_delay_timeout_.get_elapsed() > kWindowShowDelay))
    {
        logger_->info(LOG_TAG "Make the updater window visible");

        state.windows_state_hidden = false;
    }

    if (!is_canceled_ && state_timeout_.get_elapsed() > kSpecificStatesTimeout)
    {
        logger_->info(LOG_TAG "Interrupt the updater: state specific timeout");
        CancelUpdate(UpdaterDoneStatus::RunGame);
    }

    if (is_updater_thread_done_)
    {
        state.should_exit = true;
    }
}

void Updater::OnExit()
{
    if (!done_status_)
    {
        done_status_ = UpdaterDoneStatus::RunGame;
    }
}

UpdaterResult Updater::GetResult()
{
    return { *done_status_ };
}

std::vector<BranchEntry> Updater::Branches()
{
    return branches_;
}

void Updater::ViewExitHandler()
{
    logger_->info(LOG_TAG "Interrupt the updater: user cancelled");
    CancelUpdate(UpdaterDoneStatus::Exit);
}

void Updater::CancelUpdate(UpdaterDoneStatus done_status)
{
    is_canceled_ = true;
    done_status_ = done_status;
    next_updater_.Cancel();
}

void Updater::RunWorkingThread()
{
    UpdaterDoneStatus done_status{};

    if ((flags_ & UpdaterFlags::Updater) != UpdaterFlags::None)
    {
        NextUpdaterResult next_updater_result = next_updater_.Start();
        done_status = GetDoneStatusByUpdaterResult(next_updater_result);
    }

    if (!is_canceled_)
    {
        sync_ctx_->Execute([this]
        {
            state_timeout_.start();

            view_->SetState(UpdaterViewState::RequestingBranchList);
        });

        RequestBranchList();
    }

    sync_ctx_->Execute([this, done_status]
    {
        is_updater_thread_done_ = true;

        if (!done_status_)
        {
            done_status_ = done_status;
        }
    });
}

void Updater::OnUpdaterEvent(const NextUpdaterEvent& event)
{
    if (event.flags & NextUpdaterEventFlags::StateChanged)
    {
        sync_ctx_->Execute([this, event]
        {
            if (event.state == NextUpdaterState::RequestingFileList ||
                event.state == NextUpdaterState::GatheringFilesToUpdate ||
                event.state == NextUpdaterState::OpeningFilesToInstall)
            {
                state_timeout_.start();
            }
            else
            {
                state_timeout_.reset();
            }

            view_->SetState(GetViewStateByUpdaterState(event.state));
        });
    }

    if (event.flags & NextUpdaterEventFlags::ProgressChanged)
    {
        sync_ctx_->Execute([this, event]
        {
            view_->SetProgress(event.state_progress);
        });
    }

    if (event.flags & NextUpdaterEventFlags::ErrorChanged)
    {
        sync_ctx_->Execute([this, event]
        {
            view_->SetError(event.error_description);

            if (error_event_callback_ != nullptr)
            {
                error_event_callback_(event);
            }
        });
    }
}

void Updater::WaitForUpdaterThread()
{
    if (working_thread_.joinable())
    {
        working_thread_.join();
    }
}

void Updater::RequestBranchList()
{
    logger_->info(LOG_TAG "Requesting branch list...");

    HttpResponse response = http_service_->Post("branch_list", "null", [this](cpr::cpr_off_t total, cpr::cpr_off_t downloaded)
    {
        return !is_canceled_.load();
    });

    if (is_canceled_ || response.error || response.data.empty())
    {
        if (is_canceled_)
        {
            logger_->info(LOG_TAG "Branch list request canceled");
        }
        else if (response.error)
        {
            logger_->info(LOG_TAG "Branch list response error: %v", response.error.message);
        }

        return;
    }

    logger_->info(LOG_TAG "Branch list received");

    UpdaterJsonValue v;
    try
    {
        v = tao::json::basic_from_string<UpdaterJsonTraits>(response.data);
        branches_ = v.as<std::vector<BranchEntry>>();

        logger_->info(LOG_TAG "Branch list deserialized");
    }
    catch (...)
    {
        logger_->info(LOG_TAG "Branch list deserialization error");
    }
}

UpdaterDoneStatus Updater::GetDoneStatusByUpdaterResult(NextUpdaterResult next_updater_result)
{
    switch (next_updater_result)
    {
    case NextUpdaterResult::UpdatedIncludingLauncher:
        return UpdaterDoneStatus::RunNewGame;

    default:
        return UpdaterDoneStatus::RunGame;
    }
}

UpdaterViewState Updater::GetViewStateByUpdaterState(NextUpdaterState next_updater_state)
{
    static const std::unordered_map<NextUpdaterState, UpdaterViewState> state_map = {
        {NextUpdaterState::Idle, UpdaterViewState::Initialization},
        {NextUpdaterState::RestoringFromBackup, UpdaterViewState::RestoringFromBackup},
        {NextUpdaterState::ClearingBackupFolder, UpdaterViewState::ClearingBackupFolder},
        {NextUpdaterState::RequestingFileList, UpdaterViewState::RequestingFileList},
        {NextUpdaterState::GatheringFilesToUpdate, UpdaterViewState::GatheringFilesToUpdate},
        {NextUpdaterState::Backuping, UpdaterViewState::Backuping},
        {NextUpdaterState::OpeningFilesToInstall, UpdaterViewState::OpeningFilesToInstall},
        {NextUpdaterState::Downloading, UpdaterViewState::Downloading},
        {NextUpdaterState::Installing, UpdaterViewState::Installing},
        {NextUpdaterState::CanceledByUser, UpdaterViewState::CanceledByUser},
        {NextUpdaterState::Done, UpdaterViewState::Done}
    };

    auto it = state_map.find(next_updater_state);
    return it != state_map.end() ? it->second : UpdaterViewState::Done;
}
