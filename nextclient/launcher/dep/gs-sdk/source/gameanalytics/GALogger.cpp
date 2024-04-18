#include "GALogger.h"
#include "GameAnalytics.h"
#include <iostream>
#include "GADevice.h"
#include <cstdarg>
#include <exception>
#if USE_UWP
#include "GAUtilities.h"
#include <collection.h>
#include <ppltasks.h>
#elif USE_TIZEN
#include <dlog.h>
#else
#include "GAUtilities.h"
#include <vector>
#include "zf_log.h"
#endif

#if USE_UWP
#define GALOGGER_PREFIX                     L"GALogger_"
#define DEFAULT_SESSION_NAME                GALOGGER_PREFIX L"Session"
#define DEFAULT_CHANNEL_NAME                GALOGGER_PREFIX L"Channel"
#define OUR_SAMPLE_APP_LOG_FILE_FOLDER_NAME GALOGGER_PREFIX L"LogFiles"
#define LOGGING_ENABLED_SETTING_KEY_NAME    GALOGGER_PREFIX L"LoggingEnabled"
#define LOGFILEGEN_BEFORE_SUSPEND_SETTING_KEY_NAME GALOGGER_PREFIX L"LogFileGeneratedBeforeSuspend"
#endif

namespace gameanalytics
{
    namespace logging
    {
        const char* GALogger::tag = "GameAnalytics";

        bool GALogger::_destroyed = false;
        GALogger* GALogger::_instance = 0;
        std::once_flag GALogger::_initInstanceFlag;

        GALogger::GALogger()
        {
            infoLogEnabled = false;
            customLogHandler = {};

#if defined(_DEBUG)
            // log debug is in dev mode
            debugEnabled = true;
#else
            debugEnabled = false;
#endif

#if USE_UWP
            Windows::Storage::StorageFolder^ gaFolder = concurrency::create_task(Windows::Storage::ApplicationData::Current->LocalFolder->CreateFolderAsync("GameAnalytics", Windows::Storage::CreationCollisionOption::OpenIfExists)).get();
            file = concurrency::create_task(gaFolder->CreateFileAsync("ga_log.txt", Windows::Storage::CreationCollisionOption::ReplaceExisting)).get();
#endif

#if !USE_UWP && !USE_TIZEN
            logInitialized = false;
            currentLogCount = 0;
            maxLogCount = 5000;
#endif
        }

        GALogger::~GALogger()
        {
#if !USE_UWP && !USE_TIZEN
            if(logInitialized)
            {
                fclose(log_file);
            }
#endif
        }

        void GALogger::cleanUp()
        {
            _instance->customLogHandler = {};
            delete _instance;
            _instance = 0;
            _destroyed = true;
        }

        GALogger* GALogger::getInstance()
        {
            std::call_once(_initInstanceFlag, &GALogger::initInstance);
            return _instance;
        }

        void GALogger::setCustomLogHandler(const std::function<void(const char *, EGALoggerMessageType)> &handler)
        {
            GALogger *i = GALogger::getInstance();
            if (!i)
            {
                return;
            }
            i->customLogHandler = handler;
        }

        void GALogger::setInfoLog(bool enabled)
        {
            GALogger* i = GALogger::getInstance();
            if(!i)
            {
                return;
            }
            i->infoLogEnabled = enabled;
        }

        void GALogger::setVerboseInfoLog(bool enabled)
        {
            GALogger* i = GALogger::getInstance();
            if(!i)
            {
                return;
            }
            i->infoLogVerboseEnabled = enabled;
        }

#if !USE_UWP && !USE_TIZEN
        void GALogger::file_output_callback(const zf_log_message *msg, void *arg)
        {
            GALogger* i = GALogger::getInstance();
            if(!i)
            {
                return;
            }

            (void)arg;
            *msg->p = '\n';
            fwrite(msg->buf, msg->p - msg->buf + 1, 1, i->log_file);
            fflush(i->log_file);
        }

