#include "DownloadLoggerAggregator.h"

void DownloadLoggerAggregator::AddLogFileError(const char* filename, LogFileTypeError type, int error_code, int http_code)
{
    for (auto& logger : loggers_)
        logger->AddLogFileError(filename, type, error_code, http_code);
}

void DownloadLoggerAggregator::AddLogFile(const char* filename, size_t size, LogFileType type)
{
    for (auto& logger : loggers_)
        logger->AddLogFile(filename, size, type);
}

void DownloadLoggerAggregator::AddLogDownloadAborted(DownloadAbortedReason reason)
{
    for (auto& logger : loggers_)
        logger->AddLogDownloadAborted(reason);
}

bool DownloadLoggerAggregator::AddLogger(DownloadFileLoggerInterface* logger)
{
    if (std::find(loggers_.cbegin(), loggers_.cend(), logger) != loggers_.cend())
        return false;

    loggers_.push_back(logger);
    return true;
}

bool DownloadLoggerAggregator::RemoveLogger(DownloadFileLoggerInterface* logger)
{
    auto logger_it = std::find(loggers_.cbegin(), loggers_.cend(), logger);
    if (logger_it == loggers_.cend())
        return false;

    loggers_.erase(logger_it);
    return true;
}
