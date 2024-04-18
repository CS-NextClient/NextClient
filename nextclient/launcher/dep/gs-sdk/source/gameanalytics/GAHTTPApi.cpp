//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#if !USE_UWP
#include "GAHTTPApi.h"
#include "GAState.h"
#include "GALogger.h"
#include "GAUtilities.h"
#include "GAValidator.h"
#include <future>
#include <utility>
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/error/en.h"
#include <string.h>
#include <stdio.h>
#if USE_TIZEN
#include <net_connection.h>
#endif
#include <array>

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

        void initResponseData(struct ResponseData *s)
        {
            s->len = 0;
            s->ptr = static_cast<char*>(malloc(s->len+1));
            if (s->ptr == NULL)
            {
                exit(EXIT_FAILURE);
            }
            s->ptr[0] = '\0';
        }

        size_t writefunc(void *ptr, size_t size, size_t nmemb, struct ResponseData *s)
        {
            size_t new_len = s->len + size*nmemb;
            s->ptr = static_cast<char*>(realloc(s->ptr, new_len+1));
            if (s->ptr == NULL)
            {
                exit(EXIT_FAILURE);
            }
            memcpy(s->ptr+s->len, ptr, size*nmemb);
            s->ptr[new_len] = '\0';
            s->len = new_len;

            return size*nmemb;
        }

        bool GAHTTPApi::_destroyed = false;
        GAHTTPApi* GAHTTPApi::_instance = 0;
        std::once_flag GAHTTPApi::_initInstanceFlag;

        // Constructor - setup the basic information for HTTP
        GAHTTPApi::GAHTTPApi()
        {
            curl_global_init(CURL_GLOBAL_DEFAULT);

            snprintf(GAHTTPApi::baseUrl, sizeof(GAHTTPApi::baseUrl), "%s://%s/%s", protocol, hostName, version);
            snprintf(GAHTTPApi::remoteConfigsBaseUrl, sizeof(GAHTTPApi::remoteConfigsBaseUrl), "%s://%s/remote_configs/%s", protocol, hostName, remoteConfigsVersion);
            // use gzip compression on JSON body
#if defined(_DEBUG)
            useGzip = false;
#else
            useGzip = true;
#endif
        }

        GAHTTPApi::~GAHTTPApi()
        {
            curl_global_cleanup();
        }

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

        void GAHTTPApi::requestInitReturningDict(EGAHTTPApiResponse& response_out, rapidjson::Document& json_out, const char* configsHash)
        {
            const char* gameKey = state::GAState::getGameKey();

            // Generate URL
            char url[513] = "";
            snprintf(url, sizeof(url), "%s/%s?game_key=%s&interval_seconds=0&configs_hash=%s", remoteConfigsBaseUrl, initializeUrlPath, gameKey, configsHash);

            logging::GALogger::d("Sending 'init' URL: %s", url);

            rapidjson::Document initAnnotations;
            initAnnotations.SetObject();
            state::GAState::getInitAnnotations(initAnnotations);

            // make JSON string from data
            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                initAnnotations.Accept(writer);
            }
            const char* JSONstring = buffer.GetString();

            if (strlen(JSONstring) == 0)
            {
                response_out = JsonEncodeFailed;
                json_out.SetNull();
                return;
            }

            std::vector<char> payloadData = createPayloadData(JSONstring, useGzip);

            CURL *curl;
            CURLcode res;
            curl = curl_easy_init();
            if(!curl)
            {
                response_out = NoResponse;
                json_out.SetNull();
                return;
            }

            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

            struct ResponseData s;
            initResponseData(&s);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
#if USE_TIZEN
            connection_h connection;
            int conn_err;
            conn_err = connection_create(&connection);
            if (conn_err != CONNECTION_ERROR_NONE)
            {
                response_out = NoResponse;
                json_out.SetNull();
                return;
            }
