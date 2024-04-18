//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#include "GAState.h"
#include "GAEvents.h"
#include "GAStore.h"
#include "GAUtilities.h"
#include "GAValidator.h"
#include "GAHTTPApi.h"
#include "GAThreading.h"
#include "GALogger.h"
#include "GADevice.h"
#include "GAThreading.h"
#include <utility>
#include <algorithm>
#include <array>
#include <climits>
#include <string.h>
#include <stdio.h>
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#define MAX_CUSTOM_FIELDS_COUNT 50
#define MAX_CUSTOM_FIELDS_KEY_LENGTH 64
#define MAX_CUSTOM_FIELDS_VALUE_STRING_LENGTH 256

namespace gameanalytics
{
    namespace state
    {
        const char* GAState::CategorySdkError = "sdk_error";

        bool GAState::_destroyed = false;
        GAState* GAState::_instance = 0;
        std::once_flag GAState::_initInstanceFlag;

        const int GAState::MaxCount = 10;
        rapidjson::Document GAState::countMap;
        rapidjson::Document GAState::timestampMap;

        GAState::GAState()
        {
        }

        GAState::~GAState()
        {
        }

        void GAState::cleanUp()
        {
            delete _instance;
            _instance = 0;
            _destroyed = true;
            threading::GAThreading::endThread();
        }

        GAState* GAState::getInstance()
        {
            std::call_once(_initInstanceFlag, &GAState::initInstance);
            return _instance;
        }

        bool GAState::isDestroyed()
        {
            return _destroyed;
        }

        void GAState::setUserId(const char* id)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            snprintf(i->_userId, sizeof(i->_userId), "%s", id);
            cacheIdentifier();
        }

        const char* GAState::getIdentifier()
        {
            return getInstance()->_identifier;
        }

