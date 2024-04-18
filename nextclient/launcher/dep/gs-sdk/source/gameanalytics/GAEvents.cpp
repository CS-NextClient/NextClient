//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#include <vector>
#include "GAEvents.h"
#include "GAState.h"
#include "GAUtilities.h"
#include "GALogger.h"
#include "GAStore.h"
#include "GAThreading.h"
#include "GAValidator.h"
#include <string.h>
#include <stdio.h>
#include <cmath>
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"
#include <inttypes.h>

bool mergeObjects(rapidjson::Value &dstObject, const rapidjson::Value &srcObject, rapidjson::Document::AllocatorType &allocator, bool overwrite)
{
    for (auto srcIt = srcObject.MemberBegin(); srcIt != srcObject.MemberEnd(); ++srcIt)
    {
        auto dstIt = dstObject.FindMember(srcIt->name);
        if (dstIt == dstObject.MemberEnd())
        {
            rapidjson::Value dstName;
            dstName.CopyFrom(srcIt->name, allocator);
            rapidjson::Value dstVal;
            dstVal.CopyFrom(srcIt->value, allocator);

            dstObject.AddMember(dstName, dstVal, allocator);

            dstName.CopyFrom(srcIt->name, allocator);
            dstIt = dstObject.FindMember(dstName);
            if (dstIt == dstObject.MemberEnd())
                return false;
        }
        else if(overwrite)
        {
            auto srcT = srcIt->value.GetType();
            auto dstT = dstIt->value.GetType();
            if (srcT != dstT)
                return false;

            if (srcIt->value.IsArray())
            {
                for (auto arrayIt = srcIt->value.Begin(); arrayIt != srcIt->value.End(); ++arrayIt)
                {
                    rapidjson::Value dstVal;
                    dstVal.CopyFrom(*arrayIt, allocator);
                    dstIt->value.PushBack(dstVal, allocator);
                }
            }
            else if (srcIt->value.IsObject())
            {
                if (!mergeObjects(dstIt->value, srcIt->value, allocator, overwrite))
                    return false;
            }
            else
            {
                dstIt->value.CopyFrom(srcIt->value, allocator);
            }
        }
    }

    return true;
}

namespace gameanalytics
{
    namespace events
    {
        const char* GAEvents::CategorySessionStart = "user";
        const char* GAEvents::CategorySessionEnd = "session_end";
        const char* GAEvents::CategoryDesign = "design";
        const char* GAEvents::CategoryBusiness = "business";
        const char* GAEvents::CategoryProgression = "progression";
        const char* GAEvents::CategoryResource = "resource";
        const char* GAEvents::CategoryError = "error";
        const double GAEvents::ProcessEventsIntervalInSeconds = 8.0;
        const int GAEvents::MaxEventCount = 500;

        bool GAEvents::_destroyed = false;
        GAEvents* GAEvents::_instance = 0;
        std::once_flag GAEvents::_initInstanceFlag;

        GAEvents::GAEvents()
        {
            isRunning = false;
            keepRunning = false;
        }

        GAEvents::~GAEvents()
        {
            isRunning = false;
            keepRunning = false;
        }

        void GAEvents::cleanUp()
        {
            delete _instance;
            _instance = 0;
            _destroyed = true;
            threading::GAThreading::endThread();
        }

        GAEvents* GAEvents::getInstance()
        {
            std::call_once(_initInstanceFlag, &GAEvents::initInstance);
            return _instance;
        }

        void GAEvents::stopEventQueue()
        {
            GAEvents* i = GAEvents::getInstance();
            if(!i)
            {
                return;
            }

            i->keepRunning = false;
        }

        void GAEvents::ensureEventQueueIsRunning()
        {
            GAEvents* i = GAEvents::getInstance();
            if(!i)
            {
                return;
            }

            i->keepRunning = true;
            if (!i->isRunning)
            {
                i->isRunning = true;
                threading::GAThreading::scheduleTimer(GAEvents::ProcessEventsIntervalInSeconds, processEventQueue);
            }
        }

        // USER EVENTS
        void GAEvents::addSessionStartEvent()
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            const char* categorySessionStart = GAEvents::CategorySessionStart;

            // Event specific data
            rapidjson::Document eventDict;
            eventDict.SetObject();
            rapidjson::Document::AllocatorType& allocator = eventDict.GetAllocator();
            {
                rapidjson::Value v(categorySessionStart, allocator);
                eventDict.AddMember("category", v.Move(), allocator);
            }


            // Increment session number  and persist
            state::GAState::incrementSessionNum();
            char sessionNum[11] = "";
            snprintf(sessionNum, sizeof(sessionNum), "%d", state::GAState::getSessionNum());
            const char* parameters[2] = {"session_num", sessionNum};
            store::GAStore::executeQuerySync("INSERT OR REPLACE INTO ga_state (key, value) VALUES(?, ?);", parameters, 2);

