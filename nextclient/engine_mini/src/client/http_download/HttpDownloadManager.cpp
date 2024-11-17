#include "HttpDownloadManager.h"
#include "../../engine.h"
#include "../../console/console.h"
#include <nitro_utils/string_utils.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <utility>

using namespace std::chrono;
using namespace nitro_utils;

HttpDownloadManager::HttpDownloadManager(IGameUI* game_ui,
                                         vgui2::ILocalize* localize,
                                         std::shared_ptr<DownloadFileLoggerInterface> download_logger,
                                         std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider) :
    game_ui_(game_ui),
    localize_(localize),
    download_logger_(download_logger),
    config_provider_(config_provider)
{
    cvar_max_active_requests = gEngfuncs.pfnRegisterVariable("http_max_active_requests", "5", FCVAR_ARCHIVE);
    cvar_max_requests_retries = gEngfuncs.pfnRegisterVariable("http_max_requests_retries", "3", FCVAR_ARCHIVE);

    requests_.reserve(MAX_POSSIBLE_ACTIVE_REQUESTS);
}

int HttpDownloadManager::GetMaxActiveRequests() {
    return clamp(cvar_max_active_requests->value, 1, MAX_POSSIBLE_ACTIVE_REQUESTS);
}

int HttpDownloadManager::GetMaxRequestsRetries() {
    return clamp(cvar_max_requests_retries->value, 0, 10);
}

void HttpDownloadManager::SetUrl(const std::string &url)
{
    if (!ValidateUrl(url))
    {
        base_url_ = "";
        Con_Printf("[HTTP] Tried to set invalid url \"%s\"\n", url.c_str());
        return;
    }

    base_url_ = FixUrl(url);
}

void HttpDownloadManager::Queue(const ResourceDescriptor& file_resource)
{
    if (!IsSafeFileToDownload(file_resource.get_download_path()))
    {
        Con_Printf("[HTTP] Invalid file path to download: %s\n", file_resource.get_download_path().c_str());
        download_logger_->AddLogFileError(file_resource.get_download_path().c_str(), LogFileTypeError::FileBlocked, 0, 0);
        return;
    }

    if (!IsSafeFileToDownload(file_resource.get_save_path()))
    {
        Con_Printf("[HTTP] Invalid file path to save: %s\n", file_resource.get_save_path().c_str());
        download_logger_->AddLogFileError(file_resource.get_download_path().c_str(), LogFileTypeError::FileBlocked, 0, 0);
        return;
    }

    total_bytes_to_download_ += file_resource.get_download_size();
    total_files_to_download_++;

    files_to_download_.emplace(file_resource, 0);
}

void HttpDownloadManager::Stop()
{
    std::queue<QueuedRequest>().swap(files_to_download_);

    for (const auto &request : requests_)
    {
        request.get_shared_data()->stop_download = true;
    }
    requests_.clear();

    if (is_download_active_)
        InvokeEndDownloadingEvent();

    is_download_active_ = false;
    download_statistics_ = TransferStatistics<uint32_t>();
    total_files_to_download_ = 0;
    total_bytes_to_download_ = 0;
    completed_requests_bytes_downloaded_ = 0;
    is_slow_speed_ = false;
}

uint32_t HttpDownloadManager::GetDownloadQueueSize()
{
    return files_to_download_.size() + requests_.size();
}

void HttpDownloadManager::Update()
{
    PruneCompletedRequests();
    StartNewDownloads();
    UpdateDownloadSpeed();
    UpdateUi();
    CheckAllDownloadsCompleted();
}

bool HttpDownloadManager::AddListener(HttpDownloadManagerEventsListenerInterface* listener)
{
    if (std::find(listeners_.cbegin(), listeners_.cend(), listener) != listeners_.cend())
        return false;

    listeners_.emplace_back(listener);
    return true;
}

bool HttpDownloadManager::RemoveListener(HttpDownloadManagerEventsListenerInterface* listener)
{
    auto it = std::find(listeners_.cbegin(), listeners_.cend(), listener);
    if (it == std::end(listeners_))
        return false;

    listeners_.erase(it);
    return true;
}

void HttpDownloadManager::InvokeStartDownloadingEvent()
{
    for (auto& listener : listeners_)
        listener->OnStartDownloading();
}

void HttpDownloadManager::InvokeEndDownloadingEvent()
{
    for (auto& listener : listeners_)
        listener->OnEndDownloading();
}

