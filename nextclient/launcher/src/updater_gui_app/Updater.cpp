#include "Updater.h"
#include <easylogging++.h>
#include <data_encoding/base64.h>
#include <data_encoding/aes.h>
#include <nitro_utils/random_utils.h>
#include <ncl_utils/scope_exit.h>
#include <taskcoro/impl/SynchronizationContextManual.h>
#include <updater_gui_app/json_data/BranchEntry.h>

#include "UpdaterView.h"

using namespace taskcoro;
using namespace concurrencpp;

static const char* LOG_TAG = "[Updater GUI App] ";

Updater::Updater(
    std::shared_ptr<AnalyticsInterface> analytics,
    std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver,
    std::shared_ptr<next_launcher::IUserStorage> user_storage,
    std::shared_ptr<next_launcher::UserInfoClient> user_info,
    const std::string& language,
    UpdaterFlags flags,
    std::function<void(NextUpdaterEvent)> error_event_callback)
:
    analytics_(std::move(analytics)),
    backend_address_resolver_(std::move(backend_address_resolver)),
    user_storage_(std::move(user_storage)),
    user_info_(std::move(user_info)),
    language_(language),
    flags_(flags),
    error_event_callback_(std::move(error_event_callback))
{
}

Updater::~Updater()
{
}

GuiAppStartUpInfo Updater::OnStart()
{
    ct_ = CancellationToken::Create();

    http_service_ = std::make_shared<NextUpdaterHttpService>(user_info_, backend_address_resolver_);
    next_updater_ = std::make_unique<NextUpdater>(kInstallFolder, kBackupFolder, http_service_, [this](const NextUpdaterEvent& event) { OnUpdaterEvent(event); });

    view_ = std::make_shared<UpdaterView>(language_);
    view_->SetExitCallback([this] { ViewExitHandler(); });
    view_->OnStart();

    ui_thread_manual_sync_ctx_ = std::make_shared<SynchronizationContextManual>();
    ui_thread_ctx_ = std::make_shared<SynchronizationContext>(ui_thread_manual_sync_ctx_);

    working_thread_task_ = TaskCoro::RunTask(TaskType::NewThread, ContinuationContextType::Callee, &Updater::RunWorkingThread, this);

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
    ui_thread_manual_sync_ctx_->Update();
    view_->OnGui();

    if (state.windows_state_hidden && IsWindowShouldBeVisible())
    {
        LOG(INFO) << LOG_TAG << "Make the updater window visible";
        state.windows_state_hidden = false;
    }

    CheckStateTimeout();

    if (working_thread_task_.status() != result_status::idle)
    {
        state.should_exit = true;
    }
}

UpdaterResult Updater::OnExit()
{
    auto [is_cancelled, cancellation_reason] = ct_->GetCancelReasonAndCancelFlag();
    if (!is_cancelled)
    {
        ct_->SetCanceled();
    }

    // In general, this is a rather dangerous call.
    // But in this case, it is acceptable because http_service_ is guaranteed to run in a non-main thread,
    // and this call will not block it.
    http_service_->ShutdownAsync().get();

    UpdaterResult result{};

    try
    {
        result = working_thread_task_.get();
    }
    catch (const OperationCanceledException& )
    { }

    if (cancellation_reason)
    {
        result.done_status = (UpdaterDoneStatus)(cancellation_reason - 1);
    }


    return result;
}

void Updater::ViewExitHandler()
{
    LOG(INFO) << LOG_TAG << "Interrupt the updater: user cancelled";
    CancelUpdate(UpdaterDoneStatus::Exit);
}

void Updater::CancelUpdate(UpdaterDoneStatus done_status)
{
    next_updater_->Cancel();
    ct_->SetCanceledWithReason((int16_t)done_status + 1);
}

