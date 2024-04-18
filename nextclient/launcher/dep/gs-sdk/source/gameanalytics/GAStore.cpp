//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#include "GAStore.h"
#include "GADevice.h"
#include "GAThreading.h"
#include "GALogger.h"
#include "GAUtilities.h"
#include <fstream>
#include <string.h>
#if USE_UWP
#elif USE_TIZEN
#elif _WIN32
#include <direct.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace gameanalytics
{
    namespace store
    {
        const int GAStore::MaxDbSizeBytes = 6291456;
        const int GAStore::MaxDbSizeBytesBeforeTrim = 5242880;

        bool GAStore::_destroyed = false;
        GAStore* GAStore::_instance = 0;
        std::once_flag GAStore::_initInstanceFlag;

        GAStore::GAStore()
        {
        }

        void GAStore::cleanUp()
        {
            delete _instance;
            _instance = 0;
            _destroyed = true;
            threading::GAThreading::endThread();
        }

        GAStore* GAStore::getInstance()
        {
            std::call_once(_initInstanceFlag, &GAStore::initInstance);
            return _instance;
        }

        bool GAStore::isDestroyed()
        {
            return _destroyed;
        }

        bool GAStore::executeQuerySync(const char* sql)
        {
            rapidjson::Document d;
            executeQuerySync(sql, d);
            return !d.IsNull();
        }

        void GAStore::executeQuerySync(const char* sql, rapidjson::Document& out)
        {
            executeQuerySync(sql, {}, 0, out);
        }


        void GAStore::executeQuerySync(const char* sql, const char* parameters[], size_t size)
        {
            rapidjson::Document d;
            executeQuerySync(sql, parameters, size, false, d);
        }

        void GAStore::executeQuerySync(const char* sql, const char* parameters[], size_t size, rapidjson::Document& out)
        {
            executeQuerySync(sql, parameters, size, false, out);
        }

        void GAStore::executeQuerySync(const char* sql, const char* parameters[], size_t size, bool useTransaction)
        {
            rapidjson::Document d;
            executeQuerySync(sql, parameters, size, useTransaction, d);
        }

        void GAStore::executeQuerySync(const char* sql, const char* parameters[], size_t size, bool useTransaction, rapidjson::Document& out)
        {
            GAStore* i = getInstance();
            if(!i)
            {
                return;
            }
            // Force transaction if it is an update, insert or delete.
            size_t arraySize = strlen(sql) + 1;
            char* sqlUpper = new char[arraySize];
            snprintf(sqlUpper, arraySize, "%s", sql);
            utilities::GAUtilities::uppercaseString(sqlUpper);
            if (utilities::GAUtilities::stringMatch(sqlUpper, "^(UPDATE|INSERT|DELETE)"))
            {
                useTransaction = true;
            }
            delete[] sqlUpper;

            // Get database connection from singelton getInstance
            sqlite3 *sqlDatabasePtr = i->getDatabase();

            // Create mutable array for results
            out.SetArray();
            rapidjson::Document::AllocatorType& allocator = out.GetAllocator();

            if (useTransaction)
            {
                if (sqlite3_exec(sqlDatabasePtr, "BEGIN;", 0, 0, 0) != SQLITE_OK)
                {
                    logging::GALogger::e("SQLITE3 BEGIN ERROR: %s", sqlite3_errmsg(sqlDatabasePtr));
                    out.SetNull();
                    return;
                }
            }

            // Create statement
            sqlite3_stmt *statement;

            // Prepare statement
            if (sqlite3_prepare_v2(sqlDatabasePtr, sql, -1, &statement, nullptr) == SQLITE_OK)
            {
                // Bind parameters
                if (size > 0)
                {
                    for (size_t index = 0; index < size; index++)
                    {
                        sqlite3_bind_text(statement, static_cast<int>(index + 1), parameters[index], -1, 0);
                    }
                }

                // get columns count
                int columnCount = sqlite3_column_count(statement);

                // Loop through results
                while (sqlite3_step(statement) == SQLITE_ROW)
                {
                    rapidjson::Value row(rapidjson::kObjectType);
                    for (int i = 0; i < columnCount; i++)
                    {
                        const char *column = (const char *)sqlite3_column_name(statement, i);
                        const char *value = (const char *)sqlite3_column_text(statement, i);

                        if (!column || !value)
                        {
                            continue;
                        }

                        switch (sqlite3_column_type(statement, i))
                        {
                            case SQLITE_INTEGER:
                            {
                                rapidjson::Value v(column, allocator);
                                row.AddMember(v.Move(), (int)strtol(value, NULL, 10), allocator);
                                break;
                            }
                            case SQLITE_FLOAT:
                            {
                                rapidjson::Value v(column, allocator);
                                double d;
                                sscanf(value, "%lf", &d);
                                row.AddMember(v.Move(), d, allocator);
                                break;
                            }
                            default:
                            {
                                rapidjson::Value v(column, allocator);
                                rapidjson::Value v1(value, allocator);
                                row.AddMember(v.Move(), v1.Move(), allocator);
                            }
                        }

                        //row[column] = value;
                    }
                    out.PushBack(row, allocator);
                }
            }
            else
            {
                // TODO(nikolaj): Should we do a db validation to see if the db is corrupt here?
                logging::GALogger::e("SQLITE3 PREPARE ERROR: %s", sqlite3_errmsg(sqlDatabasePtr));
                out.SetNull();
                return;
            }

            // Destroy statement
            if (sqlite3_finalize(statement) == SQLITE_OK)
            {
                if (useTransaction)
                {
                    if (sqlite3_exec(sqlDatabasePtr, "COMMIT", 0, 0, 0) != SQLITE_OK)
                    {
                        logging::GALogger::e("SQLITE3 COMMIT ERROR: %s", sqlite3_errmsg(sqlDatabasePtr));
                        out.SetNull();
                        return;
                    }
                }
            }
            else
            {
                logging::GALogger::d("SQLITE3 FINALIZE ERROR: %s", sqlite3_errmsg(sqlDatabasePtr));

                out.Clear();
                if (useTransaction)
                {
                    if (sqlite3_exec(sqlDatabasePtr, "ROLLBACK", 0, 0, 0) != SQLITE_OK)
                    {
                        logging::GALogger::e("SQLITE3 ROLLBACK ERROR: %s", sqlite3_errmsg(sqlDatabasePtr));
                    }
                }
                out.SetNull();
                return;
            }
        }

        sqlite3* GAStore::getDatabase()
        {
            return sqlDatabase;
        }

        bool GAStore::ensureDatabase(bool dropDatabase, const char* key)
        {
            GAStore* i = getInstance();
            if(!i)
            {
                return false;
            }
            // lazy creation of db path
            if(strlen(i->dbPath) == 0)
            {
#if USE_UWP
                snprintf(i->dbPath, sizeof(i->dbPath), "%s\\ga.sqlite3", device::GADevice::getWritablePath());
#elif USE_TIZEN
                snprintf(i->dbPath, sizeof(i->dbPath), "%s%sga.sqlite3", device::GADevice::getWritablePath(), utilities::GAUtilities::getPathSeparator());
#else
                char d[513] = "";
                const char* writablepath = device::GADevice::getWritablePath();

                if(device::GADevice::getWritablePathStatus() <= 0)
                {
                    return false;
                }
                snprintf(d, sizeof(d), "%s%s%s", writablepath, utilities::GAUtilities::getPathSeparator(), key);
#ifdef _WIN32
                _mkdir(d);
#else
                mode_t nMode = 0733;
                mkdir(d,nMode);
#endif
                snprintf(i->dbPath, sizeof(i->dbPath), "%s%sga.sqlite3", d, utilities::GAUtilities::getPathSeparator());
#endif
            }

            // Open database
            if (sqlite3_open(i->dbPath, &i->sqlDatabase) != SQLITE_OK)
            {
                i->dbReady = false;
                logging::GALogger::w("Could not open database: %s", i->dbPath);
                return false;
            }
            else
            {
                i->dbReady = true;
                logging::GALogger::i("Database opened: %s", i->dbPath);
            }

            if (dropDatabase)
            {
                logging::GALogger::d("Drop tables");
                GAStore::executeQuerySync("DROP TABLE ga_events");
                GAStore::executeQuerySync("DROP TABLE ga_state");
                GAStore::executeQuerySync("DROP TABLE ga_session");
                GAStore::executeQuerySync("DROP TABLE ga_progression");
                GAStore::executeQuerySync("VACUUM");
            }

            // Create statements
            const char* sql_ga_events = "CREATE TABLE IF NOT EXISTS ga_events(status CHAR(50) NOT NULL, category CHAR(50) NOT NULL, session_id CHAR(50) NOT NULL, client_ts CHAR(50) NOT NULL, event TEXT NOT NULL);";
            const char* sql_ga_session = "CREATE TABLE IF NOT EXISTS ga_session(session_id CHAR(50) PRIMARY KEY NOT NULL, timestamp CHAR(50) NOT NULL, event TEXT NOT NULL);";
            const char* sql_ga_state = "CREATE TABLE IF NOT EXISTS ga_state(key CHAR(255) PRIMARY KEY NOT NULL, value TEXT);";
            const char* sql_ga_progression = "CREATE TABLE IF NOT EXISTS ga_progression(progression CHAR(255) PRIMARY KEY NOT NULL, tries CHAR(255));";

            if (!GAStore::executeQuerySync(sql_ga_events))
            {
                logging::GALogger::d("ensureDatabase failed: %s", sql_ga_events);
                return false;
            }

            if (!GAStore::executeQuerySync("SELECT status FROM ga_events LIMIT 0,1"))
            {
                logging::GALogger::d("ga_events corrupt, recreating.");
                GAStore::executeQuerySync("DROP TABLE ga_events");
                if (!GAStore::executeQuerySync(sql_ga_events))
                {
                    logging::GALogger::w("ga_events corrupt, could not recreate it.");
                    return false;
                }
            }

            if (!GAStore::executeQuerySync(sql_ga_session))
            {
                return false;
            }

            if (!GAStore::executeQuerySync("SELECT session_id FROM ga_session LIMIT 0,1"))
            {
                logging::GALogger::d("ga_session corrupt, recreating.");
                GAStore::executeQuerySync("DROP TABLE ga_session");
                if (!GAStore::executeQuerySync(sql_ga_session))
                {
                    logging::GALogger::w("ga_session corrupt, could not recreate it.");
                    return false;
                }
            }

            if (!GAStore::executeQuerySync(sql_ga_state))
            {
                return false;
            }

            if (!GAStore::executeQuerySync("SELECT key FROM ga_state LIMIT 0,1"))
            {
                logging::GALogger::d("ga_state corrupt, recreating.");
                GAStore::executeQuerySync("DROP TABLE ga_state");
                if (!GAStore::executeQuerySync(sql_ga_state))
                {
                    logging::GALogger::w("ga_state corrupt, could not recreate it.");
                    return false;
                }
            }

            if (!GAStore::executeQuerySync(sql_ga_progression))
            {
                return false;
            }

            if (!GAStore::executeQuerySync("SELECT progression FROM ga_progression LIMIT 0,1"))
            {
                logging::GALogger::d("ga_progression corrupt, recreating.");
                GAStore::executeQuerySync("DROP TABLE ga_progression");
                if (!GAStore::executeQuerySync(sql_ga_progression))
                {
                    logging::GALogger::w("ga_progression corrupt, could not recreate it.");
                    return false;
                }
            }

            trimEventTable();

            i->tableReady = true;
            logging::GALogger::d("Database tables ensured present");

            return true;
        }

        void GAStore::setState(const char* key, const char* value)
        {
            if (strlen(value) == 0)
            {
                const char* parameterArray[1] = {key};
                executeQuerySync("DELETE FROM ga_state WHERE key = ?;", parameterArray, 1);
            }
            else
            {
                const char* parameterArray[2] = {key, value};
                executeQuerySync("INSERT OR REPLACE INTO ga_state (key, value) VALUES(?, ?);", parameterArray, 2, true);
            }
        }

        // long long is C 64 bit int
        long long GAStore::getDbSizeBytes()
        {
            std::ifstream in(getInstance()->dbPath, std::ifstream::ate | std::ifstream::binary);
            return in.tellg();
        }

        bool GAStore::getTableReady()
        {
            GAStore* i = GAStore::getInstance();
            if(!i)
            {
                return false;
            }
            return i->tableReady;
        }

        bool GAStore::isDbTooLargeForEvents()
        {
            return getDbSizeBytes() > MaxDbSizeBytes;
        }


        bool GAStore::trimEventTable()
        {
            if(getDbSizeBytes() > MaxDbSizeBytesBeforeTrim)
            {
                rapidjson::Document resultSessionArray;
                executeQuerySync("SELECT session_id, Max(client_ts) FROM ga_events GROUP BY session_id ORDER BY client_ts LIMIT 3", resultSessionArray);

                if(!resultSessionArray.IsNull() && resultSessionArray.Size() > 0)
                {
                    char sessionDeleteString[257] = "";

                    unsigned int i = 0;
                    for (rapidjson::Value::ConstValueIterator itr = resultSessionArray.Begin(); itr != resultSessionArray.End(); ++itr)
                    {
                        const rapidjson::Value& result = *itr;
                        const char* session_id = result.GetString();
                        char tmp[257] = "";

                        if(i < resultSessionArray.Size() - 1)
                        {
                            snprintf(tmp, 257, "%s%s%s", sessionDeleteString, session_id, ",");
                        }
                        else
                        {
                            snprintf(tmp, 257, "%s%s", sessionDeleteString, session_id);
                        }
                        snprintf(sessionDeleteString, 257, "%s", tmp);
                        ++i;
                    }

                    char deleteOldSessionsSql[513] = "";
                    snprintf(deleteOldSessionsSql, 513, "DELETE FROM ga_events WHERE session_id IN (\"%s\");", sessionDeleteString);
                    logging::GALogger::w("Database too large when initializing. Deleting the oldest 3 sessions.");
                    executeQuerySync(deleteOldSessionsSql);
                    executeQuerySync("VACUUM");

                    return true;
                }
                else
                {
                    return false;
                }
            }

            return true;
        }

    }
}
