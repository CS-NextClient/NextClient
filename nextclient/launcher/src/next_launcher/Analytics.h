#pragma once

#include <next_engine_mini/AnalyticsInterface.h>

#ifdef GAMEANALYTICS_ENABLE
#include <gameanalytics/GameAnalytics.h>
#endif

enum class AnalyticsLogType
{
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5
};

class Analytics : public AnalyticsInterface
{
    const int kMaxErrorEventsPerSession = 10; // gameanalytics restriction https://docs.gameanalytics.com/integrations/api/event-types#error-events

    int error_messages_sent_ = 0;

public:
    void SendAnalyticsEvent(const char *event) override;

    void SendCrashMonitoringEvent(const char *type, const char* value, bool with_stacktrace) override;
    void AddBreadcrumb(const char* category, const char* description) override;
    void SendAnalyticsLog(AnalyticsLogType type, const char* message);

private:
#ifdef GAMEANALYTICS_ENABLE
    static gameanalytics::EGAErrorSeverity LogTypeToGAError(AnalyticsLogType type);
#endif
};