        bool GAState::isInitialized()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return i->_initialized;
        }

        int64_t GAState::getSessionStart()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return 0;
            }
            return i->_sessionStart;
        }

        int GAState::getSessionNum()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return 0;
            }
            return i->_sessionNum;
        }

        int GAState::getTransactionNum()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return 0;
            }
            return i->_transactionNum;
        }

        const char* GAState::getSessionId()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return NULL;
            }
            return i->_sessionId;
        }

        const char* GAState::getCurrentCustomDimension01()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return NULL;
            }
            return i->_currentCustomDimension01;
        }

        const char* GAState::getCurrentCustomDimension02()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return NULL;
            }
            return i->_currentCustomDimension02;
        }

        const char* GAState::getCurrentCustomDimension03()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return NULL;
            }
            return i->_currentCustomDimension03;
        }

        void GAState::getGlobalCustomEventFields(rapidjson::Document& out)
        {
            GAState *i = getInstance();
            if (!i)
            {
                return;
            }

            out.CopyFrom(i->_currentGlobalCustomEventFields, out.GetAllocator());
        }

        void GAState::setAvailableCustomDimensions01(const StringVector& availableCustomDimensions)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            // Validate
            if (!validators::GAValidator::validateCustomDimensions(availableCustomDimensions))
            {
                return;
            }
            i->_availableCustomDimensions01 = availableCustomDimensions;

            // validate current dimension values
            validateAndFixCurrentDimensions();

            utilities::GAUtilities::printJoinStringArray(availableCustomDimensions, "Set available custom01 dimension values: (%s)");
        }

        void GAState::setAvailableCustomDimensions02(const StringVector& availableCustomDimensions)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            // Validate
            if (!validators::GAValidator::validateCustomDimensions(availableCustomDimensions))
            {
                return;
            }
            i->_availableCustomDimensions02 = availableCustomDimensions;

            // validate current dimension values
            validateAndFixCurrentDimensions();

            utilities::GAUtilities::printJoinStringArray(availableCustomDimensions, "Set available custom02 dimension values: (%s)");
        }

        void GAState::setAvailableCustomDimensions03(const StringVector& availableCustomDimensions)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            // Validate
            if (!validators::GAValidator::validateCustomDimensions(availableCustomDimensions))
            {
                return;
            }
            i->_availableCustomDimensions03 = availableCustomDimensions;

            // validate current dimension values
            validateAndFixCurrentDimensions();

            utilities::GAUtilities::printJoinStringArray(availableCustomDimensions, "Set available custom03 dimension values: (%s)");
        }

        void GAState::setAvailableResourceCurrencies(const StringVector& availableResourceCurrencies)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            // Validate
            if (!validators::GAValidator::validateResourceCurrencies(availableResourceCurrencies)) {
                return;
            }
            i->_availableResourceCurrencies = availableResourceCurrencies;

            utilities::GAUtilities::printJoinStringArray(availableResourceCurrencies, "Set available resource currencies: (%s)");
        }

        void GAState::setAvailableResourceItemTypes(const StringVector& availableResourceItemTypes)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            // Validate
            if (!validators::GAValidator::validateResourceItemTypes(availableResourceItemTypes)) {
                return;
            }
            i->_availableResourceItemTypes = availableResourceItemTypes;

            utilities::GAUtilities::printJoinStringArray(availableResourceItemTypes, "Set available resource item types: (%s)");
        }

        void GAState::setBuild(const char* build)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            snprintf(i->_build, sizeof(i->_build), "%s", build);

            logging::GALogger::i("Set build: %s", build);
        }

        void GAState::setDefaultUserId(const char* id)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            snprintf(i->_defaultUserId, sizeof(i->_defaultUserId), "%s", id);
            cacheIdentifier();
        }

        void GAState::getSdkConfig(rapidjson::Value& out)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            if (i->_sdkConfig.IsObject())
            {
                out.CopyFrom(i->_sdkConfig, i->_sdkConfig.GetAllocator());
                return;
            }
            else if (i->_sdkConfigCached.IsObject())
            {
                out.CopyFrom(i->_sdkConfigCached, i->_sdkConfigCached.GetAllocator());
                return;
            }

            if(i->_sdkConfigDefault.IsNull())
            {
                i->_sdkConfigDefault.SetObject();
            }

            out.CopyFrom(i->_sdkConfigDefault, i->_sdkConfigDefault.GetAllocator());
        }

        bool GAState::isEnabled()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return i->_enabled;
        }

        void GAState::setCustomDimension01(const char* dimension)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            snprintf(i->_currentCustomDimension01, sizeof(i->_currentCustomDimension01), "%s", dimension);
            if (store::GAStore::getTableReady())
            {
                store::GAStore::setState("dimension01", dimension);
            }
            logging::GALogger::i("Set custom01 dimension value: %s", dimension);
        }

        void GAState::setCustomDimension02(const char* dimension)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            snprintf(i->_currentCustomDimension02, sizeof(i->_currentCustomDimension02), "%s", dimension);
            if (store::GAStore::getTableReady())
            {
                store::GAStore::setState("dimension02", dimension);
            }
            logging::GALogger::i("Set custom02 dimension value: %s", dimension);
        }

        void GAState::setCustomDimension03(const char* dimension)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            snprintf(i->_currentCustomDimension03, sizeof(i->_currentCustomDimension03), "%s", dimension);
            if (store::GAStore::getTableReady())
            {
                store::GAStore::setState("dimension03", dimension);
            }
            logging::GALogger::i("Set custom03 dimension value: %s", dimension);
        }

        void GAState::setGlobalCustomEventFields(const char *customFields)
        {
            GAState *i = getInstance();
            if (!i)
            {
                return;
            }

            if (!customFields || strlen(customFields) == 0)
            {
                rapidjson::Document d;
                d.SetObject();
                i->_currentGlobalCustomEventFields.CopyFrom(d, i->_currentGlobalCustomEventFields.GetAllocator());
                return;
            }

            rapidjson::Document d;
            d.Parse(customFields);
            if(!d.IsNull())
            {
                i->_currentGlobalCustomEventFields.CopyFrom(d, i->_currentGlobalCustomEventFields.GetAllocator());
            }
            else
            {
                d.SetObject();
                i->_currentGlobalCustomEventFields.CopyFrom(d, i->_currentGlobalCustomEventFields.GetAllocator());
            }

            logging::GALogger::i("Set global custom event fields: %s", customFields);
        }

        void GAState::incrementSessionNum()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            auto sessionNumInt = getSessionNum() + 1;
            i->_sessionNum = sessionNumInt;
        }

        void GAState::incrementTransactionNum()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            auto transactionNumInt = getTransactionNum() + 1;
            i->_transactionNum = transactionNumInt;
        }

        void GAState::incrementProgressionTries(const char* progression)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            auto tries = static_cast<int>(getProgressionTries(progression) + 1);
            char key[257] = "";
            snprintf(key, sizeof(key), "%s", progression);
            i->_progressionTries.addOrUpdate(key, tries);

            // Persist
            char triesString[11] = "";
            snprintf(triesString, sizeof(triesString), "%d", tries);
            const char* parms[2] = {progression, triesString};
            store::GAStore::executeQuerySync("INSERT OR REPLACE INTO ga_progression (progression, tries) VALUES(?, ?);", parms, 2);
        }

        int GAState::getProgressionTries(const char* progression)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return 0;
            }

            return i->_progressionTries.getTries(progression);
        }

        void GAState::clearProgressionTries(const char* progression)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            i->_progressionTries.remove(progression);

            // Delete
            const char* parms[1] = {progression};
            store::GAStore::executeQuerySync("DELETE FROM ga_progression WHERE progression = ?;", parms, 1);
        }

        bool GAState::hasAvailableCustomDimensions01(const char* dimension1)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return utilities::GAUtilities::stringVectorContainsString(i->_availableCustomDimensions01, dimension1);
        }

        bool GAState::hasAvailableCustomDimensions02(const char* dimension2)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return utilities::GAUtilities::stringVectorContainsString(i->_availableCustomDimensions02, dimension2);
        }

        bool GAState::hasAvailableCustomDimensions03(const char* dimension3)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return utilities::GAUtilities::stringVectorContainsString(i->_availableCustomDimensions03, dimension3);
        }

        bool GAState::hasAvailableResourceCurrency(const char* currency)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return utilities::GAUtilities::stringVectorContainsString(i->_availableResourceCurrencies, currency);
        }

        bool GAState::hasAvailableResourceItemType(const char* itemType)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return utilities::GAUtilities::stringVectorContainsString(i->_availableResourceItemTypes, itemType);
        }

        void GAState::setKeys(const char* gameKey, const char* gameSecret)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            snprintf(i->_gameKey, sizeof(i->_gameKey), "%s", gameKey);
            snprintf(i->_gameSecret, sizeof(i->_gameSecret), "%s", gameSecret);
        }

        const char* GAState::getGameKey()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return NULL;
            }
            return i->_gameKey;
        }

        const char* GAState::getGameSecret()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return NULL;
            }
            return i->_gameSecret;
        }

        void GAState::internalInitialize()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            // Make sure database is ready
            if (!store::GAStore::getTableReady())
            {
                return;
            }

            // Make sure persisted states are loaded
            ensurePersistedStates();
            store::GAStore::setState("default_user_id", i->_defaultUserId);
            i->_initialized = true;

            startNewSession();

            if (isEnabled())
            {
                events::GAEvents::ensureEventQueueIsRunning();
            }
        }

        void GAState::resumeSessionAndStartQueue()
        {
            if(!GAState::isInitialized())
            {
                return;
            }
            logging::GALogger::i("Resuming session.");
            if(!GAState::sessionIsStarted())
            {
                startNewSession();
            }
            events::GAEvents::ensureEventQueueIsRunning();
        }

        void GAState::endSessionAndStopQueue(bool endThread)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            if(GAState::isInitialized())
            {
                logging::GALogger::i("Ending session.");
                events::GAEvents::stopEventQueue();
                if (GAState::isEnabled() && GAState::sessionIsStarted())
                {
                    events::GAEvents::addSessionEndEvent();
                    i->_sessionStart = 0;
                }
            }

            if(endThread)
            {
                threading::GAThreading::endThread();
            }
        }

        void GAState::getEventAnnotations(rapidjson::Document& out)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            out.SetObject();
            rapidjson::Document::AllocatorType& allocator = out.GetAllocator();

            // ---- REQUIRED ---- //

            // collector event API version
            out.AddMember("v", 2, allocator);

            // Event UUID
            {
                char id[129] = "";
                utilities::GAUtilities::generateUUID(id);
                rapidjson::Value v(id, allocator);
                out.AddMember("event_uuid", v.Move(), allocator);
            }

            // User identifier
            {
                rapidjson::Value v(getIdentifier(), allocator);
                out.AddMember("user_id", v.Move(), allocator);
            }

            // remote configs configurations
            if(i->_configurations.IsObject() && i->_configurations.MemberCount() > 0)
            {
                rapidjson::Value v(rapidjson::kObjectType);
                v.CopyFrom(i->_configurations, i->_configurations.GetAllocator());
                out.AddMember("configurations", v.Move(), allocator);
            }

            // A/B testing
            if (strlen(i->_abId) > 0)
            {
                rapidjson::Value v(i->_abId, allocator);
                out.AddMember("ab_id", v.Move(), allocator);
            }
            if (strlen(i->_abVariantId) > 0)
            {
                rapidjson::Value v(i->_abVariantId, allocator);
                out.AddMember("ab_variant_id", v.Move(), allocator);
            }

            // Client Timestamp (the adjusted timestamp)
            out.AddMember("client_ts", GAState::getClientTsAdjusted(), allocator);
            // SDK version
            {
                rapidjson::Value v(device::GADevice::getRelevantSdkVersion(), allocator);
                out.AddMember("sdk_version", v.Move(), allocator);
            }
            // Operation system version
            {
                rapidjson::Value v(device::GADevice::getOSVersion(), allocator);
                out.AddMember("os_version", v.Move(), allocator);
            }
            // Device make (hardcoded to apple)
            {
                rapidjson::Value v(device::GADevice::getDeviceManufacturer(), allocator);
                out.AddMember("manufacturer", v.Move(), allocator);
            }
            // Device version
            {
                rapidjson::Value v(device::GADevice::getDeviceModel(), allocator);
                out.AddMember("device", v.Move(), allocator);
            }
            // Platform (operating system)
            {
                rapidjson::Value v(device::GADevice::getBuildPlatform(), allocator);
                out.AddMember("platform", v.Move(), allocator);
            }
            // Session identifier
            {
                rapidjson::Value v(i->_sessionId, allocator);
                out.AddMember("session_id", v.Move(), allocator);
            }
            // Session number
            out.AddMember("session_num", getSessionNum(), allocator);

            // type of connection the user is currently on (add if valid)
            const char* connection_type = device::GADevice::getConnectionType();
            if (validators::GAValidator::validateConnectionType(connection_type))
            {
                rapidjson::Value v(connection_type, allocator);
                out.AddMember("connection_type", v.Move(), allocator);
            }

            if(strlen(device::GADevice::getGameEngineVersion()) > 0)
            {
                rapidjson::Value v(device::GADevice::getGameEngineVersion(), allocator);
                out.AddMember("engine_version", v.Move(), allocator);
            }

