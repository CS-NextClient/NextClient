#pragma once

#include <resource/ResourceDescriptor.h>
#include "http_download/DownloadLoggerAggregator.h"

extern std::shared_ptr<DownloadLoggerAggregator> g_DownloadFileLogger;

void CL_CreateHttpDownloadManager(IGameUI* game_ui,
                                  vgui2::ILocalize* localize,
                                  std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider);
void CL_DeleteHttpDownloadManager();
HttpDownloadManagerInterface* CL_GetHttpDownloadManager();
bool CL_AddDownloadFileLogger(DownloadFileLoggerInterface* logger);
bool CL_RemoveDonwloadFileLogger(DownloadFileLoggerInterface* logger);

void CL_HTTPSetDownloadUrl(const std::string& url);
int CL_HttpGetDownloadQueueSize();
void CL_QueueHTTPDownload(const ResourceDescriptor& file_resource);
void CL_HTTPUpdate();
void CL_HTTPCancel_f();
void CL_HTTPStop_f();
void CL_MarkMapAsUsingHTTPDownload();
