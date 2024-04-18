#include "../engine.h"
#include "http_download/DownloadLoggerAggregator.h"

std::shared_ptr<DownloadLoggerAggregator> g_DownloadFileLogger;

static std::unique_ptr<HttpDownloadManager> g_HttpDownloadManager;

void CL_CreateHttpDownloadManager(IGameUI* game_ui,
                                  vgui2::ILocalize* localize,
                                  std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider)
{
    if (g_DownloadFileLogger == nullptr)
        g_DownloadFileLogger = std::make_shared<DownloadLoggerAggregator>();

    g_HttpDownloadManager = std::make_unique<HttpDownloadManager>(game_ui, localize, g_DownloadFileLogger, config_provider);
}

void CL_DeleteHttpDownloadManager()
{
    g_HttpDownloadManager = nullptr;
    g_DownloadFileLogger = nullptr;
}

HttpDownloadManagerInterface* CL_GetHttpDownloadManager()
{
    return g_HttpDownloadManager.get();
}

bool CL_AddDownloadFileLogger(DownloadFileLoggerInterface* logger)
{
    if (g_DownloadFileLogger == nullptr)
        g_DownloadFileLogger = std::make_shared<DownloadLoggerAggregator>();

    return g_DownloadFileLogger->AddLogger(logger);
}

bool CL_RemoveDonwloadFileLogger(DownloadFileLoggerInterface* logger)
{
    if (g_DownloadFileLogger == nullptr)
        g_DownloadFileLogger = std::make_shared<DownloadLoggerAggregator>();

    return g_DownloadFileLogger->RemoveLogger(logger);
}

void CL_HTTPSetDownloadUrl(const std::string& url)
{
    if (cls->state == cactive_t::ca_active)
        return;

    g_HttpDownloadManager->SetUrl(url);
}

int CL_HttpGetDownloadQueueSize()
{
    return g_HttpDownloadManager->GetDownloadQueueSize();
}

void CL_QueueHTTPDownload(const resource_descriptor_t& file_resource)
{
    g_HttpDownloadManager->Queue(file_resource);
}

void CL_HTTPUpdate()
{
    g_HttpDownloadManager->Update();
}

void CL_HTTPCancel_f()
{
    g_HttpDownloadManager->Stop();
}

void CL_HTTPStop_f()
{
    g_HttpDownloadManager->Stop();
}

void CL_MarkMapAsUsingHTTPDownload()
{
    eng()->CL_MarkMapAsUsingHTTPDownload();
}
