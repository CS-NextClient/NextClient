//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#pragma once

#include <vector>
#include <map>
#include <functional>
#include "rapidjson/document.h"
#include "GameAnalytics.h"
#include <mutex>
#include <cstdlib>

namespace gameanalytics
{
    namespace state
    {
        // TODO(nikolaj): needed? remove.. if not
        // typedef void(*Callback) ();

        struct CStringCmp
        {
            bool operator()(const char* first, const char* second) const
            {
                return strcmp(first, second) < 0;
            }
        };

        struct ProgressionTry
        {
        public:
            char progression[257] = "";
            int tries = 0;
        };

        struct ProgressionTries
        {
        public:
            void addOrUpdate(const char* s, int tries)
            {
                for (auto & element : v)
                {
                    if(strcmp(s, element.progression) == 0)
                    {
                        element.tries = tries;
                        return;
                    }
                }

                v.push_back(ProgressionTry());
                snprintf(v[v.size() - 1].progression, sizeof(v[v.size() - 1].progression), "%s", s);
                v[v.size() - 1].tries = tries;
            }

            void remove(const char* s)
            {
                int index = 0;
                bool found = false;
                for (auto & element : v)
                {
                    if(strcmp(s, element.progression) == 0)
                    {
                        found = true;
                        break;
                    }

                    ++index;
                }

                if(found)
                {
                    v.erase(v.begin() + index);
                }
            }

            int getTries(const char* s) const
            {
                int result = 0;
                for (auto & element : v)
                {
                    if(strcmp(s, element.progression) == 0)
                    {
                        result = element.tries;
                        break;
                    }
                }
                return result;
            }

        private:
            std::vector<ProgressionTry> v;
        };

        class GAState
        {
        public:
            static GAState* getInstance();
            static bool isDestroyed();
            static void setUserId(const char* id);
            static bool isInitialized();
            static int64_t getSessionStart();
            static int getSessionNum();
            static int getTransactionNum();
            static const char* getSessionId();
            static const char* getCurrentCustomDimension01();
            static const char* getCurrentCustomDimension02();
            static const char* getCurrentCustomDimension03();
            static void getGlobalCustomEventFields(rapidjson::Document& out);
            static const char* getGameKey();
            static const char* getGameSecret();
            static void setAvailableCustomDimensions01(const StringVector& dimensions);
            static void setAvailableCustomDimensions02(const StringVector& dimensions);
            static void setAvailableCustomDimensions03(const StringVector& dimensions);
            static void setAvailableResourceCurrencies(const StringVector& availableResourceCurrencies);
            static void setAvailableResourceItemTypes(const StringVector& availableResourceItemTypes);
            static void setBuild(const char* build);
            static bool isEnabled();
            static void setCustomDimension01(const char* dimension);
            static void setCustomDimension02(const char* dimension);
            static void setCustomDimension03(const char* dimension);
            static void setGlobalCustomEventFields(const char *customFields);
            static void incrementSessionNum();
            static void incrementTransactionNum();
            static void incrementProgressionTries(const char* progression);
            static int getProgressionTries(const char* progression);
            static void clearProgressionTries(const char* progression);
            static bool hasAvailableCustomDimensions01(const char* dimension1);
            static bool hasAvailableCustomDimensions02(const char* dimension2);
            static bool hasAvailableCustomDimensions03(const char* dimension3);
            static bool hasAvailableResourceCurrency(const char* currency);
            static bool hasAvailableResourceItemType(const char* itemType);
            static void setKeys(const char* gameKey, const char* gameSecret);
            static void endSessionAndStopQueue(bool endThread);
            static void resumeSessionAndStartQueue();
            static void getEventAnnotations(rapidjson::Document& out);
            static void getSdkErrorEventAnnotations(rapidjson::Document& out);
            static void getInitAnnotations(rapidjson::Document& out);
            static void internalInitialize();
            static int64_t getClientTsAdjusted();
            static void setManualSessionHandling(bool flag);
            static bool useManualSessionHandling();
            static void setEnableErrorReporting(bool flag);
            static bool useErrorReporting();
            static void setEnabledEventSubmission(bool flag);
            static bool isEventSubmissionEnabled();
            static bool sessionIsStarted();
            static void validateAndCleanCustomFields(const rapidjson::Value& fields, rapidjson::Value& out);
            static std::vector<char> getRemoteConfigsStringValue(const char* key, const char* defaultValue);
            static bool isRemoteConfigsReady();
            static void addRemoteConfigsListener(const std::shared_ptr<IRemoteConfigsListener>& listener);
            static void removeRemoteConfigsListener(const std::shared_ptr<IRemoteConfigsListener>& listener);
            static std::vector<char> getRemoteConfigsContentAsString();
            static std::vector<char> getAbId();
            static std::vector<char> getAbVariantId();