void HttpDownloadManager::PruneCompletedRequests()
{
    for (auto it = requests_.begin(); it != requests_.end(); )
    {
        auto& request = *it;

        if (!request.get_response().valid() || request.get_response().wait_for(0s) != std::future_status::ready)
        {
            ++it;
            continue;
        }

        auto response_result = request.get_response().get();
        auto error_message = ValidateResponseAndGetErrorMessage(response_result);
        const ResourceDescriptor& resource_descriptor = request.get_file_resource();

        if (!error_message)
        {
            bool is_saved = resource_descriptor.SaveToFile(response_result.text.data(), response_result.text.length());
            if (is_saved)
            {
                download_logger_->AddLogFile(resource_descriptor.get_download_path().c_str(), response_result.downloaded_bytes, LogFileType::FileDownloaded);
            }
            else
            {
                Con_Printf("[HTTP] Can't save file: %s\n", resource_descriptor.get_save_path().c_str());
                download_logger_->AddLogFileError(resource_descriptor.get_download_path().c_str(), LogFileTypeError::FileSaveError, 0, 0);
            }

            completed_requests_bytes_downloaded_ += response_result.downloaded_bytes;

        }
        else
        {
            if (request.get_retry() <= GetMaxRequestsRetries())
            {
                files_to_download_.emplace(resource_descriptor, request.get_retry() + 1);
            }
            else
            {
                total_bytes_to_download_ -= resource_descriptor.get_download_size();

                Con_Printf("[HTTP] Can't download: %s | %s\n", resource_descriptor.get_download_path().c_str(), error_message->c_str());

                auto log_file_type = response_result.status_code == 404 ? LogFileTypeError::FileMissingHTTP : LogFileTypeError::FileErrorHTTP;
                download_logger_->AddLogFileError(resource_descriptor.get_download_path().c_str(), log_file_type, (int)response_result.error.code, response_result.status_code);
            }
        }

        it = requests_.erase(it);
    }
}

void HttpDownloadManager::StartNewDownloads()
{
    while (!files_to_download_.empty())
    {
        if (requests_.size() >= GetMaxActiveRequests())
            break;

        auto& queued_request = files_to_download_.front();

        std::string file_url = base_url_ + queued_request.get_file_resource().get_download_path();
        nitro_utils::replace_all(file_url, " ", "%20");

        auto shared_data = std::make_shared<RequestContext::Shared>();
        auto cpr_response = cpr::GetAsync(
            cpr::Url(file_url),
            cpr::UserAgent("Valve/Steam HTTP Client 1.0 (10)"),
            cpr::ConnectTimeout(std::chrono::duration_cast<std::chrono::milliseconds>(connection_timeout_)),
            cpr::ProgressCallback(
                [shared_data]
                    (size_t downloadTotal, size_t downloadNow, size_t uploadTotal, size_t uploadNow, intptr_t userdata)
                {
                    if (shared_data->stop_download)
                        return false;

                    shared_data->download_total = downloadTotal;
                    shared_data->download_now = downloadNow;

                    return true;
                }));
        requests_.emplace_back(
            queued_request.get_file_resource(),
            queued_request.get_retry(),
            std::chrono::system_clock::now(),
            shared_data,
            std::move(cpr_response));

        files_to_download_.pop();

        if (!is_download_active_)
        {
            is_download_active_ = true;
            Con_Printf("[HTTP] Start downloading from: %s\n", base_url_.c_str());
            InvokeStartDownloadingEvent();
        }
    }
}

void HttpDownloadManager::UpdateDownloadSpeed()
{
    auto current_time = system_clock::now();
    if (is_download_active_ &&
        ((bytes_downloaded_prev_time_ + 1000ms <= current_time) || download_statistics_.get_speed() == 0))
    {
        download_statistics_.UpdateTransferredBytes(GetDownloadedBytes(), current_time);
        SlowSpeedDetection();

        bytes_downloaded_prev_time_ = current_time;
    }
}

void HttpDownloadManager::SlowSpeedDetection()
{
    auto current_time = system_clock::now();

    if (download_statistics_.get_speed() >= slow_speed_threshold_)
    {
        is_slow_speed_ = false;
        return;
    }

    if (!is_slow_speed_)
    {
        is_slow_speed_ = true;
        slow_speed_start_time_ = current_time;
    }

    if (slow_speed_start_time_ + slow_speed_fallback_timeout_ <= current_time)
    {
        download_logger_->AddLogDownloadAborted(DownloadAbortedReason::HttpSlowSpeed);
        Stop();
        gEngfuncs.pfnClientCmd("retry");
    }
}

