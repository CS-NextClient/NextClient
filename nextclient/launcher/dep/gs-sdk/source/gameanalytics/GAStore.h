//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#pragma once

#include <sqlite3.h>
#include <vector>
#include "rapidjson/document.h"
#include "GameAnalytics.h"
#include <mutex>
#include <cstdlib>

namespace gameanalytics
{
    namespace store
    {
        class GAStore
        {
         public:

            sqlite3* getDatabase();
            static bool isDestroyed();

            static bool ensureDatabase(bool dropDatabase, const char* key = "");

            static void setState(const char* key, const char* value);

            static bool executeQuerySync(const char* sql);
            static void executeQuerySync(const char* sql, rapidjson::Document& out);

            static void executeQuerySync(const char* sql, const char* parameters[], size_t size);
            static void executeQuerySync(const char* sql, const char* parameters[], size_t size, rapidjson::Document& out);

            static void executeQuerySync(const char* sql, const char* parameters[], size_t size, bool useTransaction);
            static void executeQuerySync(const char* sql, const char* parameters[], size_t size, bool useTransaction, rapidjson::Document& out);

            static long long getDbSizeBytes();

            static bool getTableReady();
            static bool isDbTooLargeForEvents();

        private:
            GAStore();
            GAStore(const GAStore&) = delete;
            GAStore& operator=(const GAStore&) = delete;

            static bool _destroyed;
            static GAStore* _instance;
            static std::once_flag _initInstanceFlag;
            static void cleanUp();
            static GAStore* getInstance();

            static void initInstance()
            {
                if(!_destroyed && !_instance)
                {
                    _instance = new GAStore();
                    std::atexit(&cleanUp);
                }
            }

            static bool trimEventTable();

            // set when calling "ensureDatabase"
            // using a "writablePath" that needs to be set into the C++ component before
            char dbPath[513] = {'\0'};

            // local pointer to database
            sqlite3* sqlDatabase = nullptr;

            // 10 MB limit for database. Will initiate trim logic when exceeded.
            // long maxDbSizeBytes = 10485760;

            // ??
            bool dbReady = false;
            // bool to determine if tables are ensured ready
            bool tableReady = false;

            static const int MaxDbSizeBytes;
            static const int MaxDbSizeBytesBeforeTrim;
        };
    }
}