#if USE_UWP
            if (strlen(device::GADevice::getAdvertisingId()) > 0)
            {
                rapidjson::Value v(device::GADevice::getAdvertisingId(), allocator);
                out.AddMember("uwp_aid", v.Move(), allocator);
            }
            else if (strlen(device::GADevice::getDeviceId()) > 0)
            {
                rapidjson::Value v(device::GADevice::getDeviceId(), allocator);
                out.AddMember("uwp_id", v.Move(), allocator);
            }
#elif USE_TIZEN
            if (strlen(device::GADevice::getDeviceId()) > 0)
            {
                rapidjson::Value v(device::GADevice::getDeviceId(), allocator);
                out.AddMember("tizen_id", v.Move(), allocator);
            }
#endif

            // ---- CONDITIONAL ---- //

            // App build version (use if not nil)
            if (strlen(getBuild()) > 0)
            {
                rapidjson::Value v(getBuild(), allocator);
                out.AddMember("build", v.Move(), allocator);
            }
        }

        void GAState::getSdkErrorEventAnnotations(rapidjson::Document& out)
        {
            out.SetObject();
            rapidjson::Document::AllocatorType& allocator = out.GetAllocator();

            // ---- REQUIRED ---- //

            // collector event API version
            out.AddMember("v", 2, allocator);

            // Event UUID
            {
                char id[129] = "";
                utilities::GAUtilities::generateUUID(id);
                rapidjson::Value v(id, allocator);
                out.AddMember("event_uuid", v.Move(), allocator);
            }

            // Category
            {
                rapidjson::Value v(GAState::CategorySdkError, allocator);
                out.AddMember("category", v.Move(), allocator);
            }
            // SDK version
            {
                rapidjson::Value v(device::GADevice::getRelevantSdkVersion(), allocator);
                out.AddMember("sdk_version", v.Move(), allocator);
            }
            // Operation system version
            {
                rapidjson::Value v(device::GADevice::getOSVersion(), allocator);
                out.AddMember("os_version", v.Move(), allocator);
            }
            // Device make (hardcoded to apple)
            {
                rapidjson::Value v(device::GADevice::getDeviceManufacturer(), allocator);
                out.AddMember("manufacturer", v.Move(), allocator);
            }
            // Device version
            {
                rapidjson::Value v(device::GADevice::getDeviceModel(), allocator);
                out.AddMember("device", v.Move(), allocator);
            }
            // Platform (operating system)
            {
                rapidjson::Value v(device::GADevice::getBuildPlatform(), allocator);
                out.AddMember("platform", v.Move(), allocator);
            }

            // type of connection the user is currently on (add if valid)
            const char* connection_type = device::GADevice::getConnectionType();
            if (validators::GAValidator::validateConnectionType(connection_type))
            {
                rapidjson::Value v(connection_type, allocator);
                out.AddMember("connection_type", v.Move(), allocator);
            }

            if(strlen(device::GADevice::getGameEngineVersion()) > 0)
            {
                rapidjson::Value v(device::GADevice::getGameEngineVersion(), allocator);
                out.AddMember("engine_version", v.Move(), allocator);
            }
        }

        void GAState::getInitAnnotations(rapidjson::Document& out)
        {
            out.SetObject();
            rapidjson::Document::AllocatorType& allocator = out.GetAllocator();

            {
                if(strlen(getIdentifier()) == 0)
                {
                    cacheIdentifier();
                }
                rapidjson::Value v(getIdentifier(), allocator);
                store::GAStore::setState("last_used_identifier", getIdentifier());
                out.AddMember("user_id", v.Move(), allocator);
            }

            // SDK version
            {
                rapidjson::Value v(device::GADevice::getRelevantSdkVersion(), allocator);
                out.AddMember("sdk_version", v.Move(), allocator);
            }
            // Operation system version
            {
                rapidjson::Value v(device::GADevice::getOSVersion(), allocator);
                out.AddMember("os_version", v.Move(), allocator);
            }

            // Platform (operating system)
            {
                rapidjson::Value v(device::GADevice::getBuildPlatform(), allocator);
                out.AddMember("platform", v.Move(), allocator);
            }

            // Build
            if (strlen(getBuild()) > 0)
            {
                rapidjson::Value v(getBuild(), allocator);
                out.AddMember("build", v.Move(), allocator);
            }
            else
            {
                rapidjson::Value v;
                out.AddMember("build", v.Move(), allocator);
            }

            out.AddMember("session_num", getSessionNum(), allocator);

            // Random salt
            out.AddMember("random_salt", getSessionNum(), allocator);
        }

        void GAState::cacheIdentifier()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            if (strlen(i->_userId) > 0)
            {
                snprintf(i->_identifier, sizeof(i->_identifier), "%s", i->_userId);
            }