void HttpDownloadManager::UpdateUi()
{
    size_t download_queue_size = GetDownloadQueueSize();
    if (is_download_active_ && download_queue_size > 0)
    {
        std::string status_str;
        if (is_slow_speed_)
        {
            auto target_time = slow_speed_start_time_ + slow_speed_fallback_timeout_;
            auto cur_time = system_clock::now();
            auto diff = duration_cast<seconds>(target_time - cur_time);

            if (diff <= slow_speed_fallback_timeout_ui_)
            {
                char slow_speed[64];
                localize_->ConvertUnicodeToANSI(localize_->Find("#NextClient_Downloading_SlowSpeed"), slow_speed, sizeof(slow_speed));
                status_str = std::format("{} ({})", slow_speed, diff.count());
            }
        }

        if (status_str.empty())
        {
            char downloading_files[64];
            localize_->ConvertUnicodeToANSI(localize_->Find("#NextClient_Downloading_Files"), downloading_files, sizeof(downloading_files));

            uint32_t actual_bytes_downloaded = GetDownloadedBytes();

            status_str.append(downloading_files);
            status_str.append(" ");
            status_str.append(std::to_string(download_queue_size));
            status_str.append(std::string(std::max(0, 23 - (int)std::strlen(downloading_files)), ' '));
            status_str.append(std::format("{:.1f}/{:.1f} MB, {}/s",
                                          (float)actual_bytes_downloaded / (1024.0F * 1024.0F),
                                          (float)std::max(total_bytes_to_download_, actual_bytes_downloaded) / (1024.0F * 1024.0F),
                                          FormatFileSize(download_statistics_.get_speed(), 2)));
        }

        game_ui_->ContinueProgressBar(total_files_to_download_ - download_queue_size, 0.0);
        game_ui_->SetProgressBarStatusText(status_str.c_str());

        if (!requests_.empty())
        {
            const auto &request = requests_[0];

            float download_progress = (float)request.get_shared_data()->download_now / request.get_shared_data()->download_total;
            const char* file_path = request.get_file_resource().get_download_path().c_str();

            game_ui_->SetSecondaryProgressBar(download_progress);
            game_ui_->SetSecondaryProgressBarText(file_path);
        }
    }
}

void HttpDownloadManager::CheckAllDownloadsCompleted()
{
    if (is_download_active_ && GetDownloadQueueSize() == 0)
    {
        Stop();
        gEngfuncs.pfnClientCmd("retry");
    }
}

uint32_t HttpDownloadManager::GetDownloadedBytes()
{
    uint32_t active_downloads_bytes = std::accumulate(
        requests_.cbegin(),
        requests_.cend(),
        0,
        [](uint32_t sum, const auto &request)
        {
            return sum + request.get_shared_data()->download_now;
        });

    uint32_t actual_bytes_downloaded = completed_requests_bytes_downloaded_ + active_downloads_bytes;

    return actual_bytes_downloaded;
}

std::string HttpDownloadManager::FormatFileSize(size_t size, short precision)
{
    std::ostringstream stringSize;
    stringSize.precision(precision);

    if (size == 0) stringSize << "0 KB";
    else if (size < 1024) stringSize << "~1 KB";
    else if (size < 1048576) stringSize << size / 1024 << " KB";
    else stringSize << std::fixed << size / 1048576.0 << " MB";

    return stringSize.str();
}

bool HttpDownloadManager::ValidateUrl(const std::string &url)
{
    if (!url.starts_with("https://") && !url.starts_with("http://"))
    {
        return false;
    }

    return true;
}

std::string HttpDownloadManager::FixUrl(const std::string &url)
{
    std::string fixed_url(url);

    nitro_utils::replace_nth(fixed_url, ":/", 1, "/"); // fix for uris with empty port
    if (fixed_url.length() > 0 && fixed_url[fixed_url.length() - 1] != '/')
    {
        fixed_url = fixed_url + '/';
    }

    return fixed_url;
}

std::optional<std::string> HttpDownloadManager::ValidateResponseAndGetErrorMessage(const cpr::Response& response)
{
    if (response.error.code != cpr::ErrorCode::OK)
        return response.error.message;

    if (response.status_code != 200)
        return "HTTP Code: " + std::to_string(response.status_code);

    if (response.text.empty())
        return "Empty file";

    if (!ValidateDownloadedData(response.text))
        return "Invalid downloaded data";

    return {};
}

bool HttpDownloadManager::ValidateDownloadedData(const std::string &data)
{
    if (data.size() > 2048)
        return true;

    auto bad_content = config_provider_->get_list("invalid_file_content");
    if (!bad_content)
        return true;

    for (const auto &bad_str : *bad_content)
    {
        if (data.find(bad_str) != std::string::npos)
            return false;
    }

    return true;
}
