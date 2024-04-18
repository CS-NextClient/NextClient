//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#pragma once

#include "GameAnalytics.h"
#include "rapidjson/document.h"
#include <mutex>
#include <cstdlib>

namespace gameanalytics
{
    namespace events
    {
        class GAEvents
        {
         public:
            static void stopEventQueue();
            static void ensureEventQueueIsRunning();
            static void addSessionStartEvent();
            static void addSessionEndEvent();
            static void addBusinessEvent(const char* currency, int amount, const char* itemType, const char* itemId, const char* cartType, const rapidjson::Value& fields, bool mergeFields);
            static void addResourceEvent(EGAResourceFlowType flowType, const char* currency, double amount, const char* itemType, const char* itemId, const rapidjson::Value& fields, bool mergeFields);
            static void addProgressionEvent(EGAProgressionStatus progressionStatus, const char* progression01, const char* progression02, const char* progression03, int score, bool sendScore, const rapidjson::Value& fields, bool mergeFields);
            static void addDesignEvent(const char* eventId, double value, bool sendValue, const rapidjson::Value& fields, bool mergeFields);
            static void addErrorEvent(EGAErrorSeverity severity, const char* message, const rapidjson::Value& fields, bool mergeFields);
            static void addErrorEvent(EGAErrorSeverity severity, const char* message, const rapidjson::Value& fields, bool mergeFields, bool skipAddingFields);
            static void progressionStatusString(EGAProgressionStatus progressionStatus, char* out);
            static void errorSeverityString(EGAErrorSeverity errorSeverity, char* out);
            static void resourceFlowTypeString(EGAResourceFlowType flowType, char* out);
            static void processEvents(const char* category, bool performCleanUp);

        private:
            GAEvents();
            ~GAEvents();
            GAEvents(const GAEvents&) = delete;
            GAEvents& operator=(const GAEvents&) = delete;

            static void processEventQueue();
            static void cleanupEvents();
            static void fixMissingSessionEndEvents();
            static void addEventToStore(rapidjson::Document &eventData);
            static void addDimensionsToEvent(rapidjson::Document& eventData);
            static void addCustomFieldsToEvent(rapidjson::Document& eventData, rapidjson::Document& fields);
            static void updateSessionTime();

            static const char* CategorySessionStart;
            static const char* CategorySessionEnd;
            static const char* CategoryDesign;
            static const char* CategoryBusiness;
            static const char* CategoryProgression;
            static const char* CategoryResource;
            static const char* CategoryError;
            static const double ProcessEventsIntervalInSeconds;
            static const int MaxEventCount;

            static bool _destroyed;
            static GAEvents* _instance;
            static std::once_flag _initInstanceFlag;
            static void cleanUp();
            static GAEvents* getInstance();

            static void initInstance()
            {
                if(!_destroyed && !_instance)
                {
                    _instance = new GAEvents();
                    std::atexit(&cleanUp);
                }
            }

            bool isRunning;
            bool keepRunning;
        };
    }
}
