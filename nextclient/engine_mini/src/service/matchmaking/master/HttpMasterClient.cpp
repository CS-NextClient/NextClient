#include "HttpMasterClient.h"

#include <optick.h>
#include <utility>

#include <nitro_utils/net_utils.h>
#include <nitro_utils/string_utils.h>

using namespace cpr;
using namespace taskcoro;
using namespace concurrencpp;

HttpMasterClient::HttpMasterClient(NextClientVersion client_version, std::string url) :
    url_(std::move(url))
{
    headers_.emplace("Content-type", "application/json");
    headers_.emplace("BuildVersion", BuildNextClientVersionString(client_version));
    // headers_["UID"] = user_info_->GetClientUid();
    // headers_["Branch"] = user_info_->GetUpdateBranch();
    // headers_["LaunchGameCount"] = std::to_string(user_info_->GetLaunchGameCount());
}

result<std::vector<netadr_t>> HttpMasterClient::GetServerAddressesAsync(
    std::function<void(const netadr_t&)> address_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    Response response = co_await TaskCoro::RunIO<Response>([this, cancellation_token]
    {
        Header header;
        for (auto& [key, value] : headers_)
        {
            header.emplace(key, value);
        }

        Session session;
        session.SetUrl(url_);
        session.SetHeader(header);
        session.SetBody(Body("{\"method\": \"server_list\", \"data\": \"null\"}"));
        session.SetProgressCallback(ProgressCallback([cancellation_token](cpr_pf_arg_t, cpr_pf_arg_t, cpr_pf_arg_t, cpr_pf_arg_t, intptr_t)
        {
            if (cancellation_token != nullptr && cancellation_token->IsCanceled())
            {
                return false;
            }

            return true;
        }));

        return session.Get();
    });

    cancellation_token->ThrowIfCancelled();

    std::vector<netadr_t> addresses = ParseResponse(response.text);

    if (address_received_callback)
    {
        for (const netadr_t& address : addresses)
        {
            address_received_callback(address);
        }
    }

    co_return addresses;
}

std::vector<netadr_t> HttpMasterClient::ParseResponse(const std::string& data)
{
    OPTICK_EVENT();

    std::vector<netadr_t> result;

    size_t last = 0, next;
    while ((next = data.find('\n', last)) != std::string::npos)
    {
        result.emplace_back(data.substr(last, next - last).c_str());
        last = next + 1;
    }
    result.emplace_back(data.substr(last).c_str());

    return result;
}
