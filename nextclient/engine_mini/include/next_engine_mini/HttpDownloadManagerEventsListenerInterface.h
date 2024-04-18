#pragma once

class HttpDownloadManagerEventsListenerInterface
{
public:
    virtual ~HttpDownloadManagerEventsListenerInterface() = default;

    virtual void OnStartDownloading() = 0;
    virtual void OnEndDownloading() = 0;
};
