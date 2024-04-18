#pragma once
#include <vector>

#include <next_engine_mini/DownloadFileLoggerInterface.h>

class DownloadLoggerAggregator : public DownloadFileLoggerInterface
{
    std::vector<DownloadFileLoggerInterface*> loggers_;

public:
    explicit DownloadLoggerAggregator() = default;
    ~DownloadLoggerAggregator() override = default;
    DownloadLoggerAggregator(DownloadLoggerAggregator& other) = delete;
    DownloadLoggerAggregator& operator=(DownloadLoggerAggregator& other) = delete;

    void AddLogFileError(const char* filename, LogFileTypeError type, int error_code, int http_code) override;
    void AddLogFile(const char* filename, size_t size, LogFileType type) override;
    void AddLogDownloadAborted(DownloadAbortedReason reason) override;

    bool AddLogger(DownloadFileLoggerInterface* logger);
    bool RemoveLogger(DownloadFileLoggerInterface* logger);
};
