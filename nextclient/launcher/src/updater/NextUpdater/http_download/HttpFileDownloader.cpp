#include "HttpFileDownloader.h"
#include <chrono>
#include <nitro_utils/string_utils.h>

using namespace std::chrono;
using namespace std::chrono_literals;

HttpFileDownloader::HttpFileDownloader(const std::vector<HttpFileRequest>& files, const std::string& base_url, const std::function<bool(cpr::cpr_off_t downloaded, cpr::cpr_off_t speed)>& progress) :
    progress_(progress)
{
    base_url_ = FixUrl(base_url);

    for (const auto& file : files)
        files_to_download_.emplace(file, 0);
}

std::vector<HttpFileResult> HttpFileDownloader::StartDownloads()
{
    bool terminate = false;

    while (true)
    {
        PruneCompletedRequests();
        StartNewDownloads();
        UpdateTransferStatistics();

        if (!terminate)
        {
            terminate = progress_(transfer_statistics_.get_downloaded_bytes(), transfer_statistics_.get_speed());
            if (terminate)
                CancelDownloads();
        }

        if (CheckAllDownloadsCompleted())
            return results_;

        std::this_thread::sleep_for(10ms);
    }
}

void HttpFileDownloader::PruneCompletedRequests()
{
    for (auto it = requests_.begin(); it != requests_.end(); )
    {
        auto& request_ctx = *it;

        if (request_ctx.get_response().wait_for(0s) != std::future_status::ready)
        {
            ++it;
            continue;
        }

        const HttpFileRequest& request = request_ctx.get_request();

        cpr::Response response = request_ctx.get_response().get();
        std::string error_message = ValidateResponseAndGetErrorMessage(response);

        if (error_message.empty())
        {
            std::vector<uint8_t> data(response.text.data(), response.text.data() + response.text.size());
            results_.emplace_back(request, std::move(data));

            completed_requests_bytes_downloaded_ += response.downloaded_bytes;
        }
        else
        {
            if (request_ctx.get_retry() <= max_retries_)
                files_to_download_.emplace(request, request_ctx.get_retry() + 1);
            else
                results_.emplace_back(request, error_message);
        }

        it = requests_.erase(it);
    }
}

void HttpFileDownloader::CreateAndAddRequest(const QueuedRequest& queued_request)
{
    std::string file_url = base_url_ + queued_request.get_request().get_filename();
    nitro_utils::replace_all(file_url, " ", "%20");

    auto shared_data = std::make_shared<RequestContext::Shared>();

    auto cpr_response = cpr::GetAsync(
        cpr::Url(file_url),
        cpr::ConnectTimeout(connection_timeout_),
        cpr::LowSpeed(slow_speed_threshold_, 5),
        cpr::ProgressCallback(
        [shared_data](cpr::cpr_off_t dn_total, cpr::cpr_off_t dn_now, cpr::cpr_off_t upld_total, cpr::cpr_off_t upld_now, intptr_t userdata)
        {
            if (shared_data->stop_download)
                return false;

            shared_data->download_total = dn_total;
            shared_data->download_now = dn_now;

            return true;
        }));

    requests_.emplace_back(
        queued_request.get_request(),
        queued_request.get_retry(),
        std::chrono::system_clock::now(),
        shared_data,
        std::move(cpr_response));
}

void HttpFileDownloader::StartNewDownloads()
{
    while (!files_to_download_.empty())
    {
        if (requests_.size() >= max_active_requests_)
            break;

        auto& queued_request = files_to_download_.front();
        CreateAndAddRequest(queued_request);

        files_to_download_.pop();
    }
}

bool HttpFileDownloader::CheckAllDownloadsCompleted()
{
    return files_to_download_.size() + requests_.size() == 0;
}

void HttpFileDownloader::UpdateTransferStatistics()
{
    auto current_time = system_clock::now();

    transfer_statistics_.UpdateTransferredBytes(GetDownloadedBytes(), current_time);
}

void HttpFileDownloader::CancelDownloads()
{
    std::queue<QueuedRequest>().swap(files_to_download_);

    for (const auto& request: requests_)
        request.get_shared_data()->stop_download = true;

    requests_.clear();
}

uint32_t HttpFileDownloader::GetDownloadedBytes()
{
    uint32_t active_downloads_bytes = std::accumulate(
        requests_.cbegin(),
        requests_.cend(),
        0,
        [](uint32_t sum, const auto& request)
        {
            return sum + request.get_shared_data()->download_now;
        });

    uint32_t actual_bytes_downloaded = completed_requests_bytes_downloaded_ + active_downloads_bytes;

    return actual_bytes_downloaded;
}

std::string HttpFileDownloader::FixUrl(const std::string &url)
{
    std::string fixed_url = url;

    if (fixed_url[fixed_url.length() - 1] != '/')
        fixed_url = fixed_url + '/';

    return fixed_url;
}

std::string HttpFileDownloader::ValidateResponseAndGetErrorMessage(const cpr::Response& response)
{
    if (response.error.code != cpr::ErrorCode::OK)
        return "Error: " + response.error.message;

    if (response.status_code != 200)
        return "HTTP Code: " + std::to_string(response.status_code);

    return {};
}
