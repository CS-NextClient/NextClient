#pragma once
#include <next_launcher/IBackendAddressResolver.h>
#include <next_launcher/UserInfoClient.h>
#include <next_engine_mini/AnalyticsInterface.h>
#include <taskcoro/TaskCoro.h>

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

    std::shared_ptr<next_launcher::UserInfoClient> user_info_client_;
    std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver_;

    std::shared_ptr<taskcoro::CancellationToken> ct_;

    int error_messages_sent_ = 0;

public:
    explicit Analytics(
        std::shared_ptr<next_launcher::UserInfoClient> user_info_client,
        std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver
    );

    ~Analytics() override;

    void SendAnalyticsEvent(const char *event) override;

    void SendCrashMonitoringEvent(const char *type, const char* value, bool with_stacktrace) override;
    void AddBreadcrumb(const char* category, const char* description) override;
    void SendAnalyticsLog(AnalyticsLogType type, const char* message);

    void SendPrimaryBackendEvent();
    void SendActualBackendEvent();
    void SendBranchEvent();

    static void PrepareStringToEventSegment(std::string& str);

private:
    concurrencpp::result<void> SendActualBackendEventAsync();

#ifdef GAMEANALYTICS_ENABLE
    static gameanalytics::EGAErrorSeverity LogTypeToGAError(AnalyticsLogType type);
#endif
};
