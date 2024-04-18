#pragma once

enum class LogFileType
{
    FileDownloaded  = 0,
    FileConsistency = 1
};

enum class LogFileTypeError
{
    FileMissingHTTP = 0,
    FileMissing     = 1,
    FileBlocked     = 2,
    FileErrorHTTP   = 3,
    FileSaveError   = 4
};

enum class DownloadAbortedReason
{
    HttpSlowSpeed = 1
};

class DownloadFileLoggerInterface
{
public:
    virtual ~DownloadFileLoggerInterface() = default;

    virtual void AddLogFileError(const char* filename, LogFileTypeError type, int error_code, int http_code) = 0;
    virtual void AddLogFile(const char* filename, size_t size, LogFileType type) = 0;
    virtual void AddLogDownloadAborted(DownloadAbortedReason reason) = 0;
};