void Updater::CheckStateTimeout()
{
    if (state_timeout_.get_elapsed() > kSpecificStatesTimeout)
    {
        LOG(INFO) << LOG_TAG << "Interrupt the updater: state timeout. State: " << magic_enum::enum_name(view_->GetState());
        CancelUpdate(UpdaterDoneStatus::RunGame);
    }
}

bool Updater::IsWindowShouldBeVisible()
{
    bool is_should_be_visible =
        next_updater_->get_state() == NextUpdaterState::Downloading ||
        show_window_delay_timeout_.get_elapsed() > kWindowShowDelay;

    return is_should_be_visible;
}

result<UpdaterResult> Updater::RunWorkingThread()
{
    UpdaterResult result{};

    if (!co_await ProcessNextUpdater(result))
    {
        co_return result;
    }

    co_await ProcessBranchList(result);

    co_return result;
}

result<bool> Updater::ProcessNextUpdater(UpdaterResult& status_out)
{
    if ((flags_ & UpdaterFlags::Updater) == UpdaterFlags::None)
    {
        co_return true;
    }

    NextUpdaterResult next_updater_result = next_updater_->Start();

    status_out.done_status = GetDoneStatusByUpdaterResult(next_updater_result);

    if (next_updater_result == NextUpdaterResult::ConnectionError)
    {
        co_return false;
    }

    co_return true;
}

result<void> Updater::ProcessBranchList(UpdaterResult& status_out)
{
    SetViewStateThreadSafe(UpdaterViewState::RequestingBranchList, true);
    status_out.branches = co_await RequestBranchList();
    ResetViewStateTimeoutThreadSafe();
}

void Updater::OnUpdaterEvent(const NextUpdaterEvent& event)
{
    if (event.flags & NextUpdaterEventFlags::StateChanged)
    {
        bool is_transient_state =
            event.state == NextUpdaterState::RequestingFileList ||
            event.state == NextUpdaterState::GatheringFilesToUpdate ||
            event.state == NextUpdaterState::OpeningFilesToInstall;

        SetViewStateThreadSafe(GetViewStateByUpdaterState(event.state), !is_transient_state);
    }

    if (event.flags & NextUpdaterEventFlags::ProgressChanged)
    {
        ui_thread_ctx_->Run([this, event] { view_->SetProgress(event.state_progress); });
    }

    if (event.flags & NextUpdaterEventFlags::ErrorChanged)
    {
        ui_thread_ctx_->Run([this, event]
        {
            view_->SetError(event.error_description);

            if (error_event_callback_ != nullptr)
            {
                error_event_callback_(event);
            }
        });
    }
}

result<std::vector<BranchEntry>> Updater::RequestBranchList()
{
    LOG(INFO) << LOG_TAG << "Requesting branch list...";

    std::vector<BranchEntry> branches;

    HttpResponse response = co_await http_service_->PostAsync("branch_list", "null", ct_);
    if (response.error)
    {
        LOG(INFO) << LOG_TAG << "Branch list response error: " << response.error.message;
        co_return branches;
    }

    LOG(INFO) << LOG_TAG << "Branch list received";

    UpdaterJsonValue v;
    try
    {
        v = tao::json::basic_from_string<UpdaterJsonTraits>(response.data);
        branches = v.as<std::vector<BranchEntry>>();

        LOG(INFO) << LOG_TAG << "Branch list deserialized";
    }
    catch (...)
    {
        LOG(INFO) << LOG_TAG << "Branch list deserialization error";
    }

    co_return branches;
}

void Updater::SetViewStateThreadSafe(UpdaterViewState state, bool start_timeout)
{
    ui_thread_ctx_->Run([this, state, start_timeout]
    {
        view_->SetState(state);

        if (start_timeout)
        {
            state_timeout_.start();
        }
        else
        {
            state_timeout_.reset();
        }
    });
}

void Updater::ResetViewStateTimeoutThreadSafe()
{
    ui_thread_ctx_->Run([this]
    {
        state_timeout_.reset();
    });
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
