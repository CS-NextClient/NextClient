//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#pragma once

#include <vector>
#include <map>
#include "rapidjson/document.h"
#if USE_UWP
#include <ppltasks.h>
#else
#include <curl/curl.h>
#endif
#include <mutex>
#include <cstdlib>
#include <tuple>

namespace gameanalytics
{
    namespace http
    {

        enum EGAHTTPApiResponse
        {
            // client
            NoResponse = 0,
            BadResponse = 1,
            RequestTimeout = 2, // 408
            JsonEncodeFailed = 3,
            JsonDecodeFailed = 4,
            // server
            InternalServerError = 5,
            BadRequest = 6, // 400
            Unauthorized = 7, // 401
            UnknownResponseCode = 8,
            Ok = 9,
            Created = 10
        };

        enum EGASdkErrorCategory
        {
            EventValidation = 1,
            Database = 2,
            Init = 3,
            Http = 4,
            Json = 5
        };

        enum EGASdkErrorArea
        {
            BusinessEvent = 1,
            ResourceEvent = 2,
            ProgressionEvent = 3,
            DesignEvent = 4,
            ErrorEvent = 5,
            InitHttp = 9,
            EventsHttp = 10,
            ProcessEvents = 11,
            AddEventsToStore = 12
        };

        enum EGASdkErrorAction
        {
            InvalidCurrency = 1,
            InvalidShortString = 2,
            InvalidEventPartLength = 3,
            InvalidEventPartCharacters = 4,
            InvalidStore = 5,
            InvalidFlowType = 6,
            StringEmptyOrNull = 7,
            NotFoundInAvailableCurrencies = 8,
            InvalidAmount = 9,
            NotFoundInAvailableItemTypes = 10,
            WrongProgressionOrder = 11,
            InvalidEventIdLength = 12,
            InvalidEventIdCharacters = 13,
            InvalidProgressionStatus = 15,
            InvalidSeverity = 16,
            InvalidLongString = 17,
            DatabaseTooLarge = 18,
            DatabaseOpenOrCreate = 19,
            JsonError = 25,
            FailHttpJsonDecode = 29,
            FailHttpJsonEncode = 30
        };

        enum EGASdkErrorParameter
        {
            Currency = 1,
            CartType = 2,
            ItemType = 3,
            ItemId = 4,
            Store = 5,
            FlowType = 6,
            Amount = 7,
            Progression01 = 8,
            Progression02 = 9,
            Progression03 = 10,
            EventId = 11,
            ProgressionStatus = 12,
            Severity = 13,
            Message = 14
        };

        struct ResponseData
        {
            char *ptr;
            size_t len;
        };

        typedef std::tuple<EGASdkErrorCategory, EGASdkErrorArea> ErrorType;

        class GAHTTPApi
        {
        public:

            static GAHTTPApi* getInstance();

#if USE_UWP
            concurrency::task<std::pair<EGAHTTPApiResponse, std::string>> requestInitReturningDict(const char* configsHash);
            concurrency::task<std::pair<EGAHTTPApiResponse, std::string>> sendEventsInArray(const rapidjson::Value& eventArray);
            void sendSdkErrorEvent(EGASdkErrorCategory category, EGASdkErrorArea area, EGASdkErrorAction action, EGASdkErrorParameter parameter, std::string reason, std::string gameKey, std::string secretKey);
#else
            void requestInitReturningDict(EGAHTTPApiResponse& response_out, rapidjson::Document& json_out, const char* configsHash);
            void sendEventsInArray(EGAHTTPApiResponse& response_out, rapidjson::Value& json_out, const rapidjson::Value& eventArray);
            void sendSdkErrorEvent(EGASdkErrorCategory category, EGASdkErrorArea area, EGASdkErrorAction action, EGASdkErrorParameter parameter, const char* reason, const char* gameKey, const char* secretKey);
#endif

