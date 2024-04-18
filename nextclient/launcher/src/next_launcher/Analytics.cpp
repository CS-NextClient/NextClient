#include "Analytics.h"
#include <format>
#include <easylogging++.h>
#include <magic_enum.hpp>
#include <next_launcher/version.h>
#ifdef SENTRY_ENABLE
#include <sentry.h>
#endif

#ifdef GAMEANALYTICS_ENABLE

void Analytics::SendAnalyticsEvent(const char *event)
{
    gameanalytics::GameAnalytics::addDesignEvent(event);
}

void Analytics::SendAnalyticsLog(AnalyticsLogType type, const char* message)
{
    if (error_messages_sent_ >= kMaxErrorEventsPerSession)
    {
        LOG(WARNING) << "Max analytics event exceeded (max " << kMaxErrorEventsPerSession << "), type: " << magic_enum::enum_name(type) << ", message: " << message;
        return;
    }

    gameanalytics::GameAnalytics::addErrorEvent(LogTypeToGAError(type), std::format("{} {}", NEXT_CLIENT_BUILD_VERSION, message).c_str());
    error_messages_sent_++;
}

gameanalytics::EGAErrorSeverity Analytics::LogTypeToGAError(AnalyticsLogType type)
{
    switch (type)
    {
        case AnalyticsLogType::Debug:    return gameanalytics::EGAErrorSeverity::Debug;
        case AnalyticsLogType::Info:     return gameanalytics::EGAErrorSeverity::Info;
        case AnalyticsLogType::Warning:  return gameanalytics::EGAErrorSeverity::Warning;
        case AnalyticsLogType::Error:    return gameanalytics::EGAErrorSeverity::Error;
        case AnalyticsLogType::Critical: return gameanalytics::EGAErrorSeverity::Critical;
    }

    return gameanalytics::EGAErrorSeverity::Info;
}

#else

void Analytics::SendAnalyticsEvent(const char *event) { }
void Analytics::SendAnalyticsLog(AnalyticsLogType type, const char* message) { }

#endif


#ifdef SENTRY_ENABLE

void Analytics::SendCrashMonitoringEvent(const char *type, const char *value, bool with_stacktrace)
{

    sentry_value_t event = sentry_value_new_event();

    sentry_value_t exc = sentry_value_new_exception(type, value);
    sentry_event_add_exception(event, exc);
    if (with_stacktrace)
        sentry_value_set_stacktrace(exc, nullptr, 0);

    sentry_capture_event(event);
}

void Analytics::AddBreadcrumb(const char* category, const char* description)
{
    sentry_value_t crumb = sentry_value_new_breadcrumb("default", description);
    sentry_value_set_by_key(crumb, "category", sentry_value_new_string(category));
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string("info"));
    sentry_add_breadcrumb(crumb);
}

#else

void Analytics::SendCrashMonitoringEvent(const char *type, const char *value, bool with_stacktrace) { }
void Analytics::AddBreadcrumb(const char* category, const char* description) { }

#endif