            // Add custom dimensions
            GAEvents::addDimensionsToEvent(eventDict);

            rapidjson::Document cleanedFields;
            cleanedFields.SetObject();

            {
                rapidjson::Document d;
                d.SetObject();
                state::GAState::getGlobalCustomEventFields(d);
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }

            GAEvents::addCustomFieldsToEvent(eventDict, cleanedFields);

            // Add to store
            addEventToStore(eventDict);

            // Log
            logging::GALogger::i("Add SESSION START event");

            // Send event right away
            GAEvents::processEvents(categorySessionStart, false);
        }

        void GAEvents::addSessionEndEvent()
        {
            state::GAState* state = state::GAState::getInstance();
            if(!state)
            {
                return;
            }

            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            int64_t session_start_ts = state->getSessionStart();
            int64_t client_ts_adjusted = state::GAState::getClientTsAdjusted();
            int64_t sessionLength = client_ts_adjusted - session_start_ts;

            if(sessionLength < 0)
            {
                // Should never happen.
                // Could be because of edge cases regarding time altering on device.
                logging::GALogger::w("Session length was calculated to be less then 0. Should not be possible. Resetting to 0.");
                sessionLength = 0;
            }

            // Event specific data
            rapidjson::Document eventDict;
            eventDict.SetObject();
            rapidjson::Document::AllocatorType& allocator = eventDict.GetAllocator();

            {
                rapidjson::Value v(GAEvents::CategorySessionEnd, allocator);
                eventDict.AddMember("category", v.Move(), allocator);
            }
            eventDict.AddMember("length", sessionLength, allocator);

            // Add custom dimensions
            GAEvents::addDimensionsToEvent(eventDict);

            rapidjson::Document cleanedFields;
            cleanedFields.SetObject();

            {
                rapidjson::Document d;
                d.SetObject();
                state::GAState::getGlobalCustomEventFields(d);
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }

            GAEvents::addCustomFieldsToEvent(eventDict, cleanedFields);

            // Add to store
            addEventToStore(eventDict);

            // Log
            logging::GALogger::i("Add SESSION END event.");

            // Send all event right away
            GAEvents::processEvents("", false);
        }

        // BUSINESS EVENT
        void GAEvents::addBusinessEvent(const char* currency, int amount, const char* itemType, const char* itemId, const char* cartType, const rapidjson::Value& fields, bool mergeFields)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            // Validate event params
            validators::ValidationResult validationResult;
            validators::GAValidator::validateBusinessEvent(currency, amount, cartType, itemType, itemId, validationResult);
            if (!validationResult.result)
            {
                http::GAHTTPApi* httpInstance = http::GAHTTPApi::getInstance();
                if(!httpInstance)
                {
                    return;
                }
                httpInstance->sendSdkErrorEvent(validationResult.category, validationResult.area, validationResult.action, validationResult.parameter, validationResult.reason, state::GAState::getGameKey(), state::GAState::getGameSecret());
                return;
            }

            // Create empty eventData
            rapidjson::Document eventDict;
            eventDict.SetObject();
            rapidjson::Document::AllocatorType& allocator = eventDict.GetAllocator();

            // Increment transaction number and persist
            state::GAState::incrementTransactionNum();
            char transactionNum[11] = "";
            snprintf(transactionNum, sizeof(transactionNum), "%d", state::GAState::getTransactionNum());
            const char* params[2] = {"transaction_num", transactionNum};
            store::GAStore::executeQuerySync("INSERT OR REPLACE INTO ga_state (key, value) VALUES(?, ?);", params, 2);

            // Required
            {
                char s[129] = "";
                snprintf(s, sizeof(s), "%s:%s", itemType, itemId);
                rapidjson::Value v(s, allocator);
                eventDict.AddMember("event_id", v.Move(), allocator);
            }
            {
                rapidjson::Value v(GAEvents::CategoryBusiness, allocator);
                eventDict.AddMember("category", v.Move(), allocator);
            }
            {
                rapidjson::Value v(currency, allocator);
                eventDict.AddMember("currency", v.Move(), allocator);
            }
            eventDict.AddMember("amount", amount, allocator);
            eventDict.AddMember("transaction_num", state::GAState::getTransactionNum(), allocator);

            // Optional
            if (strlen(cartType) > 0)
            {
                rapidjson::Value v(cartType, allocator);
                eventDict.AddMember("cart_type", v.Move(), allocator);
            }

            // Add custom dimensions
            GAEvents::addDimensionsToEvent(eventDict);

            rapidjson::Document cleanedFields;
            cleanedFields.SetObject();

            if (fields.IsObject() && fields.MemberCount() > 0)
            {
                rapidjson::Document d;
                d.SetObject();
                mergeObjects(d, fields, d.GetAllocator(), false);
                if(mergeFields)
                {
                    rapidjson::Document globalFields;
                    globalFields.SetObject();
                    state::GAState::getGlobalCustomEventFields(d);
                    mergeObjects(d, globalFields, d.GetAllocator(), false);
                }
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }
            else
            {
                rapidjson::Document d;
                d.SetObject();
                state::GAState::getGlobalCustomEventFields(d);
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }

            GAEvents::addCustomFieldsToEvent(eventDict, cleanedFields);

            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                cleanedFields.Accept(writer);
            }

            // Log
            logging::GALogger::i("Add BUSINESS event: {currency:%s, amount:%d, itemType:%s, itemId:%s, cartType:%s, fields:%s}", currency, amount, itemType, itemId, cartType, buffer.GetString());

            // Send to store
            addEventToStore(eventDict);
        }

        void GAEvents::addResourceEvent(EGAResourceFlowType flowType, const char* currency, double amount, const char* itemType, const char* itemId, const rapidjson::Value& fields, bool mergeFields)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            // Validate event params
            validators::ValidationResult validationResult;
            validators::GAValidator::validateResourceEvent(flowType, currency, amount, itemType, itemId, validationResult);
            if (!validationResult.result)
            {
                http::GAHTTPApi* httpInstance = http::GAHTTPApi::getInstance();
                if(!httpInstance)
                {
                    return;
                }
                httpInstance->sendSdkErrorEvent(validationResult.category, validationResult.area, validationResult.action, validationResult.parameter, validationResult.reason, state::GAState::getGameKey(), state::GAState::getGameSecret());
                return;
            }

            // If flow type is sink reverse amount
            if (flowType == Sink)
            {
                amount *= -1;
            }

            // Create empty eventData
            rapidjson::Document eventDict;
            eventDict.SetObject();
            rapidjson::Document::AllocatorType& allocator = eventDict.GetAllocator();

            // insert event specific values
            char flowTypeString[10] = "";
            resourceFlowTypeString(flowType, flowTypeString);
            {
                char s[257] = "";
                snprintf(s, sizeof(s), "%s:%s:%s:%s", flowTypeString, currency, itemType, itemId);
                rapidjson::Value v(s, allocator);
                eventDict.AddMember("event_id", v.Move(), allocator);
            }
            {
                rapidjson::Value v(GAEvents::CategoryResource, allocator);
                eventDict.AddMember("category", v.Move(), allocator);
            }
            eventDict.AddMember("amount", amount, allocator);

            // Add custom dimensions
            GAEvents::addDimensionsToEvent(eventDict);

            rapidjson::Document cleanedFields;
            cleanedFields.SetObject();

            if (fields.IsObject() && fields.MemberCount() > 0)
            {
                rapidjson::Document d;
                d.SetObject();
                mergeObjects(d, fields, d.GetAllocator(), false);
                if(mergeFields)
                {
                    rapidjson::Document globalFields;
                    globalFields.SetObject();
                    state::GAState::getGlobalCustomEventFields(d);
                    mergeObjects(d, globalFields, d.GetAllocator(), false);
                }
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }
            else
            {
                rapidjson::Document d;
                d.SetObject();
                state::GAState::getGlobalCustomEventFields(d);
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }

            GAEvents::addCustomFieldsToEvent(eventDict, cleanedFields);

            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                fields.Accept(writer);
            }

            // Log
            logging::GALogger::i("Add RESOURCE event: {currency:%s, amount: %f, itemType:%s, itemId:%s, fields:%s}", currency, amount, itemType, itemId, buffer.GetString());

            // Send to store
            addEventToStore(eventDict);
        }

        void GAEvents::addProgressionEvent(EGAProgressionStatus progressionStatus, const char* progression01, const char* progression02, const char* progression03, int score, bool sendScore, const rapidjson::Value& fields, bool mergeFields)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            char statusString[10] = "";
            progressionStatusString(progressionStatus, statusString);

            // Validate event params
            validators::ValidationResult validationResult;
            validators::GAValidator::validateProgressionEvent(progressionStatus, progression01, progression02, progression03, validationResult);
            if (!validationResult.result)
            {
                http::GAHTTPApi* httpInstance = http::GAHTTPApi::getInstance();
                if(!httpInstance)
                {
                    return;
                }
                httpInstance->sendSdkErrorEvent(validationResult.category, validationResult.area, validationResult.action, validationResult.parameter, validationResult.reason, state::GAState::getGameKey(), state::GAState::getGameSecret());
                return;
            }

            // Create empty eventData
            rapidjson::Document eventDict;
            eventDict.SetObject();
            rapidjson::Document::AllocatorType& allocator = eventDict.GetAllocator();

            // Progression identifier
            char progressionIdentifier[257] = "";

            if (strlen(progression02) == 0)
            {
                snprintf(progressionIdentifier, sizeof(progressionIdentifier), "%s", progression01);
            }
            else if (strlen(progression03) == 0)
            {
                snprintf(progressionIdentifier, sizeof(progressionIdentifier), "%s:%s", progression01, progression02);
            }
            else
            {
                snprintf(progressionIdentifier, sizeof(progressionIdentifier), "%s:%s:%s", progression01, progression02, progression03);
            }

            // Append event specifics
            {
                rapidjson::Value v(GAEvents::CategoryProgression, allocator);
                eventDict.AddMember("category", v.Move(), allocator);
            }
            {
                char s[513] = "";
                snprintf(s, sizeof(s), "%s:%s", statusString, progressionIdentifier);
                rapidjson::Value v(s, allocator);
                eventDict.AddMember("event_id", v.Move(), allocator);
            }

            // Attempt
            int attempt_num = 0;

            // Add score if specified and status is not start
            if (sendScore && progressionStatus != EGAProgressionStatus::Start)
            {
                eventDict.AddMember("score", score, allocator);
            }

            // Count attempts on each progression fail and persist
            if (progressionStatus == EGAProgressionStatus::Fail)
            {
                // Increment attempt number
                state::GAState::incrementProgressionTries(progressionIdentifier);
            }

            // increment and add attempt_num on complete and delete persisted
            if (progressionStatus == EGAProgressionStatus::Complete)
            {
                // Increment attempt number
                state::GAState::incrementProgressionTries(progressionIdentifier);

                // Add to event
                attempt_num = state::GAState::getProgressionTries(progressionIdentifier);
                eventDict.AddMember("attempt_num", attempt_num, allocator);

                // Clear
                state::GAState::clearProgressionTries(progressionIdentifier);
            }

            // Add custom dimensions
            GAEvents::addDimensionsToEvent(eventDict);

            rapidjson::Document cleanedFields;
            cleanedFields.SetObject();

            if (fields.IsObject() && fields.MemberCount() > 0)
            {
                rapidjson::Document d;
                d.SetObject();
                mergeObjects(d, fields, d.GetAllocator(), false);
                if(mergeFields)
                {
                    rapidjson::Document globalFields;
                    globalFields.SetObject();
                    state::GAState::getGlobalCustomEventFields(d);
                    mergeObjects(d, globalFields, d.GetAllocator(), false);
                }
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }
            else
            {
                rapidjson::Document d;
                d.SetObject();
                state::GAState::getGlobalCustomEventFields(d);
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }

            GAEvents::addCustomFieldsToEvent(eventDict, cleanedFields);

            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                fields.Accept(writer);
            }

            // Log
            logging::GALogger::i("Add PROGRESSION event: {status:%s, progression01:%s, progression02:%s, progression03:%s, score:%d, attempt:%d, fields:%s}", statusString, progression01, progression02, progression03, score, attempt_num, buffer.GetString());

            // Send to store
            addEventToStore(eventDict);
        }

        void GAEvents::addDesignEvent(const char* eventId, double value, bool sendValue, const rapidjson::Value& fields, bool mergeFields)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            // Validate
            validators::ValidationResult validationResult;
            validators::GAValidator::validateDesignEvent(eventId, validationResult);
            if (!validationResult.result)
            {
                http::GAHTTPApi* httpInstance = http::GAHTTPApi::getInstance();
                if(!httpInstance)
                {
                    return;
                }
                httpInstance->sendSdkErrorEvent(validationResult.category, validationResult.area, validationResult.action, validationResult.parameter, validationResult.reason, state::GAState::getGameKey(), state::GAState::getGameSecret());
                return;
            }

            // Create empty eventData
            rapidjson::Document eventData;
            eventData.SetObject();
            rapidjson::Document::AllocatorType& allocator = eventData.GetAllocator();

            // Append event specifics
            {
                rapidjson::Value v(GAEvents::CategoryDesign, allocator);
                eventData.AddMember("category", v.Move(), allocator);
            }
            {
                rapidjson::Value v(eventId, allocator);
                eventData.AddMember("event_id", v.Move(), allocator);
            }

            if (sendValue)
            {
                eventData.AddMember("value", value, allocator);
            }

            rapidjson::Document cleanedFields;
            cleanedFields.SetObject();

            if (fields.IsObject() && fields.MemberCount() > 0)
            {
                rapidjson::Document d;
                d.SetObject();
                mergeObjects(d, fields, d.GetAllocator(), false);
                if(mergeFields)
                {
                    rapidjson::Document globalFields;
                    globalFields.SetObject();
                    state::GAState::getGlobalCustomEventFields(d);
                    mergeObjects(d, globalFields, d.GetAllocator(), false);
                }
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }
            else
            {
                rapidjson::Document d;
                d.SetObject();
                state::GAState::getGlobalCustomEventFields(d);
                state::GAState::validateAndCleanCustomFields(d, cleanedFields);
            }

            GAEvents::addCustomFieldsToEvent(eventData, cleanedFields);

            // Add custom dimensions
            GAEvents::addDimensionsToEvent(eventData);

            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                fields.Accept(writer);
            }

            // Log
            logging::GALogger::i("Add DESIGN event: {eventId:%s, value:%f, fields:%s}", eventId, value, buffer.GetString());

            // Send to store
            addEventToStore(eventData);
        }

        void GAEvents::addErrorEvent(EGAErrorSeverity severity, const char* message, const rapidjson::Value& fields, bool mergeFields)
        {
            addErrorEvent(severity, message, fields, mergeFields, false);
        }

        void GAEvents::addErrorEvent(EGAErrorSeverity severity, const char* message, const rapidjson::Value& fields, bool mergeFields, bool skipAddingFields)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            char severityString[10] = "";
            errorSeverityString(severity, severityString);

            // Validate
            validators::ValidationResult validationResult;
            validators::GAValidator::validateErrorEvent(severity, message, validationResult);
            if (!validationResult.result)
            {
                http::GAHTTPApi* httpInstance = http::GAHTTPApi::getInstance();
                if(!httpInstance)
                {
                    return;
                }
                httpInstance->sendSdkErrorEvent(validationResult.category, validationResult.area, validationResult.action, validationResult.parameter, validationResult.reason, state::GAState::getGameKey(), state::GAState::getGameSecret());
                return;
            }

            // Create empty eventData
            rapidjson::Document eventData;
            eventData.SetObject();
            rapidjson::Document::AllocatorType& allocator = eventData.GetAllocator();

            // Append event specifics
            {
                rapidjson::Value v(GAEvents::CategoryError, allocator);
                eventData.AddMember("category", v.Move(), allocator);
            }
            {
                rapidjson::Value v(severityString, allocator);
                eventData.AddMember("severity", v.Move(), allocator);
            }
            {
                rapidjson::Value v(message, allocator);
                eventData.AddMember("message", v.Move(), allocator);
            }

            if(!skipAddingFields)
            {
                rapidjson::Document cleanedFields;
                cleanedFields.SetObject();

                if (fields.IsObject() && fields.MemberCount() > 0)
                {
                    rapidjson::Document d;
                    d.SetObject();
                    mergeObjects(d, fields, d.GetAllocator(), false);
                    if(mergeFields)
                    {
                        rapidjson::Document globalFields;
                        globalFields.SetObject();
                        state::GAState::getGlobalCustomEventFields(d);
                        mergeObjects(d, globalFields, d.GetAllocator(), false);
                    }
                    state::GAState::validateAndCleanCustomFields(d, cleanedFields);
                }
                else
                {
                    rapidjson::Document d;
                    d.SetObject();
                    state::GAState::getGlobalCustomEventFields(d);
                    state::GAState::validateAndCleanCustomFields(d, cleanedFields);
                }

                GAEvents::addCustomFieldsToEvent(eventData, cleanedFields);
            }

            // Add custom dimensions
            GAEvents::addDimensionsToEvent(eventData);

            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                fields.Accept(writer);
            }

            // Log
            logging::GALogger::i("Add ERROR event: {severity:%s, message:%s, fields:%s}", severityString, message, buffer.GetString());

            // Send to store
            addEventToStore(eventData);
        }

        void GAEvents::processEventQueue()
        {
            processEvents("", true);
            GAEvents* i = GAEvents::getInstance();
            if(!i)
            {
                return;
            }
            if (i->keepRunning)
            {
                threading::GAThreading::scheduleTimer(GAEvents::ProcessEventsIntervalInSeconds, processEventQueue);
            }
            else
            {
                i->isRunning = false;
            }
        }

        void GAEvents::processEvents(const char* category, bool performCleanup)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            // Request identifier
            char requestIdentifier[65] = "";
            utilities::GAUtilities::generateUUID(requestIdentifier);

            char selectSql[129] = "";
            char updateSql[257] = "";
            char deleteSql[129] = "";
            snprintf(deleteSql, sizeof(deleteSql), "DELETE FROM ga_events WHERE status = '%s'", requestIdentifier);
            char putbackSql[129] = "";
            snprintf(putbackSql, sizeof(putbackSql), "UPDATE ga_events SET status = 'new' WHERE status = '%s';", requestIdentifier);

            // Cleanup
            if (performCleanup)
            {
                cleanupEvents();
                fixMissingSessionEndEvents();
            }

            // Prepare SQL
            char andCategory[65] = "";
            if (strlen(category) > 0)
            {
                snprintf(andCategory, sizeof(andCategory), " AND category='%s' ", category);
            }
            snprintf(selectSql, sizeof(selectSql), "SELECT event FROM ga_events WHERE status = 'new' %s;", andCategory);
            snprintf(updateSql, sizeof(updateSql), "UPDATE ga_events SET status = '%s' WHERE status = 'new' %s;", requestIdentifier, andCategory);

            // Get events to process
            rapidjson::Document events;
            store::GAStore::executeQuerySync(selectSql, events);

            // Check for errors or empty
            if (events.IsNull() || events.Size() == 0)
            {
                logging::GALogger::i("Event queue: No events to send");
                GAEvents::updateSessionTime();
                return;
            }

            // Check number of events and take some action if there are too many?
            if (events.Size() > GAEvents::MaxEventCount)
            {
                // Make a limit request
                snprintf(selectSql, sizeof(selectSql), "SELECT client_ts FROM ga_events WHERE status = 'new' %s ORDER BY client_ts ASC LIMIT 0,%d;", andCategory, GAEvents::MaxEventCount);
                store::GAStore::executeQuerySync(selectSql, events);
                if (events.IsNull())
                {
                    return;
                }

                // Get last timestamp
                const rapidjson::Value& lastItem = events[events.Size() - 1];
                const char* lastTimestamp = lastItem["client_ts"].GetString();

                // Select again
                snprintf(selectSql, sizeof(selectSql), "SELECT event FROM ga_events WHERE status = 'new' %s AND client_ts<='%s';", andCategory, lastTimestamp);
                store::GAStore::executeQuerySync(selectSql, events);
                if (events.IsNull())
                {
                    return;
                }

                // Update sql
                snprintf(updateSql, sizeof(updateSql), "UPDATE ga_events SET status='%s' WHERE status='new' %s AND client_ts<='%s';", requestIdentifier, andCategory, lastTimestamp);
            }

            // Log
            logging::GALogger::i("Event queue: Sending %d events.", events.Size());

            // Set status of events to 'sending' (also check for error)
            rapidjson::Document updateResult;
            store::GAStore::executeQuerySync(updateSql, updateResult);
            if (updateResult.IsNull())
            {
                return;
            }

            // Create payload data from events
            rapidjson::Document payloadArray;
            payloadArray.SetArray();
            rapidjson::Document::AllocatorType& allocator = payloadArray.GetAllocator();

            for (rapidjson::Value::ConstValueIterator itr = events.Begin(); itr != events.End(); ++itr)
            {
                const char* eventDict = (*itr).HasMember("event") ? (*itr)["event"].GetString() : "";
                if (strlen(eventDict) > 0)
                {
                    rapidjson::Document d;
                    rapidjson::ParseResult ok = d.Parse(eventDict);
                    if(!ok)
                    {
                        logging::GALogger::d("processEvents -- JSON error (offset %u): %s", (unsigned)ok.Offset(), GetParseError_En(ok.Code()));
                        logging::GALogger::d("%s", eventDict);
                    }
                    else
                    {
                        if(d.HasMember("client_ts"))
                        {
                            if (!validators::GAValidator::validateClientTs(d["client_ts"].GetInt64()))
                            {
                                d.RemoveMember("client_ts");
                            }
                        }

                        rapidjson::Value v;
                        v.CopyFrom(d, allocator);
                        payloadArray.PushBack(v.Move(), allocator);
                    }
                }
            }

            rapidjson::StringBuffer buffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                payloadArray.Accept(writer);
            }

            // send events
            rapidjson::Value dataDict(rapidjson::kArrayType);
            http::EGAHTTPApiResponse responseEnum;
            http::GAHTTPApi* http = http::GAHTTPApi::getInstance();
            if(!http)
            {
                return;
            }