            static void sdkErrorCategoryString(EGASdkErrorCategory value, char* out)
            {
                switch (value)
                {
                    case EventValidation:
                        snprintf(out, 40, "%s", "event_validation");
                        return;
                    case Database:
                        snprintf(out, 40, "%s", "db");
                        return;
                    case Init:
                        snprintf(out, 40, "%s", "init");
                        return;
                    case Http:
                        snprintf(out, 40, "%s", "http");
                        return;
                    case Json:
                        snprintf(out, 40, "%s", "json");
                        return;
                    default:
                        break;
                }
                snprintf(out, 40, "%s", "");
            }

            static void sdkErrorAreaString(EGASdkErrorArea value, char* out)
            {
                switch (value)
                {
                    case BusinessEvent:
                        snprintf(out, 40, "%s", "business");
                        return;
                    case ResourceEvent:
                        snprintf(out, 40, "%s", "resource");
                        return;
                    case ProgressionEvent:
                        snprintf(out, 40, "%s", "progression");
                        return;
                    case DesignEvent:
                        snprintf(out, 40, "%s", "design");
                        return;
                    case ErrorEvent:
                        snprintf(out, 40, "%s", "error");
                        return;
                    case InitHttp:
                        snprintf(out, 40, "%s", "init_http");
                        return;
                    case EventsHttp:
                        snprintf(out, 40, "%s", "events_http");
                        return;
                    case ProcessEvents:
                        snprintf(out, 40, "%s", "process_events");
                        return;
                    case AddEventsToStore:
                        snprintf(out, 40, "%s", "add_events_to_store");
                        return;
                    default:
                        break;
                }
                snprintf(out, 40, "%s", "");
            }

            static void sdkErrorActionString(EGASdkErrorAction value, char* out)
            {
                switch (value)
                {
                    case InvalidCurrency:
                        snprintf(out, 40, "%s", "invalid_currency");
                        return;
                    case InvalidShortString:
                        snprintf(out, 40, "%s", "invalid_short_string");
                        return;
                    case InvalidEventPartLength:
                        snprintf(out, 40, "%s", "invalid_event_part_length");
                        return;
                    case InvalidEventPartCharacters:
                        snprintf(out, 40, "%s", "invalid_event_part_characters");
                        return;
                    case InvalidStore:
                        snprintf(out, 40, "%s", "invalid_store");
                        return;
                    case InvalidFlowType:
                        snprintf(out, 40, "%s", "invalid_flow_type");
                        return;
                    case StringEmptyOrNull:
                        snprintf(out, 40, "%s", "string_empty_or_null");
                        return;
                    case NotFoundInAvailableCurrencies:
                        snprintf(out, 40, "%s", "not_found_in_available_currencies");
                        return;
                    case InvalidAmount:
                        snprintf(out, 40, "%s", "invalid_amount");
                        return;
                    case NotFoundInAvailableItemTypes:
                        snprintf(out, 40, "%s", "not_found_in_available_item_types");
                        return;
                    case WrongProgressionOrder:
                        snprintf(out, 40, "%s", "wrong_progression_order");
                        return;
                    case InvalidEventIdLength:
                        snprintf(out, 40, "%s", "invalid_event_id_length");
                        return;
                    case InvalidEventIdCharacters:
                        snprintf(out, 40, "%s", "invalid_event_id_characters");
                        return;
                    case InvalidProgressionStatus:
                        snprintf(out, 40, "%s", "invalid_progression_status");
                        return;
                    case InvalidSeverity:
                        snprintf(out, 40, "%s", "invalid_severity");
                        return;
                    case InvalidLongString:
                        snprintf(out, 40, "%s", "invalid_long_string");
                        return;
                    case DatabaseTooLarge:
                        snprintf(out, 40, "%s", "db_too_large");
                        return;
                    case DatabaseOpenOrCreate:
                        snprintf(out, 40, "%s", "db_open_or_create");
                        return;
                    case JsonError:
                        snprintf(out, 40, "%s", "json_error");
                        return;
                    case FailHttpJsonDecode:
                        snprintf(out, 40, "%s", "fail_http_json_decode");
                        return;
                    case FailHttpJsonEncode:
                        snprintf(out, 40, "%s", "fail_http_json_encode");
                        return;
                    default:
                        break;
                }
                snprintf(out, 40, "%s", "");
            }