#if USE_UWP
            else if (strlen(device::GADevice::getAdvertisingId()) > 0)
            {
                snprintf(i->_identifier, sizeof(i->_identifier), "%s", device::GADevice::getAdvertisingId());
            }
            else if (strlen(device::GADevice::getDeviceId()) > 0)
            {
                snprintf(i->_identifier, sizeof(i->_identifier), "%s", device::GADevice::getDeviceId());
            }
#elif USE_TIZEN
            else if (strlen(device::GADevice::getDeviceId()) > 0)
            {
                snprintf(i->_identifier, sizeof(i->_identifier), "%s", device::GADevice::getDeviceId());
            }
#endif
            else if (strlen(i->_defaultUserId) > 0)
            {
                snprintf(i->_identifier, sizeof(i->_identifier), "%s", i->_defaultUserId);
            }

            logging::GALogger::d("identifier, {clean:%s}", i->_identifier);
        }

        void GAState::ensurePersistedStates()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            // get and extract stored states
            rapidjson::Document state_dict;
            state_dict.SetObject();
            rapidjson::Document::AllocatorType& allocator = state_dict.GetAllocator();
            rapidjson::Document results_ga_state;
            store::GAStore::executeQuerySync("SELECT * FROM ga_state;", results_ga_state);

            if (!results_ga_state.IsNull() && !results_ga_state.Empty())
            {
                for (rapidjson::Value::ConstValueIterator itr = results_ga_state.Begin(); itr != results_ga_state.End(); ++itr)
                {
                    if(itr->HasMember("key") && itr->HasMember("value"))
                    {
                        rapidjson::Value v((*itr)["key"].GetString(), allocator);
                        rapidjson::Value v1((*itr)["value"].GetString(), allocator);
                        state_dict.AddMember(v.Move(), v1.Move(), allocator);
                    }
                }
            }

            // insert into GAState instance

            const char* defaultId = state_dict.HasMember("default_user_id") ? state_dict["default_user_id"].GetString() : "";
            if(strlen(defaultId) == 0)
            {
                char id[129] = "";
                utilities::GAUtilities::generateUUID(id);
                i->setDefaultUserId(id);
            }
            else
            {
                i->setDefaultUserId(defaultId);
            }

            i->_sessionNum = (int)strtol(state_dict.HasMember("session_num") ? state_dict["session_num"].GetString() : "0", NULL, 10);

            i->_transactionNum = (int)strtol(state_dict.HasMember("transaction_num") ? state_dict["transaction_num"].GetString() : "0", NULL, 10);

            // restore dimension settings
            if (strlen(i->_currentCustomDimension01) > 0)
            {
                store::GAStore::setState("dimension01", i->_currentCustomDimension01);
            }
            else
            {
                snprintf(i->_currentCustomDimension01, sizeof(i->_currentCustomDimension01), "%s", state_dict.HasMember("dimension01") ? state_dict["dimension01"].GetString() : "");
                if (strlen(i->_currentCustomDimension01))
                {
                    logging::GALogger::d("Dimension01 found in cache: %s", i->_currentCustomDimension01);
                }
            }

            if (strlen(i->_currentCustomDimension02) > 0)
            {
                store::GAStore::setState("dimension02", i->_currentCustomDimension02);
            }
            else
            {
                snprintf(i->_currentCustomDimension02, sizeof(i->_currentCustomDimension02), "%s", state_dict.HasMember("dimension02") ? state_dict["dimension02"].GetString() : "");
                if (strlen(i->_currentCustomDimension02) > 0)
                {
                    logging::GALogger::d("Dimension02 found in cache: %s", i->_currentCustomDimension02);
                }
            }

            if (strlen(i->_currentCustomDimension03) > 0)
            {
                store::GAStore::setState("dimension03", i->_currentCustomDimension03);
            }
            else
            {
                snprintf(i->_currentCustomDimension03, sizeof(i->_currentCustomDimension03), "%s", state_dict.HasMember("dimension03") ? state_dict["dimension03"].GetString() : "");
                if (strlen(i->_currentCustomDimension03) > 0)
                {
                    logging::GALogger::d("Dimension03 found in cache: %s", i->_currentCustomDimension03);
                }
            }

            // get cached init call values
            const char* sdkConfigCachedString = state_dict.HasMember("sdk_config_cached") ? state_dict["sdk_config_cached"].GetString() : "";
            if (strlen(sdkConfigCachedString) > 0)
            {
                // decode JSON
                rapidjson::Document d;
                d.Parse(sdkConfigCachedString);
                if (!d.IsNull())
                {
                    const char *lastUsedIdentifier = state_dict.HasMember("last_used_identifier") ? state_dict["last_used_identifier"].GetString() : "";
                    if (strlen(lastUsedIdentifier) > 0 && strcmp(lastUsedIdentifier, getIdentifier()) != 0)
                    {
                        if (d.HasMember("configs_hash"))
                        {
                            d.RemoveMember("configs_hash");
                        }
                    }
                    i->_sdkConfigCached.CopyFrom(d, i->_sdkConfigCached.GetAllocator());
                }
            }

            {
                rapidjson::Value currentSdkConfig(rapidjson::kObjectType);
                GAState::getSdkConfig(currentSdkConfig);
                GAState::setConfigsHash(currentSdkConfig.HasMember("configs_hash") && currentSdkConfig["configs_hash"].IsString() ? currentSdkConfig["configs_hash"].GetString() : "");
                GAState::setAbId(currentSdkConfig.HasMember("ab_id") && currentSdkConfig["ab_id"].IsString() ? currentSdkConfig["ab_id"].GetString() : "");
                GAState::setAbVariantId(currentSdkConfig.HasMember("ab_variant_id") && currentSdkConfig["ab_variant_id"].IsString() ? currentSdkConfig["ab_variant_id"].GetString() : "");
            }

            rapidjson::Document results_ga_progression;
            store::GAStore::executeQuerySync("SELECT * FROM ga_progression;", results_ga_progression);

            if (!results_ga_progression.IsNull() && !results_ga_progression.Empty())
            {
                for (rapidjson::Value::ConstValueIterator itr = results_ga_progression.Begin(); itr != results_ga_progression.End(); ++itr)
                {
                    i->_progressionTries.addOrUpdate((*itr)["progression"].GetString(), (int)strtol((*itr).HasMember("tries") ? (*itr)["tries"].GetString() : "0", NULL, 10));

                }
            }
        }

        void GAState::startNewSession()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            logging::GALogger::i("Starting a new session.");

            // make sure the current custom dimensions are valid
            GAState::validateAndFixCurrentDimensions();

            // call the init call
            http::GAHTTPApi *httpApi = http::GAHTTPApi::getInstance();
            if(!httpApi)
            {
                return;
            }
            rapidjson::Document initResponseDict;
            initResponseDict.SetObject();
            rapidjson::Document::AllocatorType& allocator = initResponseDict.GetAllocator();
            http::EGAHTTPApiResponse initResponse;
