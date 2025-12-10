#include "Analytics.h"
#include <format>
#include <easylogging++.h>
#include <taskcoro/TaskCoro.h>
#include <magic_enum/magic_enum.hpp>
#include <ncl_utils/backend_config_parser.h>
#include <ncl_utils/backend_config_data/Config.h>
#include <next_launcher/version.h>
#include <nitro_utils/string_utils.h>

#ifdef SENTRY_ENABLE
#include <sentry.h>
#endif

using namespace taskcoro;
using namespace concurrencpp;

Analytics::Analytics(
    std::shared_ptr<next_launcher::UserInfoClient> user_info_client,
    std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver)
:
    user_info_client_(std::move(user_info_client)),
    backend_address_resolver_(std::move(backend_address_resolver))
{
    ct_ = CancellationToken::Create();
}

Analytics::~Analytics()
{
    ct_->SetCanceled();
}

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

void Analytics::SendPrimaryBackendEvent()
{
    ncl_utils::backend_config_data::Config config = ncl_utils::ParseBackendConfig("platform\\config\\backend.json");
    std::string event = "primary_backend:";

    if (!config.payload.addresses.empty())
    {
        std::string backend_address = config.payload.addresses[0];
        PrepareStringToEventSegment(backend_address);

        event += backend_address;
    }
    else
    {
        event += "empty";
    }

    if (event.size() > 64)
    {
        event.resize(64);
    }

    SendAnalyticsEvent(event.c_str());
}

void Analytics::SendActualBackendEvent()
{
    TaskCoro::RunTask(TaskType::ThreadPool, ContinuationContextType::Callee, &Analytics::SendActualBackendEventAsync, this);
}

void Analytics::SendBranchEvent()
{
    std::string event = "branch:" + user_info_client_->GetUpdateBranch();

    if (event.size() > 64)
    {
        event.resize(64);
    }

    SendAnalyticsEvent(event.c_str());
}

void Analytics::PrepareStringToEventSegment(std::string& str)
{
    if (str.size() > 128)
    {
        str.resize(128);
    }

    nitro_utils::replace_all(str, "https://", "");
    nitro_utils::replace_all(str, "http://", "");

    if (str.empty())
    {
        return;
    }

    auto last_it = str.end() - 1;

    for (auto it = str.begin(); it != str.end(); )
    {
        char ch = *it;

        if ((ch == '/' || ch == '\\') && it != last_it)
        {
            *it = '_';
            ++it;
            continue;
        }

        if (nitro_utils::is_alpha_numeric_ascii(ch) ||
            ch == '-' || ch == '_' || ch == '.' ||
            ch == '(' || ch == ')' || ch == '!' || ch == '?')
        {
            ++it;
        }
        else
        {
            it = str.erase(it);
        }
    }

    if (str.size() > 64)
    {
        str.resize(64);
    }
}

result<void> Analytics::SendActualBackendEventAsync()
{
    std::string event = "actual_backend:";
    std::string address = co_await backend_address_resolver_->GetBackendAddressAsync(ct_);

    if (!address.empty())
    {
        PrepareStringToEventSegment(address);
        event += address;
    }
    else
    {
        event += "empty";
    }

    if (event.size() > 64)
    {
        event.resize(64);
    }

    SendAnalyticsEvent(event.c_str());
}
