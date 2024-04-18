#pragma once

class AnalyticsInterface
{
public:
    virtual ~AnalyticsInterface() = default;

    virtual void SendAnalyticsEvent(const char* event) = 0;
    virtual void SendCrashMonitoringEvent(const char* type, const char* value, bool with_stacktrace) = 0;
    virtual void AddBreadcrumb(const char* category, const char* description) = 0;
};