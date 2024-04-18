#pragma once

#include "HttpDownloadManagerEventsListenerInterface.h"
#include "DownloadFileLoggerInterface.h"

class HttpDownloadManagerInterface
{
public:
    virtual ~HttpDownloadManagerInterface() = default;

    virtual bool AddListener(HttpDownloadManagerEventsListenerInterface* listener) = 0;
    virtual bool RemoveListener(HttpDownloadManagerEventsListenerInterface* listener) = 0;
};