        private:
            GAState();
            ~GAState();
            GAState(const GAState&) = delete;
            GAState& operator=(const GAState&) = delete;

            static const char* getIdentifier();
            static void setDefaultUserId(const char* id);
            static void getSdkConfig(rapidjson::Value& out);
            static void cacheIdentifier();
            static void ensurePersistedStates();
            static void startNewSession();
            static void validateAndFixCurrentDimensions();
            static const char* getBuild();
            static int64_t calculateServerTimeOffset(int64_t serverTs);
            static void populateConfigurations(rapidjson::Value& sdkConfig);
            static void setConfigsHash(const char* configsHash);
            static void setAbId(const char* abId);
            static void setAbVariantId(const char* abVariantId);

            static bool _destroyed;
            static GAState* _instance;
            static std::once_flag _initInstanceFlag;
            static void cleanUp();

            static void addErrorEvent(const char* baseMessage, EGAErrorSeverity severity, const char* message);

            static void initInstance()
            {
                if(!_destroyed && !_instance)
                {
                    _instance = new GAState();
                    std::atexit(&cleanUp);
                }
            }

            char _userId[129] = {'\0'};
            char _identifier[129] = {'\0'};
            bool _initialized = false;
            int64_t _sessionStart = 0;
            int _sessionNum = 0;
            int _transactionNum = 0;
            char _sessionId[65] = {'\0'};
            char _currentCustomDimension01[65] = {'\0'};
            char _currentCustomDimension02[65] = {'\0'};
            char _currentCustomDimension03[65] = {'\0'};
            rapidjson::Document _currentGlobalCustomEventFields;
            char _gameKey[65] = {'\0'};
            char _gameSecret[65] = {'\0'};
            StringVector _availableCustomDimensions01;
            StringVector _availableCustomDimensions02;
            StringVector _availableCustomDimensions03;
            StringVector _availableResourceCurrencies;
            StringVector _availableResourceItemTypes;
            char _build[65] = {'\0'};
            bool _initAuthorized = false;
            bool _enabled = false;
            int64_t _clientServerTimeOffset = 0;
            char _defaultUserId[129] = {'\0'};
            char _configsHash[129] = {'\0'};
            char _abId[129] = {'\0'};
            char _abVariantId[129] = {'\0'};
            ProgressionTries _progressionTries;
            rapidjson::Document _sdkConfigDefault;
            rapidjson::Document _sdkConfig;
            rapidjson::Document _sdkConfigCached;
            static const char* CategorySdkError;
            bool _useManualSessionHandling = false;
            bool _enableErrorReporting = true;
            bool _enableEventSubmission = true;
            rapidjson::Document _configurations;
            bool _remoteConfigsIsReady;
            std::vector<std::shared_ptr<IRemoteConfigsListener>> _remoteConfigsListeners;
            std::mutex _mtx;

            static const int MaxCount;
            static rapidjson::Document countMap;
            static rapidjson::Document timestampMap;
        };
    }
}