#if USE_UWP
            std::pair<http::EGAHTTPApiResponse, std::string> pair;

            try
            {
                pair = http->sendEventsInArray(payloadArray).get();
            }
            catch(Platform::COMException^ e)
            {
                pair = std::pair<http::EGAHTTPApiResponse, std::string>(http::NoResponse, "");
            }
            responseEnum = pair.first;
            rapidjson::Document d;
            if(pair.second.size() > 0)
            {
                rapidjson::ParseResult ok = d.Parse(pair.second.c_str());
                if(!ok)
                {
                    logging::GALogger::d("processEvents -- JSON error (offset %u): %s", (unsigned)ok.Offset(), GetParseError_En(ok.Code()));
                    logging::GALogger::d("%s", pair.second.c_str());
                }
                else
                {
                    dataDict.CopyFrom(d, d.GetAllocator());
                }
            }
#else
            http->sendEventsInArray(responseEnum, dataDict, payloadArray);
#endif

            if (responseEnum == http::Ok)
            {
                // Delete events
                store::GAStore::executeQuerySync(deleteSql);

                logging::GALogger::i("Event queue: %d events sent.", events.Size());
            }
            else
            {
                // Put events back (Only in case of no response)
                if (responseEnum == http::NoResponse)
                {
                    logging::GALogger::w("Event queue: Failed to send events to collector - Retrying next time");
                    store::GAStore::executeQuerySync(putbackSql);
                    // Delete events (When getting some anwser back always assume events are processed)
                }
                else
                {
                    if (responseEnum == http::BadRequest && dataDict.IsArray())
                    {
                        logging::GALogger::w("Event queue: %d events sent. %d events failed GA server validation.", events.Size(), dataDict.Size());
                    }
                    else
                    {
                        logging::GALogger::w("Event queue: Failed to send events.");
                    }

                    store::GAStore::executeQuerySync(deleteSql);
                }
            }
        }

        void GAEvents::updateSessionTime()
        {
            if(state::GAState::sessionIsStarted())
            {
                rapidjson::Document ev;
                ev.SetObject();
                state::GAState::getEventAnnotations(ev);

                // Add custom dimensions
                GAEvents::addDimensionsToEvent(ev);

                rapidjson::Document cleanedFields;
                cleanedFields.SetObject();

                {
                    rapidjson::Document d;
                    d.SetObject();
                    state::GAState::getGlobalCustomEventFields(d);
                    state::GAState::validateAndCleanCustomFields(d, cleanedFields);
                }

                GAEvents::addCustomFieldsToEvent(ev, cleanedFields);

                rapidjson::StringBuffer buffer;
                {
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    ev.Accept(writer);
                }
                const char* jsonDefaults = buffer.GetString();
                const char* sql = "INSERT OR REPLACE INTO ga_session(session_id, timestamp, event) VALUES(?, ?, ?);";
                char sessionStart[21] = "";
                state::GAState* state = state::GAState::getInstance();
                if(!state)
                {
                    return;
                }
                snprintf(sessionStart, sizeof(sessionStart), "%" PRId64, state->getSessionStart());
                const char* parameters[3] = { ev["session_id"].GetString(), sessionStart, jsonDefaults};
                store::GAStore::executeQuerySync(sql, parameters, 3);
            }
        }

        void GAEvents::cleanupEvents()
        {
            store::GAStore::executeQuerySync("UPDATE ga_events SET status = 'new';");
        }

        void GAEvents::fixMissingSessionEndEvents()
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            // Get all sessions that are not current
            const char* parameters[] = { state::GAState::getSessionId() };

            const char* sql = "SELECT timestamp, event FROM ga_session WHERE session_id != ?;";
            rapidjson::Document sessions;
            store::GAStore::executeQuerySync(sql, parameters, 1, sessions);

            if (sessions.IsNull() || sessions.Empty())
            {
                return;
            }

            logging::GALogger::i("%d session(s) located with missing session_end event.", sessions.Size());

            // Add missing session_end events
            for (rapidjson::Value::ConstValueIterator itr = sessions.Begin(); itr != sessions.End(); ++itr)
            {
                const rapidjson::Value& session = *itr;
                if(session.HasMember("event"))
                {
                    rapidjson::Document sessionEndEvent;
                    rapidjson::ParseResult ok = sessionEndEvent.Parse(session["event"].GetString());
                    if(!ok)
                    {
                        logging::GALogger::d("fixMissingSessionEndEvents -- JSON error (offset %u): %s", (unsigned)ok.Offset(), GetParseError_En(ok.Code()));
                        logging::GALogger::d("%s", session["event"].GetString());
                    }
                    if(!ok)
                    {
                        logging::GALogger::d("JSON parse error: %s (%u)", rapidjson::GetParseError_En(ok.Code()), ok.Offset());
                    }

                    rapidjson::Document::AllocatorType& allocator = sessionEndEvent.GetAllocator();
                    int64_t event_ts = sessionEndEvent.HasMember("client_ts") ? sessionEndEvent["client_ts"].GetInt64() : 0;
                    int64_t start_ts = (int64_t)strtol(session.HasMember("timestamp") ? session["timestamp"].GetString() : "0", NULL, 10);

                    int64_t length = event_ts - start_ts;
                    length = static_cast<int64_t>(fmax(length, 0));

                    logging::GALogger::d("fixMissingSessionEndEvents length calculated: %lld", length);

                    {
                        rapidjson::Value v(GAEvents::CategorySessionEnd, allocator);
                        sessionEndEvent.AddMember("category", v.Move(), allocator);
                    }
                    sessionEndEvent.AddMember("length", length, allocator);

                    // Add to store
                    addEventToStore(sessionEndEvent);
                }
            }
        }

        // GENERAL
        void GAEvents::addEventToStore(rapidjson::Document& eventData)
        {
            if(!state::GAState::isEventSubmissionEnabled())
            {
                return;
            }

            if(store::GAStore::isDestroyed())
            {
                return;
            }

            // Check if datastore is available
            if (!store::GAStore::getTableReady())
            {
                logging::GALogger::w("Could not add event: SDK datastore error");
                return;
            }

            // Check if we are initialized
            if (!state::GAState::isInitialized())
            {
                logging::GALogger::w("Could not add event: SDK is not initialized");
                return;
            }

            // Check db size limits (10mb)
            // If database is too large block all except user, session and business
            if (store::GAStore::isDbTooLargeForEvents() && !utilities::GAUtilities::stringMatch(eventData["category"].GetString(), "^(user|session_end|business)$"))
            {
                logging::GALogger::w("Database too large. Event has been blocked.");
                http::GAHTTPApi* httpInstance = http::GAHTTPApi::getInstance();
                if(!httpInstance)
                {
                    return;
                }
                httpInstance->sendSdkErrorEvent(http::EGASdkErrorCategory::Database, http::EGASdkErrorArea::AddEventsToStore, http::EGASdkErrorAction::DatabaseTooLarge, (http::EGASdkErrorParameter)0, "", state::GAState::getGameKey(), state::GAState::getGameSecret());
                return;
            }

            // Get default annotations
            rapidjson::Document ev;
            ev.SetObject();
            state::GAState::getEventAnnotations(ev);

            // Merge with eventData
            mergeObjects(ev, eventData, ev.GetAllocator(), true);

            // Create json string representation
            rapidjson::StringBuffer evBuffer;
            {
                rapidjson::Writer<rapidjson::StringBuffer> writer(evBuffer);
                ev.Accept(writer);
            }
            const char* json = evBuffer.GetString();

            // output if VERBOSE LOG enabled
            logging::GALogger::ii("Event added to queue: %s", json);

            // Add to store
            char client_ts[21] = "";
            snprintf(client_ts, sizeof(client_ts), "%" PRId64, ev["client_ts"].GetInt64());
            const char* parameters[] = { "new", ev["category"].GetString(), ev["session_id"].GetString(), client_ts, json };
            const char* sql = "INSERT INTO ga_events (status, category, session_id, client_ts, event) VALUES(?, ?, ?, ?, ?);";

            store::GAStore::executeQuerySync(sql, parameters, 5);

            // Add to session store if not last
            if (strcmp(eventData["category"].GetString(), GAEvents::CategorySessionEnd) == 0)
            {
                const char* params[] = { ev["session_id"].GetString() };
                store::GAStore::executeQuerySync("DELETE FROM ga_session WHERE session_id = ?;", params, 1);
            }
            else
            {
                GAEvents::updateSessionTime();
            }
        }

        void GAEvents::addDimensionsToEvent(rapidjson::Document& eventData)
        {
            if (eventData.IsNull())
            {
                return;
            }

            rapidjson::Document::AllocatorType& allocator = eventData.GetAllocator();

            // add to dict (if not nil)
            if (strlen(state::GAState::getCurrentCustomDimension01()) > 0)
            {
                rapidjson::Value v(state::GAState::getCurrentCustomDimension01(), allocator);
                eventData.AddMember("custom_01", v.Move(), allocator);
            }
            if (strlen(state::GAState::getCurrentCustomDimension02()) > 0)
            {
                rapidjson::Value v(state::GAState::getCurrentCustomDimension02(), allocator);
                eventData.AddMember("custom_02", v.Move(), allocator);
            }
            if (strlen(state::GAState::getCurrentCustomDimension03()) > 0)
            {
                rapidjson::Value v(state::GAState::getCurrentCustomDimension03(), allocator);
                eventData.AddMember("custom_03", v.Move(), allocator);
            }
        }

        void GAEvents::addCustomFieldsToEvent(rapidjson::Document &eventData, rapidjson::Document &fields)
        {
            if(eventData.IsNull())
            {
                return;
            }

            if (fields.IsObject() && fields.MemberCount() > 0)
            {
                rapidjson::Value v(rapidjson::kObjectType);
                v.CopyFrom(fields, fields.GetAllocator());
                eventData.AddMember("custom_fields", v.Move(), eventData.GetAllocator());
            }
        }

        void GAEvents::progressionStatusString(EGAProgressionStatus progressionStatus, char* out)
        {
            switch (progressionStatus) {
            case Start:
                snprintf(out, 10, "%s", "Start");
                return;
            case Complete:
                snprintf(out, 10, "%s", "Complete");
                return;
            case Fail:
                snprintf(out, 10, "%s", "Fail");
                return;
            default:
                break;
            }

            snprintf(out, 10, "%s", "");
        }

        void GAEvents::errorSeverityString(EGAErrorSeverity errorSeverity, char* out)
        {
            switch (errorSeverity) {
            case Info:
                snprintf(out, 10, "%s", "info");
                return;
            case Debug:
                snprintf(out, 10, "%s", "debug");
                return;
            case Warning:
                snprintf(out, 10, "%s", "warning");
                return;
            case Error:
                snprintf(out, 10, "%s", "error");
                return;
            case Critical:
                snprintf(out, 10, "%s", "critical");
                return;
            default:
                break;
            }

            snprintf(out, 10, "%s", "");
        }

        void GAEvents::resourceFlowTypeString(EGAResourceFlowType flowType, char* out)
        {
            switch (flowType) {
            case Source:
                snprintf(out, 10, "%s", "Source");
                return;
            case Sink:
                snprintf(out, 10, "%s", "Sink");
                return;
            default:
                break;
            }

            snprintf(out, 10, "%s", "");
        }
    }
}