            static void sdkErrorParameterString(EGASdkErrorParameter value, char* out)
            {
                switch (value)
                {
                    case Currency:
                        snprintf(out, 40, "%s", "currency");
                        return;
                    case CartType:
                        snprintf(out, 40, "%s", "cart_type");
                        return;
                    case ItemType:
                        snprintf(out, 40, "%s", "item_type");
                        return;
                    case ItemId:
                        snprintf(out, 40, "%s", "item_id");
                        return;
                    case Store:
                        snprintf(out, 40, "%s", "store");
                        return;
                    case FlowType:
                        snprintf(out, 40, "%s", "flow_type");
                        return;
                    case Amount:
                        snprintf(out, 40, "%s", "amount");
                        return;
                    case Progression01:
                        snprintf(out, 40, "%s", "progression01");
                        return;
                    case Progression02:
                        snprintf(out, 40, "%s", "progression02");
                        return;
                    case Progression03:
                        snprintf(out, 40, "%s", "progression03");
                        return;
                    case EventId:
                        snprintf(out, 40, "%s", "event_id");
                        return;
                    case ProgressionStatus:
                        snprintf(out, 40, "%s", "progression_status");
                        return;
                    case Severity:
                        snprintf(out, 40, "%s", "severity");
                        return;
                    case Message:
                        snprintf(out, 40, "%s", "message");
                        return;
                    default:
                        break;
                }
                snprintf(out, 40, "%s", "");
            }

        private:
            GAHTTPApi();
            ~GAHTTPApi();
            GAHTTPApi(const GAHTTPApi&) = delete;
            GAHTTPApi& operator=(const GAHTTPApi&) = delete;
            std::vector<char> createPayloadData(const char* payload, bool gzip);

#if USE_UWP
            std::vector<char> createRequest(Windows::Web::Http::HttpRequestMessage^ message, const std::string& url, const std::vector<char>& payloadData, bool gzip);
            EGAHTTPApiResponse processRequestResponse(Windows::Web::Http::HttpResponseMessage^ response, const std::string& requestId);
            concurrency::task<Windows::Storage::Streams::InMemoryRandomAccessStream^> createStream(std::string data);
#else
            std::vector<char> createRequest(CURL *curl, const char* url, const std::vector<char>& payloadData, bool gzip);
            EGAHTTPApiResponse processRequestResponse(long statusCode, const char* body, const char* requestId);
#endif
            static char protocol[];
            static char hostName[];
            static char version[];
            static char remoteConfigsVersion[];
            static char baseUrl[];
            static char remoteConfigsBaseUrl[];
            static char initializeUrlPath[];
            static char eventsUrlPath[];
            bool useGzip;
            static const int MaxCount;
            static std::map<ErrorType, int> countMap;
            static std::map<ErrorType, int64_t> timestampMap;

            static bool _destroyed;
            static GAHTTPApi* _instance;
            static std::once_flag _initInstanceFlag;
            static void cleanUp();

            static void initInstance()
            {
                if(!_destroyed && !_instance)
                {
                    _instance = new GAHTTPApi();
                    std::atexit(&cleanUp);
                }
            }
#if USE_UWP
            Windows::Web::Http::HttpClient^ httpClient;
#endif
        };

#if USE_UWP
        ref class GANetworkStatus sealed
        {
        internal:
            static void NetworkInformationOnNetworkStatusChanged(Platform::Object^ sender);
            static void CheckInternetAccess();
            static bool hasInternetAccess;
        };
#endif
    }
}
