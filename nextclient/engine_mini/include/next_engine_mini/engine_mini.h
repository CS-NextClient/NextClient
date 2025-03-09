#pragma once

#include "DownloadFileLoggerInterface.h"
#include "CommandLoggerInterface.h"
#include "AnalyticsInterface.h"
#include "HttpDownloadManagerInterface.h"
#include "NextClientVersion.h"

#ifdef NITRO_API_INCLUDED
#include <nitroapi/NitroApiInterface.h>
#else
namespace nitroapi
{
    typedef void NitroApiInterface;
}
#endif

class ISteamMatchmakingServers;

class EngineMiniInterface : public IBaseInterface
{
public:
    virtual void Init(nitroapi::NitroApiInterface* nitro_api, NextClientVersion client_version, AnalyticsInterface* analytics = nullptr) = 0;
    virtual void Uninitialize() = 0;
    virtual void GetVersion(char* buffer, int size) = 0;

    virtual HttpDownloadManagerInterface* GetHttpDownloadManager() = 0;
    virtual bool AddDownloadLogger(DownloadFileLoggerInterface* logger) = 0;
    virtual bool RemoveDownloadLogger(DownloadFileLoggerInterface* logger) = 0;

    virtual bool AddCmdLogger(CommandLoggerInterface* logger) = 0;
    virtual bool RemoveCmdLogger(CommandLoggerInterface* logger) = 0;

    virtual ISteamMatchmakingServers* GetSteamMatchmakingServers() = 0;
};

#define ENGINE_MINI_INTERFACE_VERSION "EngineMini006"