        void GALogger::initializeLog()
        {
            GALogger *ga = GALogger::getInstance();
            if(!ga)
            {
                return;
            }

            if(!ga->logInitialized)
            {
                const char* writablepath = device::GADevice::getWritablePath();

                if(device::GADevice::getWritablePathStatus() <= 0)
                {
                    return;
                }
                snprintf(ga->p, sizeof(ga->p), "%s%sga_log.txt", writablepath, utilities::GAUtilities::getPathSeparator());

                ga->log_file = fopen(ga->p, "w");
                if (!ga->log_file)
                {
                    ZF_LOGW("Failed to open log file %s", ga->p);
                    return;
                }
                zf_log_set_output_v(ZF_LOG_PUT_STD, 0, file_output_callback);

                ga->logInitialized = true;
                ga->currentLogCount = 0;

                GALogger::i("Log file added under: %s", device::GADevice::getWritablePath());
            }
        }

        void GALogger::customInitializeLog()
        {
            GALogger *ga = GALogger::getInstance();
            if(!ga)
            {
                return;
            }

            if(ga->logInitialized)
            {
                fclose(ga->log_file);
            }

            const char* writablepath = device::GADevice::getWritablePath();

            if(device::GADevice::getWritablePathStatus() <= 0)
            {
                return;
            }
            snprintf(ga->p, sizeof(ga->p), "%s%sga_log.txt", writablepath, utilities::GAUtilities::getPathSeparator());

            ga->log_file = fopen(ga->p, "w");
            if (!ga->log_file)
            {
                ZF_LOGW("Failed to open log file %s", ga->p);
                return;
            }
            zf_log_set_output_v(ZF_LOG_PUT_STD, 0, file_output_callback);

            ga->logInitialized = true;

            GALogger::i("Log file added under: %s", device::GADevice::getWritablePath());
        }
#endif

        // i: information logging
        //
        // used for:
        // - non-errors
        // - initializing, adding event, sending events etc.
        // - generally small text
        void GALogger::i(const char* format, ...)
        {
            GALogger *ga = GALogger::getInstance();
            if(!ga)
            {
                return;
            }

            if (!ga->infoLogEnabled) {
                // No logging of info unless in client debug mode
                return;
            }

            try
            {
                va_list args;
                va_start (args, format);
                size_t len = std::vsnprintf(NULL, 0, format, args);
                va_end (args);
                char* formatted = new char[len + 1];
                va_start (args, format);
                std::vsnprintf(formatted, len + 1, format, args);
                va_end (args);

                size_t s = len + 1 + 11 + strlen(ga->tag);
                char* message = new char[s];
                snprintf(message, s, "Info/%s: %s", ga->tag, formatted);
                ga->sendNotificationMessage(message, LogInfo);
                delete[] message;
                delete[] formatted;
            }
            catch(const std::exception& e)
            {
                if (!ga->debugEnabled) {
                    // No logging of debug unless in full debug logging mode
                    return;
                }
                ga->sendNotificationMessage(format, LogDebug);
            }
        }


        // w: warning logging (ALWAYS show)
        //
        // used for:
        // - validation errors
        // - trying to initialize with wrong keys
        // - other non-critical
        void GALogger::w(const char* format, ...)
        {
            GALogger *ga = GALogger::getInstance();
            if(!ga)
            {
                return;
            }

            try
            {
                va_list args;
                va_start (args, format);
                size_t len = std::vsnprintf(NULL, 0, format, args);
                va_end (args);
                char* formatted = new char[len + 1];
                va_start (args, format);
                std::vsnprintf(formatted, len + 1, format, args);
                va_end (args);

                size_t s = len + 1 + 14 + strlen(ga->tag);
                char* message = new char[s];
                snprintf(message, s, "Warning/%s: %s", ga->tag, formatted);
                ga->sendNotificationMessage(message, LogWarning);
                delete[] message;
                delete[] formatted;
            }
            catch(const std::exception& e)
            {
                if (!ga->debugEnabled) {
                    // No logging of debug unless in full debug logging mode
                    return;
                }
                ga->sendNotificationMessage(format, LogDebug);
            }
        }


