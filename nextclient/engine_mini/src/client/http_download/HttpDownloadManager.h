#pragma once

#include <string>
#include <queue>
#include <future>
#include <chrono>

#include <cpr/api.h>

#include <next_engine_mini/HttpDownloadManagerInterface.h>
#include <next_engine_mini//DownloadFileLoggerInterface.h>
#include <nitro_utils/config_utils.h>
#include <vgui/ILocalize.h>
#include <IGameUI.h>

#include <resource/ResourceDescriptor.h>

#include "TransferStatistics.h"
#include "RequestContext.hpp"
#include "QueuedRequest.hpp"

class HttpDownloadManager : public HttpDownloadManagerInterface
{
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    const int MAX_POSSIBLE_ACTIVE_REQUESTS = 30;

    cvar_t* cvar_max_active_requests;
    cvar_t* cvar_max_requests_retries;

    const uint32_t slow_speed_threshold_ = 1024 * 30; // bytes per sec
    const std::chrono::seconds connection_timeout_ = std::chrono::seconds(7);
    const std::chrono::seconds slow_speed_fallback_timeout_ = std::chrono::seconds(5);
    const std::chrono::seconds slow_speed_fallback_timeout_ui_ = std::chrono::seconds(3);

    IGameUI* game_ui_;
    vgui2::ILocalize* localize_;

    std::shared_ptr<DownloadFileLoggerInterface> download_logger_;
    std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider_;

    std::vector<HttpDownloadManagerEventsListenerInterface*> listeners_;

    std::string base_url_;

    std::queue<QueuedRequest> files_to_download_;
    std::vector<RequestContext> requests_;

    bool is_download_active_ = false;
    bool is_slow_speed_ = false;
    time_point slow_speed_start_time_{};
    uint32_t total_files_to_download_ = 0;
    uint32_t total_bytes_to_download_ = 0;
    uint32_t completed_requests_bytes_downloaded_ = 0;

    TransferStatistics<uint32_t> download_statistics_;
    time_point bytes_downloaded_prev_time_{};

public:
    explicit HttpDownloadManager(IGameUI* game_ui,
                                 vgui2::ILocalize* localize,
                                 std::shared_ptr<DownloadFileLoggerInterface> download_logger,
                                 std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider);

    void SetUrl(const std::string& url);
    void Queue(const ResourceDescriptor& file_resource);
    void Stop();
    uint32_t GetDownloadQueueSize();
    void Update();

    bool AddListener(HttpDownloadManagerEventsListenerInterface* listener) override;
    bool RemoveListener(HttpDownloadManagerEventsListenerInterface* listener) override;

private:
    void InvokeStartDownloadingEvent();
    void InvokeEndDownloadingEvent();
    void PruneCompletedRequests();
    void StartNewDownloads();
    void UpdateUi();
    void UpdateDownloadSpeed();
    void SlowSpeedDetection();
    void CheckAllDownloadsCompleted();
    uint32_t GetDownloadedBytes();

    int GetMaxActiveRequests();
    int GetMaxRequestsRetries();

    std::optional<std::string> ValidateResponseAndGetErrorMessage(const cpr::Response& response);
    bool ValidateDownloadedData(const std::string &data);

public:
    static std::string FormatFileSize(size_t size, short precision);
    static bool ValidateUrl(const std::string &url);
    static std::string FixUrl(const std::string &url);
};
