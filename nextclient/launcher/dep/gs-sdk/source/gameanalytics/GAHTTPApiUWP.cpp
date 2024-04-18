#if USE_UWP
#include "GAHTTPApi.h"
#include "GAState.h"
#include "GALogger.h"
#include "GAUtilities.h"
#include "GAValidator.h"
#include <map>
#include <robuffer.h>
#include <assert.h>
#if USE_UWP
#include "GADevice.h"
#endif
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

namespace gameanalytics
{
    namespace http
    {
        // base url settings
        char GAHTTPApi::protocol[6] = "https";
        char GAHTTPApi::hostName[22] = "api.gameanalytics.com";

        char GAHTTPApi::version[3] = "v2";
        char GAHTTPApi::remoteConfigsVersion[3] = "v1";

        // create base url
        char GAHTTPApi::baseUrl[257] = "";
        char GAHTTPApi::remoteConfigsBaseUrl[257] = "";

        // route paths
        char GAHTTPApi::initializeUrlPath[5] = "init";
        char GAHTTPApi::eventsUrlPath[7] = "events";
        // Constructor - setup the basic information for HTTP
        GAHTTPApi::GAHTTPApi()
        {
            // use gzip compression on JSON body
#if defined(_DEBUG)
            useGzip = false;
#else
            useGzip = false;
#endif
            snprintf(GAHTTPApi::baseUrl, sizeof(GAHTTPApi::baseUrl), "%s://%s/%s", protocol, hostName, version);
            snprintf(GAHTTPApi::remoteConfigsBaseUrl, sizeof(GAHTTPApi::remoteConfigsBaseUrl), "%s://%s/remote_configs/%s", protocol, hostName, remoteConfigsVersion);
            httpClient = ref new Windows::Web::Http::HttpClient();
            Windows::Networking::Connectivity::NetworkInformation::NetworkStatusChanged += ref new Windows::Networking::Connectivity::NetworkStatusChangedEventHandler(&GANetworkStatus::NetworkInformationOnNetworkStatusChanged);
            GANetworkStatus::CheckInternetAccess();
        }

        GAHTTPApi::~GAHTTPApi()
        {
        }

        bool GAHTTPApi::_destroyed = false;
        GAHTTPApi* GAHTTPApi::_instance = 0;
        std::once_flag GAHTTPApi::_initInstanceFlag;

        void GAHTTPApi::cleanUp()
        {
            delete _instance;
            _instance = 0;
            _destroyed = true;
        }

        GAHTTPApi* GAHTTPApi::getInstance()
        {
            std::call_once(_initInstanceFlag, &GAHTTPApi::initInstance);
            return _instance;
        }

        bool GANetworkStatus::hasInternetAccess = false;

        void GANetworkStatus::NetworkInformationOnNetworkStatusChanged(Platform::Object^ sender)
        {
            CheckInternetAccess();
        }

        void GANetworkStatus::CheckInternetAccess()
        {
            auto connectionProfile = Windows::Networking::Connectivity::NetworkInformation::GetInternetConnectionProfile();
            hasInternetAccess = (connectionProfile != nullptr && connectionProfile->GetNetworkConnectivityLevel() == Windows::Networking::Connectivity::NetworkConnectivityLevel::InternetAccess);

            if (hasInternetAccess)
            {
                if (connectionProfile->IsWlanConnectionProfile)
                {
                    device::GADevice::setConnectionType("wifi");
                }
                else if (connectionProfile->IsWwanConnectionProfile)
                {
                    device::GADevice::setConnectionType("wwan");
                }
                else
                {
                    device::GADevice::setConnectionType("lan");
                }
            }
            else
            {
                device::GADevice::setConnectionType("offline");
            }
        }

        concurrency::task<std::pair<EGAHTTPApiResponse, std::string>> GAHTTPApi::requestInitReturningDict(const char* configsHash)
        {
            std::string gameKey = state::GAState::getGameKey();

            std::string hash = std::string(configsHash);

            // Generate URL
            std::string url = std::string(remoteConfigsBaseUrl) + "/" + std::string(initializeUrlPath) + "?game_key=" + std::string(gameKey) + "&interval_seconds=0&configs_hash=" + hash;

            logging::GALogger::d("Sending 'init' URL: %s", url.c_str());

            rapidjson::Document initAnnotations;
            initAnnotations.SetObject();
            state::GAState::getInitAnnotations(initAnnotations);

            // make JSON string from data
            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                initAnnotations.Accept(writer);
            }
            std::string JSONstring = buffer.GetString();