        // e: error logging (ALWAYS show)
        //
        // used for:
        // - breaking app behaviour
        // - JSON decoding/encoding errors
        // - unexpected exceptions
        // - errors that never should happen
        void GALogger::e(const char* format, ...)
        {
            GALogger *ga = GALogger::getInstance();
            if(!ga)
            {
                return;
            }

            try
            {
                va_list args;
                va_start (args, format);
                size_t len = std::vsnprintf(NULL, 0, format, args);
                va_end (args);
                char* formatted = new char[len + 1];
                va_start (args, format);
                std::vsnprintf(formatted, len + 1, format, args);
                va_end (args);

                size_t s = len + 1 + 12 + strlen(ga->tag);
                char* message = new char[s];
                snprintf(message, s, "Error/%s: %s", ga->tag, formatted);
                ga->sendNotificationMessage(message, LogError);
                delete[] message;
                delete[] formatted;
            }
            catch(const std::exception& e)
            {
                if (!ga->debugEnabled) {
                    // No logging of debug unless in full debug logging mode
                    return;
                }
                ga->sendNotificationMessage(format, LogDebug);
            }
        }


        // d: debug logging (show when developing)
        //
        // used for:
        // - development only
        // - use large debug text like HTTP payload etc.
        void GALogger::d(const char* format, ...)
        {
            GALogger *ga = GALogger::getInstance();
            if(!ga)
            {
                return;
            }

            if (!ga->debugEnabled) {
                // No logging of debug unless in full debug logging mode
                return;
            }

            try
            {
                va_list args;
                va_start (args, format);
                size_t len = std::vsnprintf(NULL, 0, format, args);
                va_end (args);
                char* formatted = new char[len + 1];
                va_start (args, format);
                std::vsnprintf(formatted, len + 1, format, args);
                va_end (args);

                size_t s = len + 1 + 12 + strlen(ga->tag);
                char* message = new char[s];
                snprintf(message, s, "Debug/%s: %s", ga->tag, formatted);
                ga->sendNotificationMessage(message, LogDebug);
                delete[] message;
                delete[] formatted;
            }
            catch(const std::exception& e)
            {
                if (!ga->debugEnabled) {
                    // No logging of debug unless in full debug logging mode
                    return;
                }
                ga->sendNotificationMessage(format, LogDebug);
            }
        }


        // ii: Advanced information logging
        //
        // used for:
        // - Large logs
        void GALogger::ii(const char* format, ...)
        {
            GALogger *ga = GALogger::getInstance();
            if(!ga)
            {
                return;
            }

            if (!ga->infoLogVerboseEnabled) {
                // No logging of info unless in client debug mode
                return;
            }

            try
            {
                va_list args;
                va_start (args, format);
                size_t len = std::vsnprintf(NULL, 0, format, args);
                va_end (args);
                char* formatted = new char[len + 1];
                va_start (args, format);
                std::vsnprintf(formatted, len + 1, format, args);
                va_end (args);

                size_t s = len + 1 + 14 + strlen(ga->tag);
                char* message = new char[s];
                snprintf(message, s, "Verbose/%s: %s", ga->tag, formatted);
                ga->sendNotificationMessage(message, LogInfo);
                delete[] message;
                delete[] formatted;
            }
            catch(const std::exception& e)
            {
                if (!ga->debugEnabled) {
                    // No logging of debug unless in full debug logging mode
                    return;
                }
                ga->sendNotificationMessage(format, LogDebug);
            }
        }