#if USE_UWP
            std::pair<http::EGAHTTPApiResponse, std::string> pair;
            try
            {
                pair = httpApi->requestInitReturningDict(i->_configsHash).get();
            }
            catch(Platform::COMException^ e)
            {
                pair = std::pair<http::EGAHTTPApiResponse, std::string>(http::NoResponse, "");
            }
            initResponse = pair.first;
            if(pair.second.size() > 0)
            {
                initResponseDict.Parse(pair.second.c_str());
            }
#else
            httpApi->requestInitReturningDict(initResponse, initResponseDict, i->_configsHash);
#endif

            // init is ok
            if ((initResponse == http::Ok || initResponse == http::Created) && !initResponseDict.IsNull())
            {
                // set the time offset - how many seconds the local time is different from servertime
                int64_t timeOffsetSeconds = 0;
                int64_t server_ts = initResponseDict.HasMember("server_ts") ? initResponseDict["server_ts"].GetInt64() : -1;
                if (server_ts > 0)
                {
                    timeOffsetSeconds = calculateServerTimeOffset(server_ts);
                }
                // insert timeOffset in received init config (so it can be used when offline)
                initResponseDict.AddMember("time_offset", timeOffsetSeconds, allocator);

                if(initResponse != http::Created)
                {
                    rapidjson::Value currentSdkConfig(rapidjson::kObjectType);
                    GAState::getSdkConfig(currentSdkConfig);
                    // use cached if not Created
                    if(currentSdkConfig.HasMember("configs") && currentSdkConfig["configs"].IsArray())
                    {
                        rapidjson::Value configs(rapidjson::kArrayType);
                        initResponseDict.AddMember("configs", configs, allocator);
                        initResponseDict["configs"].CopyFrom(currentSdkConfig["configs"], allocator);
                    }
                    if(currentSdkConfig.HasMember("configs_hash") && currentSdkConfig["configs_hash"].IsString())
                    {
                        rapidjson::Value configs_hash(currentSdkConfig["configs_hash"].GetString(), allocator);
                        initResponseDict.AddMember("configs_hash", configs_hash.Move(), allocator);
                    }
                    if(currentSdkConfig.HasMember("ab_id") && currentSdkConfig["ab_id"].IsString())
                    {
                        rapidjson::Value ab_id(currentSdkConfig["ab_id"].GetString(), allocator);
                        initResponseDict.AddMember("ab_id", ab_id.Move(), allocator);
                    }
                    if(currentSdkConfig.HasMember("ab_variant_id") && currentSdkConfig["ab_variant_id"].IsString())
                    {
                        rapidjson::Value ab_variant_id(currentSdkConfig["ab_variant_id"].GetString(), allocator);
                        initResponseDict.AddMember("ab_variant_id", ab_variant_id.Move(), allocator);
                    }
                }

                GAState::setConfigsHash(initResponseDict.HasMember("configs_hash") && initResponseDict["configs_hash"].IsString() ? initResponseDict["configs_hash"].GetString() : "");
                GAState::setAbId(initResponseDict.HasMember("ab_id") && initResponseDict["ab_id"].IsString() ? initResponseDict["ab_id"].GetString() : "");
                GAState::setAbVariantId(initResponseDict.HasMember("ab_variant_id") && initResponseDict["ab_variant_id"].IsString() ? initResponseDict["ab_variant_id"].GetString() : "");

                rapidjson::StringBuffer buffer;
                {
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    initResponseDict.Accept(writer);
                }

                // insert new config in sql lite cross session storage
                store::GAStore::setState("sdk_config_cached", buffer.GetString());

                // set new config and cache in memory
                i->_sdkConfigCached.CopyFrom(initResponseDict, i->_sdkConfigCached.GetAllocator());
                i->_sdkConfig.CopyFrom(initResponseDict, i->_sdkConfig.GetAllocator());

                i->_initAuthorized = true;
            }
            else if (initResponse == http::Unauthorized) {
                logging::GALogger::w("Initialize SDK failed - Unauthorized");
                i->_initAuthorized = false;
            }
            else
            {
                // log the status if no connection
                if (initResponse == http::NoResponse || initResponse == http::RequestTimeout)
                {
                    logging::GALogger::i("Init call (session start) failed - no response. Could be offline or timeout.");
                }
                else if (initResponse == http::BadResponse || initResponse == http::JsonEncodeFailed || initResponse == http::JsonDecodeFailed)
                {
                    logging::GALogger::i("Init call (session start) failed - bad response. Could be bad response from proxy or GA servers.");
                }
                else if (initResponse == http::BadRequest || initResponse == http::UnknownResponseCode)
                {
                    logging::GALogger::i("Init call (session start) failed - bad request or unknown response.");
                }

                // init call failed (perhaps offline)
                if (i->_sdkConfig.IsNull())
                {
                    if (!i->_sdkConfigCached.IsNull())
                    {
                        logging::GALogger::i("Init call (session start) failed - using cached init values.");
                        // set last cross session stored config init values
                        i->_sdkConfig.CopyFrom(i->_sdkConfigCached, i->_sdkConfig.GetAllocator());
                    }
                    else
                    {
                        logging::GALogger::i("Init call (session start) failed - using default init values.");
                        // set default init values

                        if(i->_sdkConfigDefault.IsNull())
                        {
                            i->_sdkConfigDefault.SetObject();
                        }

                        i->_sdkConfig.CopyFrom(i->_sdkConfigDefault, i->_sdkConfig.GetAllocator());
                    }
                }
                else
                {
                    logging::GALogger::i("Init call (session start) failed - using cached init values.");
                }
                i->_initAuthorized = true;
            }

            rapidjson::Value currentSdkConfig;
            GAState::getSdkConfig(currentSdkConfig);

            {
                if (currentSdkConfig.IsObject() && ((currentSdkConfig.HasMember("enabled") && currentSdkConfig["enabled"].IsBool()) ? currentSdkConfig["enabled"].GetBool() : true) == false)
                {
                    i->_enabled = false;
                }
                else if (!i->_initAuthorized)
                {
                    i->_enabled = false;
                }
                else
                {
                    i->_enabled = true;
                }
            }

            // set offset in state (memory) from current config (config could be from cache etc.)

            i->_clientServerTimeOffset = currentSdkConfig.HasMember("time_offset") ? currentSdkConfig["time_offset"].GetInt64() : 0;

            // populate configurations
            populateConfigurations(currentSdkConfig);

            // if SDK is disabled in config
            if (!GAState::isEnabled())
            {
                logging::GALogger::w("Could not start session: SDK is disabled.");
                // stop event queue
                // + make sure it's able to restart if another session detects it's enabled again
                events::GAEvents::stopEventQueue();
                return;
            }
            else
            {
                events::GAEvents::ensureEventQueueIsRunning();
            }

            // generate the new session
            char newSessionId[65] = "";
            utilities::GAUtilities::generateUUID(newSessionId);
            utilities::GAUtilities::lowercaseString(newSessionId);

            // Set session id
            snprintf(i->_sessionId, sizeof(i->_sessionId), "%s", newSessionId);

            // Set session start
            i->_sessionStart = getClientTsAdjusted();

            // Add session start event
            events::GAEvents::addSessionStartEvent();
        }

        void GAState::validateAndFixCurrentDimensions()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            // validate that there are no current dimension01 not in list
            if (!validators::GAValidator::validateDimension01(i->_currentCustomDimension01))
            {
                logging::GALogger::d("Invalid dimension01 found in variable. Setting to nil. Invalid dimension: %s", i->_currentCustomDimension01);
                setCustomDimension01("");
            }
            // validate that there are no current dimension02 not in list
            if (!validators::GAValidator::validateDimension02(i->_currentCustomDimension02))
            {
                logging::GALogger::d("Invalid dimension02 found in variable. Setting to nil. Invalid dimension: %s", i->_currentCustomDimension02);
                setCustomDimension02("");
            }
            // validate that there are no current dimension03 not in list
            if (!validators::GAValidator::validateDimension03(i->_currentCustomDimension03))
            {
                logging::GALogger::d("Invalid dimension03 found in variable. Setting to nil. Invalid dimension: %s", i->_currentCustomDimension03);
                setCustomDimension03("");
            }
        }

        bool GAState::sessionIsStarted()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return i->_sessionStart != 0;
        }

        std::vector<char> GAState::getRemoteConfigsStringValue(const char* key, const char* defaultValue)
        {
            std::vector<char> result;
            GAState* i = getInstance();
            if(!i)
            {
                result.push_back('\0');
                return result;
            }
            std::lock_guard<std::mutex> lg(i->_mtx);
            const char* returnValue = i->_configurations.HasMember(key) ? i->_configurations[key].GetString() : defaultValue;

            size_t s = strlen(returnValue);
            for(size_t i = 0; i < s; ++i)
            {
                result.push_back(returnValue[i]);
            }
            result.push_back('\0');

            return result;
        }

        bool GAState::isRemoteConfigsReady()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return i->_remoteConfigsIsReady;
        }

        void GAState::addRemoteConfigsListener(const std::shared_ptr<IRemoteConfigsListener>& listener)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            if(std::find(i->_remoteConfigsListeners.begin(), i->_remoteConfigsListeners.end(), listener) == i->_remoteConfigsListeners.end())
            {
                i->_remoteConfigsListeners.push_back(listener);
            }
        }

        void GAState::removeRemoteConfigsListener(const std::shared_ptr<IRemoteConfigsListener>& listener)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            if(std::find(i->_remoteConfigsListeners.begin(), i->_remoteConfigsListeners.end(), listener) != i->_remoteConfigsListeners.end())
            {
                i->_remoteConfigsListeners.erase(std::remove(i->_remoteConfigsListeners.begin(), i->_remoteConfigsListeners.end(), listener), i->_remoteConfigsListeners.end());
            }
        }

        std::vector<char> GAState::getRemoteConfigsContentAsString()
        {
            std::vector<char> result;
            GAState* i = getInstance();
            if(!i)
            {
                result.push_back('\0');
                return result;
            }

            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            i->_configurations.Accept(writer);
            const char* returnValue = buffer.GetString();

            size_t s = strlen(returnValue);
            for(size_t i = 0; i < s; ++i)
            {
                result.push_back(returnValue[i]);
            }
            result.push_back('\0');

            return result;
        }

        void GAState::populateConfigurations(rapidjson::Value& sdkConfig)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            i->_mtx.lock();

            i->_configurations.SetObject();
            rapidjson::Document::AllocatorType& allocator = i->_configurations.GetAllocator();

            if(sdkConfig.HasMember("configs") && sdkConfig["configs"].IsArray())
            {
                rapidjson::Value& configurations = sdkConfig["configs"];

                for (rapidjson::Value::ConstValueIterator itr = configurations.Begin(); itr != configurations.End(); ++itr)
                {
                    const rapidjson::Value& configuration = *itr;

                    if(!configuration.IsNull())
                    {
                        const char* key = (configuration.HasMember("key") && configuration["key"].IsString()) ? configuration["key"].GetString() : "";
                        int64_t start_ts = (configuration.HasMember("start_ts") && configuration["start_ts"].IsInt64()) ? configuration["start_ts"].GetInt64() : LONG_MIN;
                        int64_t end_ts = (configuration.HasMember("end_ts") && configuration["end_ts"].IsInt64()) ? configuration["end_ts"].GetInt64() : LONG_MAX;
                        int64_t client_ts_adjusted = getClientTsAdjusted();

                        if(strlen(key) > 0 && configuration.HasMember("value") && client_ts_adjusted > start_ts && client_ts_adjusted < end_ts)
                        {
                            if(configuration["value"].IsString())
                            {
                                rapidjson::Value v(key, allocator);
                                rapidjson::Value v1(configuration["value"].GetString(), allocator);
                                i->_configurations.AddMember(v.Move(), v1.Move(), allocator);
                            }
                            else if(configuration["value"].IsNumber())
                            {
                                rapidjson::Value v(key, allocator);

                                if(configuration["value"].IsInt64())
                                {
                                    i->_configurations.AddMember(v.Move(), configuration["value"].GetInt64(), allocator);
                                }
                                else if(configuration["value"].IsInt())
                                {
                                    i->_configurations.AddMember(v.Move(), configuration["value"].GetInt(), allocator);
                                }
                                else if(configuration["value"].IsDouble())
                                {
                                    i->_configurations.AddMember(v.Move(), configuration["value"].GetDouble(), allocator);
                                }
                            }

                            rapidjson::StringBuffer buffer;
                            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
                            configuration.Accept(writer);

                            logging::GALogger::d("configuration added: %s", buffer.GetString());
                        }
                    }
                }
            }

            i->_remoteConfigsIsReady = true;
            for(auto& listener : i->_remoteConfigsListeners)
            {
                listener->onRemoteConfigsUpdated();
            }

            i->_mtx.unlock();
        }

        void GAState::addErrorEvent(const char* baseMessage, EGAErrorSeverity severity, const char* message)
        {
            if(!GAState::isEventSubmissionEnabled())
            {
                return;
            }

            if(timestampMap.IsNull())
            {
                timestampMap.SetObject();
            }
            if(countMap.IsNull())
            {
                countMap.SetObject();
            }

            rapidjson::Document::AllocatorType& timestampMapAllocator = timestampMap.GetAllocator();
            rapidjson::Document::AllocatorType& countMapMapAllocator = countMap.GetAllocator();

            int64_t now = utilities::GAUtilities::timeIntervalSince1970();
            if(!timestampMap.HasMember(baseMessage))
            {
                rapidjson::Value v(baseMessage, timestampMapAllocator);
                timestampMap.AddMember(v.Move(), now, timestampMapAllocator);
            }
            if(!countMap.HasMember(baseMessage))
            {
                rapidjson::Value v(baseMessage, countMapMapAllocator);
                countMap.AddMember(v.Move(), 0, countMapMapAllocator);
            }

            int64_t diff = now - timestampMap[baseMessage].GetInt64();
            if(diff >= 3600)
            {
                countMap[baseMessage] = 0;
                countMap.FindMember(baseMessage)->value = 0;
                timestampMap.FindMember(baseMessage)->value = now;
            }

            if(countMap[baseMessage].GetInt() >= MaxCount)
            {
                return;
            }

            std::array<char, 8200> baseMessage_ = {'\0'};
            snprintf(baseMessage_.data(), baseMessage_.size(), "%s", baseMessage ? baseMessage : "");
            std::array<char, 8200> message_ = {'\0'};
            snprintf(message_.data(), message_.size(), "%s", message ? message : "");

            threading::GAThreading::performTaskOnGAThread([baseMessage_, severity, message_]()
            {
                rapidjson::Document fieldsJson;
                fieldsJson.Parse("{}");
                events::GAEvents::addErrorEvent(severity, message_.data(), fieldsJson, true);

                countMap.FindMember(baseMessage_.data())->value = countMap[baseMessage_.data()].GetInt() + 1;
            });
        }

        void GAState::validateAndCleanCustomFields(const rapidjson::Value& fields, rapidjson::Value& out)
        {
            rapidjson::Document result;
            result.SetObject();
            rapidjson::Document::AllocatorType& allocator = result.GetAllocator();

            if (fields.IsObject() && fields.MemberCount() > 0)
            {
                int count = 0;

                for (rapidjson::Value::ConstMemberIterator itr = fields.MemberBegin(); itr != fields.MemberEnd(); ++itr)
                {
                    const char* key = itr->name.GetString();
                    if(fields[key].IsNull())
                    {
                        const char* baseMessage = "validateAndCleanCustomFields: entry with key=%s, value=null has been omitted because its key or value is null";
                        std::array<char, 8200> message = {'\0'};
                        snprintf(message.data(), message.size(), baseMessage, key);
                        logging::GALogger::w(message.data());
                        addErrorEvent(baseMessage, EGAErrorSeverity::Warning, message.data());
                    }
                    else if(count < MAX_CUSTOM_FIELDS_COUNT)
                    {
                        char pattern[65] = "";
                        snprintf(pattern, sizeof(pattern), "^[a-zA-Z0-9_]{1,%d}$", MAX_CUSTOM_FIELDS_KEY_LENGTH);
                        if(utilities::GAUtilities::stringMatch(key, pattern))
                        {
                            const rapidjson::Value& value = fields[key];

                            if(value.IsNumber())
                            {
                                rapidjson::Value v(key, allocator);
                                result.AddMember(v.Move(), value.GetDouble(), allocator);
                                ++count;
                            }
                            else if(value.IsString())
                            {
                                std::string valueAsString = value.GetString();

                                if(valueAsString.length() <= MAX_CUSTOM_FIELDS_VALUE_STRING_LENGTH && valueAsString.length() > 0)
                                {
                                    rapidjson::Value v(key, allocator);
                                    rapidjson::Value v1(value.GetString(), allocator);
                                    result.AddMember(v.Move(), v1.Move(), allocator);
                                    ++count;
                                }
                                else
                                {
                                    const char* baseMessage = "validateAndCleanCustomFields: entry with key=%s, value=%s has been omitted because its value is an empty string or exceeds the max number of characters (%d)";
                                    std::array<char, 8200> message = {'\0'};
                                    snprintf(message.data(), message.size(), baseMessage, key, fields[key].GetString(), MAX_CUSTOM_FIELDS_VALUE_STRING_LENGTH);
                                    logging::GALogger::w(message.data());
                                    addErrorEvent(baseMessage, EGAErrorSeverity::Warning, message.data());
                                }
                            }
                            else
                            {
                                const char* baseMessage = "validateAndCleanCustomFields: entry with key=%s has been omitted because its value is not a string or number";
                                std::array<char, 8200> message = {'\0'};
                                snprintf(message.data(), message.size(), baseMessage, key);
                                logging::GALogger::w(message.data());
                                addErrorEvent(baseMessage, EGAErrorSeverity::Warning, message.data());
                            }
                        }
                        else
                        {
                            const char* baseMessage = "validateAndCleanCustomFields: entry with key=%s, value=%s has been omitted because its key contains illegal character, is empty or exceeds the max number of characters (%d)";
                            std::array<char, 8200> message = {'\0'};
                            snprintf(message.data(), message.size(), baseMessage, key, fields[key].GetString(), MAX_CUSTOM_FIELDS_KEY_LENGTH);
                            logging::GALogger::w(message.data());
                            addErrorEvent(baseMessage, EGAErrorSeverity::Warning, message.data());
                        }
                    }
                    else
                    {
                        const char* baseMessage = "validateAndCleanCustomFields: entry with key=%s has been omitted because it exceeds the max number of custom fields (%d)";
                        std::array<char, 8200> message = {'\0'};
                        snprintf(message.data(), message.size(), baseMessage, key, MAX_CUSTOM_FIELDS_COUNT);
                        logging::GALogger::w(message.data());
                        addErrorEvent(baseMessage, EGAErrorSeverity::Warning, message.data());
                    }
                }
            }

            out.CopyFrom(result, allocator);
        }

        int64_t GAState::getClientTsAdjusted()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return 0;
            }

            int64_t clientTs = utilities::GAUtilities::timeIntervalSince1970();
            int64_t clientTsAdjustedInteger = clientTs + i->_clientServerTimeOffset;

            if (validators::GAValidator::validateClientTs(clientTsAdjustedInteger))
            {
                return clientTsAdjustedInteger;
            }
            else
            {
                return clientTs;
            }
        }

        const char* GAState::getBuild()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return NULL;
            }
            return i->_build;
        }

        int64_t GAState::calculateServerTimeOffset(int64_t serverTs)
        {
            int64_t clientTs = utilities::GAUtilities::timeIntervalSince1970();
            return serverTs - clientTs;
        }

        void GAState::setManualSessionHandling(bool flag)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            i->_useManualSessionHandling = flag;
            if(flag)
            {
                logging::GALogger::i("Use manual session handling: true");
            }
            else
            {
                logging::GALogger::i("Use manual session handling: false");
            }
        }

        bool GAState::useManualSessionHandling()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return i->_useManualSessionHandling;
        }

        void GAState::setEnableErrorReporting(bool flag)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            i->_enableErrorReporting = flag;
            if(flag)
            {
                logging::GALogger::i("Use error reporting: true");
            }
            else
            {
                logging::GALogger::i("Use error reporting: false");
            }
        }

        bool GAState::useErrorReporting()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return i->_enableErrorReporting;
        }

        void GAState::setEnabledEventSubmission(bool flag)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }
            i->_enableEventSubmission = flag;
        }

        bool GAState::isEventSubmissionEnabled()
        {
            GAState* i = getInstance();
            if(!i)
            {
                return false;
            }
            return i->_enableEventSubmission;
        }

        void GAState::setConfigsHash(const char* configsHash)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            snprintf(i->_configsHash, sizeof(i->_configsHash), "%s", configsHash);
        }

        void GAState::setAbId(const char* abId)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            snprintf(i->_abId, sizeof(i->_abId), "%s", abId);
        }

        void GAState::setAbVariantId(const char* abVariantId)
        {
            GAState* i = getInstance();
            if(!i)
            {
                return;
            }

            snprintf(i->_abVariantId, sizeof(i->_abVariantId), "%s", abVariantId);
        }

        std::vector<char> GAState::getAbId()
        {
            std::vector<char> result;
            GAState* i = getInstance();
            if(!i)
            {
                result.push_back('\0');
                return result;
            }

            const char* returnValue = i->_abId;

            size_t s = strlen(returnValue);
            for(size_t i = 0; i < s; ++i)
            {
                result.push_back(returnValue[i]);
            }
            result.push_back('\0');

            return result;
        }

        std::vector<char> GAState::getAbVariantId()
        {
            std::vector<char> result;
            GAState* i = getInstance();
            if(!i)
            {
                result.push_back('\0');
                return result;
            }

            const char* returnValue = i->_abVariantId;

            size_t s = strlen(returnValue);
            for(size_t i = 0; i < s; ++i)
            {
                result.push_back(returnValue[i]);
            }
            result.push_back('\0');

            return result;
        }
    }
}