#endif

            std::vector<char> authorization = createRequest(curl, url, payloadData, useGzip);

            res = curl_easy_perform(curl);
            if(res != CURLE_OK)
            {
                logging::GALogger::d(curl_easy_strerror(res));
                response_out = NoResponse;
                json_out.SetNull();
                return;
            }

            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            curl_easy_cleanup(curl);

            // process the response
            logging::GALogger::d("init request content: %s, JSONString: %s", s.ptr, JSONstring);

            rapidjson::Document requestJsonDict;
            rapidjson::ParseResult ok = requestJsonDict.Parse(s.ptr);
            if(!ok)
            {
                logging::GALogger::d("requestInitReturningDict -- JSON error (offset %u): %s", (unsigned)ok.Offset(), GetParseError_En(ok.Code()));
                logging::GALogger::d("%s", s.ptr);
            }
            EGAHTTPApiResponse requestResponseEnum = processRequestResponse(response_code, s.ptr, "Init");
            free(s.ptr);

            // if not 200 result
            if (requestResponseEnum != Ok && requestResponseEnum != Created && requestResponseEnum != BadRequest)
            {
                logging::GALogger::d("Failed Init Call. URL: %s, JSONString: %s, Authorization: %s", url, JSONstring, authorization.data());
#if USE_TIZEN
                connection_destroy(connection);
#endif
                response_out = requestResponseEnum;
                json_out.SetNull();
                return;
            }

            if (requestJsonDict.IsNull())
            {
                logging::GALogger::d("Failed Init Call. Json decoding failed");
#if USE_TIZEN
                connection_destroy(connection);
#endif
                response_out = JsonDecodeFailed;
                json_out.SetNull();
                return;
            }

            // print reason if bad request
            if (requestResponseEnum == BadRequest)
            {
                rapidjson::StringBuffer buffer;
                rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
                requestJsonDict.Accept(writer);
                logging::GALogger::d("Failed Init Call. Bad request. Response: %s", buffer.GetString());
                // return bad request result
#if USE_TIZEN
                connection_destroy(connection);
#endif
                response_out = requestResponseEnum;
                json_out.SetNull();
                return;
            }

            // validate Init call values
            validators::GAValidator::validateAndCleanInitRequestResponse(requestJsonDict, json_out, requestResponseEnum == Created);

            if (json_out.IsNull())
            {
#if USE_TIZEN
                connection_destroy(connection);
#endif
                response_out = BadResponse;
                json_out.SetNull();
                return;
            }

#if USE_TIZEN
            connection_destroy(connection);
#endif

            // all ok
            response_out = requestResponseEnum;
        }

        void GAHTTPApi::sendEventsInArray(EGAHTTPApiResponse& response_out, rapidjson::Value& json_out, const rapidjson::Value& eventArray)
        {
            if (eventArray.Empty())
            {
                logging::GALogger::d("sendEventsInArray called with missing eventArray");
                return;
            }

            auto gameKey = state::GAState::getGameKey();

            // Generate URL
            char url[513] = "";
            snprintf(url, sizeof(url), "%s/%s/%s", baseUrl, gameKey, eventsUrlPath);

            logging::GALogger::d("Sending 'events' URL: %s", url);

            // make JSON string from data
            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                eventArray.Accept(writer);
            }

            const char* JSONstring = buffer.GetString();

            if (strlen(JSONstring) == 0)
            {
                logging::GALogger::d("sendEventsInArray JSON encoding failed of eventArray");
                response_out = JsonEncodeFailed;
                json_out.SetNull();;
                return;
            }

            std::vector<char> payloadData = createPayloadData(JSONstring, useGzip);

            CURL *curl;
            CURLcode res;
            curl = curl_easy_init();
            if(!curl)
            {
                response_out = NoResponse;
                json_out.SetNull();
                return;
            }

            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

            struct ResponseData s;
            initResponseData(&s);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
#if USE_TIZEN
            connection_h connection;
            int conn_err;
            conn_err = connection_create(&connection);
            if (conn_err != CONNECTION_ERROR_NONE)
            {
                response_out = NoResponse;
                json_out = rapidjson::Value();
                return;
            }
#endif
            std::vector<char> authorization = createRequest(curl, url, payloadData, useGzip);

            res = curl_easy_perform(curl);
            if(res != CURLE_OK)
            {
                logging::GALogger::d(curl_easy_strerror(res));
                response_out = NoResponse;
                json_out.SetNull();
                return;
            }

            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            curl_easy_cleanup(curl);

            logging::GALogger::d("body: %s", s.ptr);

            EGAHTTPApiResponse requestResponseEnum = processRequestResponse(response_code, s.ptr, "Events");

            // if not 200 result
            if (requestResponseEnum != Ok && requestResponseEnum != Created && requestResponseEnum != BadRequest)
            {
                logging::GALogger::d("Failed Events Call. URL: %s, JSONString: %s, Authorization: %s", url, JSONstring, authorization.data());
#if USE_TIZEN
                connection_destroy(connection);
#endif
                response_out = requestResponseEnum;
                json_out = rapidjson::Value();
            }

            // decode JSON
            rapidjson::Document requestJsonDict;
            rapidjson::ParseResult ok = requestJsonDict.Parse(s.ptr);
            if(!ok)
            {
                logging::GALogger::d("sendEventsInArray -- JSON error (offset %u): %s", (unsigned)ok.Offset(), GetParseError_En(ok.Code()));
                logging::GALogger::d("%s", s.ptr);
            }
            free(s.ptr);

            if (requestJsonDict.IsNull())
            {
#if USE_TIZEN
                connection_destroy(connection);
#endif
                response_out = JsonDecodeFailed;
                json_out = rapidjson::Value();
                return;
            }

            // print reason if bad request
            if (requestResponseEnum == BadRequest)
            {
                rapidjson::StringBuffer buffer;
                rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
                requestJsonDict.Accept(writer);

                logging::GALogger::d("Failed Events Call. Bad request. Response: %s", buffer.GetString());

                response_out = requestResponseEnum;
                json_out = rapidjson::Value();
                return;
            }

#if USE_TIZEN
            connection_destroy(connection);