        void GALogger::sendNotificationMessage(const char* message, EGALoggerMessageType type)
        {
            if(GameAnalytics::isThreadEnding())
            {
                //return;
            }

            if(customLogHandler)
            {
                customLogHandler(message, type);
                return;
            }

#if !USE_UWP && !USE_TIZEN
            if(logInitialized)
            {
                ++currentLogCount;
                if(currentLogCount >= maxLogCount)
                {
                    fclose(log_file);

                    char p_prev[513] = "";
                    const char* writablepath = device::GADevice::getWritablePath();

                    if(device::GADevice::getWritablePathStatus() <= 0)
                    {
                        return;
                    }
                    snprintf(p_prev, sizeof(p_prev), "%s%sga_log-prev.txt", writablepath, utilities::GAUtilities::getPathSeparator());

                    log_file = fopen(p, "r");
                    if (!log_file)
                    {
                        ZF_LOGW("Failed to open log file %s", p);
                        return;
                    }
                    FILE *log_file_prev;
                    log_file_prev = fopen(p_prev, "w");
                    if (!log_file_prev)
                    {
                        ZF_LOGW("Failed to open log file %s", p_prev);
                        return;
                    }

                    char ch;
                    while((ch = fgetc(log_file)) != EOF)
                    {
                        fputc(ch, log_file_prev);
                    }

                    fclose(log_file);
                    fclose(log_file_prev);

                    log_file = fopen(p, "w");
                    if (!log_file)
                    {
                        ZF_LOGW("Failed to open log file %s", p);
                        return;
                    }
                    currentLogCount = 0;
                }
            }
#endif
#if USE_UWP
            auto m = ref new Platform::String(utilities::GAUtilities::s2ws(message).c_str());
            Platform::Collections::Vector<Platform::String^>^ lines = ref new Platform::Collections::Vector<Platform::String^>();
            lines->Append(m);
#endif

#if !USE_UWP && !USE_TIZEN
            if(!logInitialized)
            {
                initializeLog();
            }
            if(device::GADevice::getWritablePathStatus() <= 0)
            {
                return;
            }
#endif
            switch(type)
            {
                case gameanalytics::LogError:
#if USE_UWP
                    try
                    {
                        concurrency::create_task(Windows::Storage::FileIO::AppendLinesAsync(file, lines)).wait();
                    }
                    catch (const std::exception&)
                    {
                    }
                    LogMessageToConsole(m);
#elif USE_TIZEN
                    dlog_print(DLOG_ERROR, GALogger::tag, message);
#else
                    std::cout << message << std::endl;
                    fprintf(log_file, "%s\n", message);
#endif
                    break;

                case gameanalytics::LogWarning:
#if USE_UWP
                    try
                    {
                        concurrency::create_task(Windows::Storage::FileIO::AppendLinesAsync(file, lines)).wait();
                    }
                    catch (const std::exception&)
                    {
                    }
                    LogMessageToConsole(m);
#elif USE_TIZEN
                    dlog_print(DLOG_WARN, GALogger::tag, message);
#else
                    std::cout << message << std::endl;
                    fprintf(log_file, "%s\n", message);
#endif
                    break;

                case gameanalytics::LogDebug:
#if USE_UWP
                    try
                    {
                        concurrency::create_task(Windows::Storage::FileIO::AppendLinesAsync(file, lines)).wait();
                    }
                    catch (const std::exception&)
                    {
                    }
                    LogMessageToConsole(m);
#elif USE_TIZEN
                    dlog_print(DLOG_DEBUG, GALogger::tag, message);
#else
                    std::cout << message << std::endl;
                    fprintf(log_file, "%s\n", message);
#endif
                    break;

                case gameanalytics::LogInfo:
#if USE_UWP
                    try
                    {
                        concurrency::create_task(Windows::Storage::FileIO::AppendLinesAsync(file, lines)).wait();
                    }
                    catch (const std::exception&)
                    {
                    }
                    LogMessageToConsole(m);
#elif USE_TIZEN
                    dlog_print(DLOG_INFO, GALogger::tag, message);
#else
                    std::cout << message << std::endl;
                    fprintf(log_file, "%s\n", message);
#endif
                    break;
            }
        }

#if USE_UWP
        void GALogger::LogMessageToConsole(Platform::Object^ parameter)
        {
            auto paraString = parameter->ToString();
            auto formattedText = std::wstring(paraString->Data()).append(L"\r\n");
            OutputDebugString(formattedText.c_str());
        }
#endif
    }
}