            if (JSONstring.empty())
            {
                return concurrency::create_task([]()
                {
                    return std::pair<EGAHTTPApiResponse, std::string>(JsonEncodeFailed, "");
                });
            }

            std::vector<char> payloadData = createPayloadData(JSONstring.c_str(), useGzip);
            auto message = ref new Windows::Web::Http::HttpRequestMessage();

            std::vector<char> authorization = createRequest(message, url, payloadData, useGzip);

            if (!GANetworkStatus::hasInternetAccess)
            {
                return concurrency::create_task([]()
                {
                    return std::pair<EGAHTTPApiResponse, std::string>(NoResponse, "");
                });
            }

            return concurrency::create_task(httpClient->SendRequestAsync(message)).then([=](Windows::Web::Http::HttpResponseMessage^ response)
            {
                EGAHTTPApiResponse requestResponseEnum = processRequestResponse(response, "Init");

                // if not 200 result
                if (requestResponseEnum != Ok && requestResponseEnum != Created && requestResponseEnum != BadRequest)
                {
                    logging::GALogger::d("Failed Init Call. URL: %s, JSONString: %s, Authorization: %s", url.c_str(), JSONstring.c_str(), authorization.data());
                    return std::pair<EGAHTTPApiResponse, std::string>(requestResponseEnum, "");
                }

                // print reason if bad request
                if (requestResponseEnum == BadRequest)
                {
                    logging::GALogger::d("Failed Init Call. Bad request. Response: %s", utilities::GAUtilities::ws2s(response->Content->ToString()->Data()).c_str());
                    // return bad request result
                    return std::pair<EGAHTTPApiResponse, std::string>(requestResponseEnum, "");
                }

                concurrency::task<Platform::String^> readTask(response->Content->ReadAsStringAsync());
                Platform::String^ responseBodyAsText = readTask.get();

                // Return response.
                std::string body = utilities::GAUtilities::ws2s(responseBodyAsText->Data());

                logging::GALogger::d("init request content : %s", body.c_str());

                rapidjson::Document requestJsonDict;
                requestJsonDict.Parse(body.c_str());

                if (requestJsonDict.IsNull())
                {
                    logging::GALogger::d("Failed Init Call. Json decoding failed");
                    return std::pair<EGAHTTPApiResponse, std::string>(JsonDecodeFailed, "");
                }

                rapidjson::Document validatedInitValues;
                // validate Init call values
                validators::GAValidator::validateAndCleanInitRequestResponse(requestJsonDict, validatedInitValues, requestResponseEnum == Created);

                if (validatedInitValues.IsNull())
                {
                    return std::pair<EGAHTTPApiResponse, std::string>(BadResponse, "");
                }

                rapidjson::StringBuffer buffer;
                {
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    validatedInitValues.Accept(writer);
                }
                // all ok
                return std::pair<EGAHTTPApiResponse, std::string>(requestResponseEnum, buffer.GetString());
            });
        }

        concurrency::task<std::pair<EGAHTTPApiResponse, std::string>> GAHTTPApi::sendEventsInArray(const rapidjson::Value& eventArray)
        {
            if (eventArray.Empty())
            {
                logging::GALogger::d("sendEventsInArray called with missing eventArray");
            }

            auto gameKey = state::GAState::getGameKey();

            // Generate URL
            std::string url = std::string(baseUrl) + "/" + std::string(gameKey) + "/" + std::string(eventsUrlPath);
            logging::GALogger::d("Sending 'events' URL: %s", url.c_str());

            // make JSON string from data
            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                eventArray.Accept(writer);
            }

            std::string JSONstring = buffer.GetString();

            if (JSONstring.empty())
            {
                logging::GALogger::d("sendEventsInArray JSON encoding failed of eventArray");
                return concurrency::create_task([]()
                {
                    return std::pair<EGAHTTPApiResponse, std::string>(JsonDecodeFailed, "");
                });
            }

            std::vector<char> payloadData = createPayloadData(JSONstring.c_str(), useGzip);
            auto message = ref new Windows::Web::Http::HttpRequestMessage();

            std::string authorization = createRequest(message, url, payloadData, useGzip).data();

