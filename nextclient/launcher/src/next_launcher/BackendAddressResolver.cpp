#include "BackendAddressResolver.h"

#include <easylogging++.h>
#include <strtools.h>
#include <data_encoding/aes.h>
#include <next_launcher/version.h>
#include <ncl_utils/backend_config_parser.h>

using namespace concurrencpp;
using namespace taskcoro;
using namespace ncl_utils;

static const char* LOG_TAG = "[BackendAddressResolver] ";

BackendAddressResolver::BackendAddressResolver(std::shared_ptr<next_launcher::UserInfoClient> user_info) :
    user_info_(std::move(user_info))
{
    InitializeHeaders();

    ct_ = CancellationToken::Create();

#ifdef UPDATER_ENABLE
    initialize_backend_task_ = TaskCoro::RunTask(TaskType::IO, ContinuationContextType::Callee, &BackendAddressResolver::ResolveBackendAddress, this);
#endif
}

BackendAddressResolver::~BackendAddressResolver()
{
    ct_->SetCanceled();
    // We don't need to wait for initialize_backend_task_, so we don't.
    // Waiting could also block the thread, which may be undesirable.
}

result<std::string> BackendAddressResolver::GetBackendAddressAsync(
    std::shared_ptr<CancellationToken> cancellation_token)
{
#ifdef UPDATER_ENABLE
    co_await TaskCoro::WithCancellation(initialize_backend_task_, cancellation_token);
    co_return backend_address_;
#else
    co_return "";
#endif
}

result<void> BackendAddressResolver::ResolveBackendAddress()
{
    backend_config_data::Config config = ParseBackendConfig("platform\\config\\backend.json");
    if (config.payload.addresses.empty())
    {
        LOG(INFO) << LOG_TAG << "Config error: address array is empty";
        co_return;
    }

    // External WithCancellation is necessary because we cannot interrupt cpr/curl during the connection establishment phase.
    // Without it, the method will only return after a timeout if the connection cannot be established.
    std::string backend_address = co_await TaskCoro::WithCancellation(
        GetFirstRespondedAddress(config.payload.addresses),
        ct_
    );

    if (backend_address.empty())
    {
        LOG(INFO) << LOG_TAG << "No response received from any backend within the specified time";
        co_return;
    }

    LOG(INFO) << LOG_TAG << "Backend address: " << backend_address;
    backend_address_ = backend_address;
}

result<std::string> BackendAddressResolver::GetFirstRespondedAddress(const std::vector<std::string>& addresses) const
{
    const auto current_time = std::chrono::steady_clock::now();

    std::vector<result<std::tuple<int, cpr::Response>>> pending_pings;
    std::string result_address;

    for (int i = 0; i < addresses.size(); i++)
    {
        auto ping_task = TaskCoro::RunIO([this, address = addresses[i], i]
        {
            cpr::Response ping_response = PingRequest(address);
            return std::make_tuple(i, std::move(ping_response));
        });

        pending_pings.emplace_back(std::move(ping_task));
    }

    while (!pending_pings.empty())
    {
        auto elapsed = std::chrono::steady_clock::now() - current_time;
        auto remaining_timeout = kPingTimeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        if (remaining_timeout <= std::chrono::milliseconds(0))
        {
            break;
        }

        try
        {
            int task_index = co_await TaskCoro::WhenAny(pending_pings, false, ct_, remaining_timeout);
            auto [original_index, response] = pending_pings[task_index].get();

            if (response.error.code == cpr::ErrorCode::OK ||
                response.error.code == cpr::ErrorCode::EMPTY_RESPONSE)
            {
                result_address = addresses[original_index];
                break;
            }

            pending_pings.erase(pending_pings.begin() + task_index);
        }
        catch (OperationTimeoutException&)
        {
            break;
        }
    }

    co_return result_address;
}

cpr::Response BackendAddressResolver::PingRequest(const std::string& address) const
{
    try
    {
        cpr::Session session;
        session.SetUrl(address);
        session.SetHeader(GetHeadersForRequest());
        session.SetConnectTimeout(kPingTimeout);
        session.SetTimeout(kPingTimeout);
        session.SetProgressCallback(cpr::ProgressCallback(
            [cancellation_token = ct_]
            (cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, intptr_t)
            {
                return !cancellation_token || !cancellation_token->IsCanceled();
            }));

        cpr::Response response = session.Get();
        return response;
    }
    catch (std::exception& e)
    {
        cpr::Response response;
        response.error.code = cpr::ErrorCode::UNKNOWN_ERROR;
        response.error.message = e.what();
        return response;
    }
}

void BackendAddressResolver::InitializeHeaders()
{
    headers_["Content-type"] = "application/json";
    headers_["UID"] = user_info_->GetClientUid();
    headers_["Branch"] = user_info_->GetUpdateBranch();
    headers_["LaunchGameCount"] = std::to_string(user_info_->GetLaunchGameCount());
    headers_["BuildVersion"] = NEXT_CLIENT_BUILD_VERSION;
    headers_["Kind"] = "Updater";
}

cpr::Header BackendAddressResolver::GetHeadersForRequest() const
{
    cpr::Header headers = headers_;
    headers["Client-Time"] = std::to_string(std::time(nullptr));

    return headers;
}
