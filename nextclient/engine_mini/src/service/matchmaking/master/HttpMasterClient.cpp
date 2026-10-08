#include "HttpMasterClient.h"

#include <utility>

#include <cpr/status_codes.h>
#include <easylogging++.h>
#include <optick.h>

#include "service/matchmaking/master/HttpMasterResponse.h"

using namespace cpr;
using namespace taskcoro;
using namespace concurrencpp;

namespace
{
    constexpr size_t kMaxResponseBytes = 8 * 1024 * 1024;
} // namespace

HttpMasterClient::HttpMasterClient(NextClientVersion client_version, std::string url) :
    url_(std::move(url))
{
    headers_.emplace("Content-type", "application/json");
    headers_.emplace("BuildVersion", BuildNextClientVersionString(client_version));
    // headers_["UID"] = user_info_->GetClientUid();
    // headers_["Branch"] = user_info_->GetUpdateBranch();
    // headers_["LaunchGameCount"] = std::to_string(user_info_->GetLaunchGameCount());
}

result<std::vector<MasterServerEntry>> HttpMasterClient::GetServerListAsync(
    std::function<void(const MasterServerEntry&)> entry_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    Response response = co_await TaskCoro::RunIO([this, cancellation_token]
    {
        Header header;
        for (auto& [key, value] : headers_)
        {
            header.emplace(key, value);
        }

        Session session;
        session.SetUrl(url_);
        session.SetHeader(header);
        session.SetBody(Body("{\"method\": \"server_list\", \"data\": {\"extended\": true}}"));
        session.SetConnectTimeout(kConnectTimeout);
        session.SetTimeout(kTimeout);
        session.SetProgressCallback(ProgressCallback([cancellation_token](cpr_pf_arg_t, cpr_pf_arg_t, cpr_pf_arg_t, cpr_pf_arg_t, intptr_t)
        {
            if (cancellation_token != nullptr && cancellation_token->IsCanceled())
            {
                return false;
            }

            return true;
        }));

        // the progress callback counts the bytes before content decoding, so the size limit is applied here
        std::string body;
        session.SetWriteCallback(WriteCallback([&body](std::string data, intptr_t)
        {
            body += data;

            return body.size() <= kMaxResponseBytes;
        }));

        Response response = session.Get();
        response.text = std::move(body);

        return response;
    });

    cancellation_token->ThrowIfCancelled();

    std::optional<std::vector<MasterServerEntry>> server_list = HttpMasterClient_ReadServerList(response);

    if (!server_list.has_value())
    {
        co_return std::vector<MasterServerEntry>{};
    }

    if (entry_received_callback)
    {
        for (const MasterServerEntry& entry : *server_list)
        {
            entry_received_callback(entry);
        }
    }

    co_return std::move(*server_list);
}

std::optional<std::vector<MasterServerEntry>> HttpMasterClient_ReadServerList(const Response& response)
{
    OPTICK_EVENT();

    if (response.error.code != cpr::ErrorCode::OK)
    {
        LOG(WARNING) << "[HttpMasterClient] Server list request failed: " << response.error.message;
        return std::nullopt;
    }

    if (response.status_code != cpr::status::HTTP_OK)
    {
        LOG(WARNING) << "[HttpMasterClient] Server list request answered with HTTP status " << response.status_code;
        return std::nullopt;
    }

    std::optional<std::vector<MasterServerEntry>> server_list = HttpMasterResponse_Parse(response.text);

    if (!server_list.has_value())
    {
        LOG(WARNING) << "[HttpMasterClient] Server list response is in no known layout";
        return std::nullopt;
    }

    if (server_list->empty())
    {
        server_list->emplace_back();
    }

    return server_list;
}