            if (!GANetworkStatus::hasInternetAccess)
            {
                return concurrency::create_task([]()
                {
                    return std::pair<EGAHTTPApiResponse, std::string>(NoResponse, "");
                });
            }

            return concurrency::create_task(httpClient->SendRequestAsync(message)).then([=](Windows::Web::Http::HttpResponseMessage^ response)
            {
                EGAHTTPApiResponse requestResponseEnum = processRequestResponse(response, "Events");

                // if not 200 result
                if (requestResponseEnum != Ok && requestResponseEnum != Created && requestResponseEnum != BadRequest)
                {
                    logging::GALogger::d("Failed Events Call. URL: %s, JSONString: %s, Authorization: %s", url.c_str(), JSONstring.c_str(), authorization.c_str());
                    return std::pair<EGAHTTPApiResponse, std::string>(requestResponseEnum,"");
                }

                // print reason if bad request
                if (requestResponseEnum == BadRequest)
                {
                    logging::GALogger::d("Failed Events Call. Bad request. Response: %s", utilities::GAUtilities::ws2s(response->Content->ToString()->Data()).c_str());
                    // return bad request result
                    return std::pair<EGAHTTPApiResponse, std::string>(requestResponseEnum,"");
                }

                concurrency::task<Platform::String^> readTask(response->Content->ReadAsStringAsync());
                Platform::String^ responseBodyAsText = readTask.get();

                // Return response.
                std::string body = utilities::GAUtilities::ws2s(responseBodyAsText->Data());

                logging::GALogger::d("body: %s", body.c_str());

                rapidjson::Document requestJsonDict;
                requestJsonDict.Parse(body.c_str());

                if (requestJsonDict.IsNull())
                {
                    return std::pair<EGAHTTPApiResponse, std::string>(JsonDecodeFailed,"");
                }

                // all ok
                rapidjson::StringBuffer buffer;
                {
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    requestJsonDict.Accept(writer);
                }
                return std::pair<EGAHTTPApiResponse, std::string>(requestResponseEnum, buffer.GetString());
            });
        }

        void GAHTTPApi::sendSdkErrorEvent(EGASdkErrorCategory category, EGASdkErrorArea area, EGASdkErrorAction action, EGASdkErrorParameter parameter, std::string reason, std::string gameKey, std::string secretKey)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            // Validate
            if (!validators::GAValidator::validateSdkErrorEvent(gameKey.c_str(), secretKey.c_str(), category, area, action))
            {
                return;
            }

            // Generate URL
            std::string url = std::string(baseUrl) + "/" + std::string(gameKey) + "/" + std::string(eventsUrlPath);
            logging::GALogger::d("Sending 'events' URL: %s", url.c_str());

            rapidjson::Document json;
            json.SetObject();
            state::GAState::getSdkErrorEventAnnotations(json);

            char categoryString[40] = "";
            sdkErrorCategoryString(category, categoryString);
            {
                rapidjson::Value v(categoryString, json.GetAllocator());
                json.AddMember("error_category", v.Move(), json.GetAllocator());
            }

            char areaString[40] = "";
            sdkErrorAreaString(area, areaString);
            {
                rapidjson::Value v(areaString, json.GetAllocator());
                json.AddMember("error_area", v.Move(), json.GetAllocator());
            }

            char actionString[40] = "";
            sdkErrorActionString(action, actionString);
            {
                rapidjson::Value v(actionString, json.GetAllocator());
                json.AddMember("error_action", v.Move(), json.GetAllocator());
            }

            char parameterString[40] = "";
            sdkErrorParameterString(parameter, parameterString);
            if(strlen(parameterString) > 0)
            {
                rapidjson::Value v(parameterString, json.GetAllocator());
                json.AddMember("error_parameter", v.Move(), json.GetAllocator());
            }

            if(reason.length() > 0)
            {
                rapidjson::Value v(reason.c_str(), json.GetAllocator());
                json.AddMember("reason", v.Move(), json.GetAllocator());
            }

            rapidjson::Document eventArray;
            eventArray.SetArray();
            rapidjson::Document::AllocatorType& allocator = eventArray.GetAllocator();
            eventArray.PushBack(json, allocator);
            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                eventArray.Accept(writer);
            }
            std::string payloadJSONString =  buffer.GetString();

            if (payloadJSONString.empty())
            {
                logging::GALogger::w("sendSdkErrorEvent: JSON encoding failed.");
                return;
            }

            logging::GALogger::d("sendSdkErrorEvent json: %s", payloadJSONString.c_str());

            ErrorType errorType = std::make_tuple(category, area);

            int64_t now = utilities::GAUtilities::timeIntervalSince1970();
            if(timestampMap.count(errorType) == 0)
            {
                timestampMap[errorType] = now;
            }
            if(countMap.count(errorType) == 0)
            {
                countMap[errorType] = 0;
            }

            int64_t diff = now - timestampMap[errorType];
            if(diff >= 3600)
            {
                countMap[errorType] = 0;
                timestampMap[errorType] = now;
            }

            if(countMap[errorType] >= MaxCount)
            {
                return;
            }

            std::vector<char> payloadData = createPayloadData(payloadJSONString.c_str(), useGzip);
            auto message = ref new Windows::Web::Http::HttpRequestMessage();

            std::vector<char> authorization = createRequest(message, url, payloadData, useGzip);

            if (!GANetworkStatus::hasInternetAccess)
            {
                return;
            }

            concurrency::create_task(httpClient->SendRequestAsync(message)).then([=](Windows::Web::Http::HttpResponseMessage^ response)
            {
                Windows::Web::Http::HttpStatusCode statusCode = response->StatusCode;

                // if not 200 result
                if (statusCode != Windows::Web::Http::HttpStatusCode::Ok)
                {
                    logging::GALogger::d("sdk error failed. response code not 200. status code: %s", utilities::GAUtilities::ws2s(statusCode.ToString()->Data()).c_str());
                    return;
                }

                concurrency::task<Platform::String^> readTask(response->Content->ReadAsStringAsync());
                Platform::String^ responseBodyAsText = readTask.get();

                // Return response.
                std::string body = utilities::GAUtilities::ws2s(responseBodyAsText->Data());

                logging::GALogger::d("init request content : %s", body.c_str());

                countMap[errorType] = countMap[errorType] + 1;
            });
        }

        const int GAHTTPApi::MaxCount = 10;
        std::map<ErrorType, int> GAHTTPApi::countMap = std::map<ErrorType, int>();
        std::map<ErrorType, int64_t> GAHTTPApi::timestampMap = std::map<ErrorType, int64_t>();

        std::vector<char> GAHTTPApi::createPayloadData(const char* payload, bool gzip)
        {
            std::vector<char> payloadData;

            if (gzip)
            {
                payloadData = utilities::GAUtilities::gzipCompress(payload);
                logging::GALogger::d("Gzip stats. Size: %d, Compressed: %d", strlen(payload), payloadData.size());
            }
            else
            {
                size_t s = strlen(payload);

                for(size_t i = 0; i < s; ++i)
                {
                    payloadData.push_back(payload[i]);
                }
            }

            return payloadData;
        }

        std::vector<char> GAHTTPApi::createRequest(Windows::Web::Http::HttpRequestMessage^ message, const std::string& url, const std::vector<char>& payloadData, bool gzip)
        {
            auto urlString = ref new Platform::String(utilities::GAUtilities::s2ws(url).c_str());
            message->RequestUri = ref new Windows::Foundation::Uri(urlString);
            message->Method = Windows::Web::Http::HttpMethod::Post;

            // create authorization hash
            auto data = ref new Platform::String(utilities::GAUtilities::s2ws(payloadData.data()).c_str());
            auto key = ref new Platform::String(utilities::GAUtilities::s2ws(state::GAState::getGameSecret()).c_str());
            auto input = Windows::Security::Cryptography::CryptographicBuffer::ConvertStringToBinary(data,
                Windows::Security::Cryptography::BinaryStringEncoding::Utf8);
            auto keyBuffer = Windows::Security::Cryptography::CryptographicBuffer::ConvertStringToBinary(key,
                Windows::Security::Cryptography::BinaryStringEncoding::Utf8);
            auto macProvider = Windows::Security::Cryptography::Core::MacAlgorithmProvider::OpenAlgorithm(Windows::Security::Cryptography::Core::MacAlgorithmNames::HmacSha256);
            auto signatureKey = macProvider->CreateKey(keyBuffer);
            auto hashed = Windows::Security::Cryptography::Core::CryptographicEngine::Sign(signatureKey, input);
            auto authorization = Windows::Security::Cryptography::CryptographicBuffer::EncodeToBase64String(hashed);

            message->Headers->TryAppendWithoutValidation(L"Authorization", authorization);

            message->Content = ref new Windows::Web::Http::HttpStringContent(ref new Platform::String(utilities::GAUtilities::s2ws(payloadData.data()).c_str()), Windows::Storage::Streams::UnicodeEncoding::Utf8);

            if (gzip)
            {
                //Windows::Storage::Streams::InMemoryRandomAccessStream^ stream = createStream(std::string(payloadData.data())).get();
                //message->Content = ref new Windows::Web::Http::HttpStreamContent(stream);
                message->Content->Headers->ContentEncoding->Append(ref new Windows::Web::Http::Headers::HttpContentCodingHeaderValue(ref new Platform::String(L"gzip")));
            }
            else
            {

            }
            message->Content->Headers->ContentType = ref new Windows::Web::Http::Headers::HttpMediaTypeHeaderValue(L"application/json");

            std::string r = utilities::GAUtilities::ws2s(authorization->Data());
            std::vector<char> result;
            for(size_t i = 0; i < r.size(); ++i)
            {
                result.push_back(r[i]);
            }
            result.push_back('\0');
            return result;
        }

        concurrency::task<Windows::Storage::Streams::InMemoryRandomAccessStream^> GAHTTPApi::createStream(std::string data)
        {
            using namespace Windows::Storage::Streams;
            using namespace Microsoft::WRL;
            using namespace Platform;
            using namespace concurrency;

            return create_task([=]()
            {
                size_t size = data.size();
                Buffer^ buffer = ref new Buffer(size);
                buffer->Length = size;

                ComPtr<IBufferByteAccess> bufferByteAccess;
                HRESULT hr = reinterpret_cast<IUnknown*>(buffer)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));
                if (FAILED(hr))
                {
                    throw ref new Exception(hr);
                }

                byte* rawBuffer;
                hr = bufferByteAccess->Buffer(&rawBuffer);
                if (FAILED(hr))
                {
                    throw ref new Exception(hr);
                }

                for (unsigned int i = 0; i < size; ++i)
                {
                    rawBuffer[i] = data[i];
                }

                InMemoryRandomAccessStream^ stream = ref new InMemoryRandomAccessStream();
                task<unsigned int> writeTask(stream->WriteAsync(buffer));
                writeTask.wait();
                assert(writeTask.get() == data.size());

                // Rewind stream
                stream->Seek(0);

                return stream;
            });
        }

        EGAHTTPApiResponse GAHTTPApi::processRequestResponse(Windows::Web::Http::HttpResponseMessage^ response, const std::string& requestId)
        {
            Windows::Web::Http::HttpStatusCode statusCode = response->StatusCode;

            // if no result - often no connection
            if (!response->IsSuccessStatusCode && std::wstring(response->Content->ToString()->Data()).empty())
            {
                logging::GALogger::d("%s request. failed. Might be no connection. Status code: %s", requestId.c_str(), utilities::GAUtilities::ws2s(statusCode.ToString()->Data()).c_str());
                return NoResponse;
            }

            // ok
            if (statusCode == Windows::Web::Http::HttpStatusCode::Ok)
            {
                return Ok;
            }
            if (statusCode == Windows::Web::Http::HttpStatusCode::Created)
            {
                return Created;
            }

            // 401 can return 0 status
            if (statusCode == (Windows::Web::Http::HttpStatusCode)0 || statusCode == Windows::Web::Http::HttpStatusCode::Unauthorized)
            {
                logging::GALogger::d("%s request. 401 - Unauthorized.", requestId.c_str());
                return Unauthorized;
            }

            if (statusCode == Windows::Web::Http::HttpStatusCode::BadRequest)
            {
                logging::GALogger::d("%s request. 400 - Bad Request.", requestId.c_str());
                return BadRequest;
            }

            if (statusCode == Windows::Web::Http::HttpStatusCode::InternalServerError)
            {
                logging::GALogger::d("%s request. 500 - Internal Server Error.", requestId.c_str());
                return InternalServerError;
            }

            logging::GALogger::d("%s request. statusCode=%s response=%s.", requestId.c_str(), utilities::GAUtilities::ws2s(statusCode.ToString()->Data()).c_str(), utilities::GAUtilities::ws2s(response->Content->ToString()->Data()).c_str());

            return UnknownResponseCode;
        }
    }
}

#endif
