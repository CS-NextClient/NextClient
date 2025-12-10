#include "NextUpdaterHttpService.h"

#include <easylogging++.h>
#include <nitro_utils/random_utils.h>
#include <data_encoding/aes.h>
#include <data_encoding/base64.h>
#include <ncl_utils/backend_config_parser.h>
#include <tao/json.hpp>
#include <cpr/api.h>
#include <next_launcher/version.h>
#include <taskcoro/TaskCoro.h>
#include "NextUpdaterHttpServiceConfig.h"

using namespace ncl_utils;
using namespace ncl_utils::backend_config_data;
using namespace concurrencpp;
using namespace std::chrono_literals;
using namespace taskcoro;

NextUpdaterHttpService::NextUpdaterHttpService(
    std::shared_ptr<next_launcher::UserInfoClient> user_info,
    std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver)
:
    user_info_(std::move(user_info)),
    backend_address_resolver_(std::move(backend_address_resolver))
{
    ct_ = CancellationToken::Create();

    InitializeHeaders();
}

NextUpdaterHttpService::~NextUpdaterHttpService()
{
}

result<void> NextUpdaterHttpService::ShutdownAsync()
{
    ct_->SetCanceled();
    co_await task_tracker_.WaitAsync();
}

result<HttpResponse> NextUpdaterHttpService::PostAsync(
    std::string method,
    std::string data,
    std::shared_ptr<CancellationToken> cancellation_token)
{
    TaskTracker::Token tracker_token = task_tracker_.MakeToken();

    std::shared_ptr<CancellationToken> linked_ct = LinkedCancellationToken::Create({cancellation_token, ct_});

    co_await InitializeBackendAddressIfNeeded(linked_ct);

    result<HttpResponse> internal_task = TaskCoro::RunIO(
        [this, method, data, linked_ct]() mutable
        {
            return PostInternal(method, data, linked_ct);
        }
    );

    // External WithCancellation is necessary because we cannot interrupt cpr/curl during the connection establishment phase.
    // Without it, the method will only return after a timeout if the connection cannot be established.
    HttpResponse result = co_await TaskCoro::WithCancellation(std::move(internal_task), linked_ct);

    co_return result;
}

HttpResponse NextUpdaterHttpService::PostInternal(
    const std::string& method,
    const std::string& data,
    std::shared_ptr<CancellationToken> cancellation_token)
{
    if (backend_address_.empty())
    {
        cpr::Error error;
        error.code = cpr::ErrorCode::CONNECTION_FAILURE;
        error.message = "no backend address";
        return HttpResponse(0, error, "");
    }

    std::string request_body = BuildJsonPayload(method, data);

    cpr::Session session;
    session.SetUrl(backend_address_);
    session.SetBody(SerializeAes(request_body));
    session.SetHeader(GetHeadersForRequest());
    session.SetConnectTimeout(kConnectTimeout);
    session.SetProgressCallback(cpr::ProgressCallback(
        [cancellation_token]
        (cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, intptr_t)
        {
            return !cancellation_token || !cancellation_token->IsCanceled();
        }));

    cpr::Response response = session.Post();

    if (cancellation_token)
    {
        cancellation_token->ThrowIfCancelled();
    }

    return ParseResponse(response);
}

result<void> NextUpdaterHttpService::InitializeBackendAddressIfNeeded(std::shared_ptr<CancellationToken> cancellation_token)
{
    if (is_backend_address_initialized_)
    {
        co_return;
    }

    backend_address_ = co_await backend_address_resolver_->GetBackendAddressAsync(cancellation_token);

    is_backend_address_initialized_ = true;
}

void NextUpdaterHttpService::InitializeHeaders()
{
    headers_["Content-type"] = "application/json";
    headers_["UID"] = user_info_->GetClientUid();
    headers_["Branch"] = user_info_->GetUpdateBranch();
    headers_["LaunchGameCount"] = std::to_string(user_info_->GetLaunchGameCount());
    headers_["BuildVersion"] = NEXT_CLIENT_BUILD_VERSION;
    headers_["Kind"] = "Updater";
}

cpr::Header NextUpdaterHttpService::GetHeadersForRequest() const
{
    cpr::Header headers = headers_;
    headers["Client-Time"] = std::to_string(std::time(nullptr));

    return headers;
}

std::string NextUpdaterHttpService::BuildJsonPayload(const std::string& method, const std::string& data) const
{
    std::string json = data;
    json.insert(0, "{\"method\": \"" + method + "\", \"data\": ");
    json.append("}");

    return json;
}

HttpResponse NextUpdaterHttpService::ParseResponse(const cpr::Response& response) const
{
    if (response.status_code != 200 || response.error)
    {
        return HttpResponse(response.status_code, response.error, "");
    }

    if (response.text.empty() || response.text == "OK")
    {
        return HttpResponse(response.status_code, response.error, response.text);
    }

    try
    {
        std::string payload = DeserializeAes(response.text);
        return HttpResponse(response.status_code, response.error, payload);
    }
    catch (tao::json::pegtl::parse_error&)
    { }

    cpr::Error error;
    error.code = cpr::ErrorCode::UNKNOWN_ERROR;
    error.message = "deserialization error";
    return HttpResponse(response.status_code, error, "");
}

std::string NextUpdaterHttpService::SerializeAes(const std::string& data) const
{
    size_t data_len = data.size() + 1; // \0 at end
    size_t cipher_length = ((data_len / AES128_KEY_LEN) + ((data_len % AES128_KEY_LEN) > 0 ? 1 : 0)) * AES128_KEY_LEN;

    auto input_data = std::make_unique<uint8_t[]>(cipher_length);
    SecureZeroMemory(input_data.get(), cipher_length);

    auto output_data = std::make_unique<uint8_t[]>(cipher_length);
    SecureZeroMemory(output_data.get(), cipher_length);

    strcpy_s(reinterpret_cast<char*>(input_data.get()), data_len, data.c_str());

    auto iv = nitro_utils::GenerateRandomBytesSecure(AES128_KEY_LEN);
    AES128_CBC_encrypt_buffer(output_data.get(), input_data.get(), cipher_length, kHttpServiceCipherKey, iv.data());

    auto data_base64 = base64_encode(output_data.get(), cipher_length);
    auto iv_base64 = base64_encode(iv.data(), AES128_KEY_LEN);

    tao::json::value v = {
        { std::string("iv"), iv_base64 },
        { std::string("data"), data_base64 }
    };

    return tao::json::to_string(v);
}

std::string NextUpdaterHttpService::DeserializeAes(const std::string& json_str) const
{
    tao::json::value v;
    try
    {
        v = tao::json::from_string(json_str);
    }
    catch (...)
    {
        return "";
    }

    auto data_base64 = v.optional<std::string>("data");
    auto iv_base64 = v.optional<std::string>("iv");

    if(!data_base64 || !iv_base64)
        return "";

    auto data = base64_decode(*data_base64);
    auto iv = base64_decode(*iv_base64);

    if (iv.size() != AES128_KEY_LEN)
        return "";

    if (data.size() < AES128_KEY_LEN)
        return "";

    std::vector<uint8_t> decrypted_data(data.size());
    SecureZeroMemory(decrypted_data.data(), decrypted_data.size());
    AES128_CBC_decrypt_buffer(decrypted_data.data(), data.data(), data.size(), kHttpServiceCipherKey, iv.data());
    decrypted_data.push_back('\0');

    return reinterpret_cast<const char*>(decrypted_data.data());
}
