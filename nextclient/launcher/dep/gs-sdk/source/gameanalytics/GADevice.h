//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#pragma once

#if USE_UWP || USE_TIZEN
#include <string>
#endif

#if defined(__linux__) || defined(__unix__) || defined(__unix) || defined(unix)
#if !defined(MAX_PATH_LENGTH)
#define MAX_PATH_LENGTH 4096
#endif
#endif

#if defined(__MACH__) || defined(__APPLE__)
#if !defined(MAX_PATH_LENGTH)
#define MAX_PATH_LENGTH 1017
#endif
#endif

#if _WIN32
#if !defined(MAX_PATH_LENGTH)
#define MAX_PATH_LENGTH 261
#endif
#endif

namespace gameanalytics
{
    namespace device
    {
        class GADevice
        {
        public:
            static void disableDeviceInfo();
            static void setSdkGameEngineVersion(const char* sdkGameEngineVersion);
            static const char* getGameEngineVersion();
            static void setGameEngineVersion(const char* gameEngineVersion);
            static void setConnectionType(const char* connectionType);
            static const char* getConnectionType();
            static const char* getRelevantSdkVersion();
            static const char* getBuildPlatform();
            static void setBuildPlatform(const char* platform);
            static const char* getOSVersion();
            static void setDeviceModel(const char* deviceModel);
            static const char* getDeviceModel();
            static void setDeviceManufacturer(const char* deviceManufacturer);
            static const char* getDeviceManufacturer();
            static void setWritablePath(const char* writablePath);
            static const char* getWritablePath();
            static int getWritablePathStatus();
#if USE_UWP
            static const char* getDeviceId();
            static const char* getAdvertisingId();
#elif USE_TIZEN
            static const char* getDeviceId();
#endif
            static void UpdateConnectionType();

        private:
            static void initOSVersion();
            static void initDeviceManufacturer();
            static void initDeviceModel();
            static void initRuntimePlatform();
            static void initPersistentPath();
#if USE_UWP
            static const std::string deviceId();

            static const std::string _advertisingId;
            static const std::string _deviceId;
#elif USE_TIZEN
            static void initDeviceId();

            static char _deviceId[];
#endif

            static bool _useDeviceInfo;
            static char _buildPlatform[];
            static char _osVersion[];
            static char _deviceModel[];
            static char _deviceManufacturer[];
            static char _writablepath[];
            static int _writablepathStatus;
            static char _sdkGameEngineVersion[];
            static char _gameEngineVersion[];
            static char _connectionType[];
            static const char* _sdkWrapperVersion;
        };
    }
}