#endif

            // return response
            response_out = requestResponseEnum;
            json_out.CopyFrom(requestJsonDict, requestJsonDict.GetAllocator());
        }

        void GAHTTPApi::sendSdkErrorEvent(EGASdkErrorCategory category, EGASdkErrorArea area, EGASdkErrorAction action, EGASdkErrorParameter parameter, const char* reason, const char* gameKey, const char* secretKey)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            // Validate
            if (!validators::GAValidator::validateSdkErrorEvent(gameKey, secretKey, category, area, action))
            {
                return;
            }

            // Generate URL

            std::array<char, 257> url = {'\0'};
            snprintf(url.data(), url.size(), "%s/%s/%s", baseUrl, gameKey, eventsUrlPath);

            logging::GALogger::d("Sending 'events' URL: %s", url);

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

            if(strlen(reason) > 0)
            {
                rapidjson::Value v(reason, json.GetAllocator());
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
            std::array<char, 10000> payloadJSONString = {'\0'};
            snprintf(payloadJSONString.data(), payloadJSONString.size(), "%s", buffer.GetString());

            if(strlen(payloadJSONString.data()) == 0)
            {
                logging::GALogger::w("sendSdkErrorEvent: JSON encoding failed.");
                return;
            }

            logging::GALogger::d("sendSdkErrorEvent json: %s", payloadJSONString.data());

            ErrorType errorType = std::make_tuple(category, area);

#if !NO_ASYNC
            bool useGzip = this->useGzip;

            std::async(std::launch::async, [url, payloadJSONString, useGzip, errorType]() -> void
            {
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

                std::vector<char> payloadData = GAHTTPApi::getInstance()->createPayloadData(payloadJSONString.data(), useGzip);

                CURL *curl;
                CURLcode res;
                curl = curl_easy_init();
                if(!curl)
                {
                    return;
                }

                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

                struct ResponseData s;
                initResponseData(&s);

                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
#if USE_TIZEN
                connection_h connection;
                int conn_err;
                conn_err = connection_create(&connection);
                if (conn_err != CONNECTION_ERROR_NONE)
                {
                    return;
                }
#endif
                GAHTTPApi::getInstance()->createRequest(curl, url.data(), payloadData, useGzip);

                res = curl_easy_perform(curl);
                if(res != CURLE_OK)
                {
                    logging::GALogger::d(curl_easy_strerror(res));
                    return;
                }

                long statusCode;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
                curl_easy_cleanup(curl);

                // process the response
                logging::GALogger::d("sdk error content : %s", s.ptr);
                free(s.ptr);

                // if not 200 result
                if (statusCode != 200)
                {
                    logging::GALogger::d("sdk error failed. response code not 200. status code: %u", CURLE_OK);
#if USE_TIZEN
                    connection_destroy(connection);
#endif
                    return;
                }

#if USE_TIZEN
                connection_destroy(connection);
#endif

                countMap[errorType] = countMap[errorType] + 1;
            });
#endif
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

                logging::GALogger::d("Gzip stats. Size: %lu, Compressed: %lu", strlen(payload), payloadData.size());
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

        std::vector<char> GAHTTPApi::createRequest(CURL *curl, const char* url, const std::vector<char>& payloadData, bool gzip)
        {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            struct curl_slist *header = NULL;

            if (gzip)
            {
                header = curl_slist_append(header, "Content-Encoding: gzip");
            }

            // create authorization hash
            const char* key = state::GAState::getGameSecret();

            char authorization[257] = "";
            utilities::GAUtilities::hmacWithKey(key, payloadData, authorization);
            char auth[129] = "";
            snprintf(auth, sizeof(auth), "Authorization: %s", authorization);
            header = curl_slist_append(header, auth);

            // always JSON
            header = curl_slist_append(header, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadData.data());
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payloadData.size());

            std::vector<char> result;
            size_t s = strlen(authorization);
            for(size_t i = 0; i < s; ++i)
            {
                result.push_back(authorization[i]);
            }
            result.push_back('\0');

            return result;
        }

        EGAHTTPApiResponse GAHTTPApi::processRequestResponse(long statusCode, const char* body, const char* requestId)
        {
            // if no result - often no connection
            if (utilities::GAUtilities::isStringNullOrEmpty(body))
            {
                logging::GALogger::d("%s request. failed. Might be no connection. Status code: %ld", requestId, statusCode);
                return NoResponse;
            }

            // ok
            if (statusCode == 200)
            {
                return Ok;
            }
            if (statusCode == 201)
            {
                return Created;
            }

            // 401 can return 0 status
            if (statusCode == 0 || statusCode == 401)
            {
                logging::GALogger::d("%s request. 401 - Unauthorized.", requestId);
                return Unauthorized;
            }

            if (statusCode == 400)
            {
                logging::GALogger::d("%s request. 400 - Bad Request.", requestId);
                return BadRequest;
            }

            if (statusCode == 500)
            {
                logging::GALogger::d("%s request. 500 - Internal Server Error.", requestId);
                return InternalServerError;
            }
            return UnknownResponseCode;
        }
    }
}
#endif
