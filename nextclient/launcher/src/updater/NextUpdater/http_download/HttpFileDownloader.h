#pragma once

#include <functional>
#include <cpr/cpr.h>
#include "QueuedRequest.h"
#include "RequestContext.h"
#include "TransferStatistics.h"
#include "HttpFileResult.h"

class HttpFileDownloader
{
    const int max_active_requests_ = 3;
    const std::chrono::seconds connection_timeout_ = std::chrono::seconds(7);
    const int max_retries_ = 3;
    const int32_t slow_speed_threshold_ = 1024 * 10; // bytes per sec

    std::string base_url_;
    std::function<bool(cpr::cpr_off_t downloaded, cpr::cpr_off_t speed)> progress_;

    std::queue<QueuedRequest> files_to_download_;
    std::vector<RequestContext> requests_;
    std::vector<HttpFileResult> results_;

    // statistics
    cpr::cpr_off_t completed_requests_bytes_downloaded_{};
    TransferStatistics<cpr::cpr_off_t> transfer_statistics_;

public:
    explicit HttpFileDownloader(const std::vector<HttpFileRequest>& files, const std::string& base_url, const std::function<bool(cpr::cpr_off_t downloaded, cpr::cpr_off_t speed)>& progress);
    [[nodiscard]] std::vector<HttpFileResult> StartDownloads();

private:
    void PruneCompletedRequests();
    void StartNewDownloads();
    void CreateAndAddRequest(const QueuedRequest& queued_request);
    bool CheckAllDownloadsCompleted();
    void UpdateTransferStatistics();
    void CancelDownloads();
    uint32_t GetDownloadedBytes();
    static std::string FixUrl(const std::string &url);
    static std::string ValidateResponseAndGetErrorMessage(const cpr::Response& response);
};
