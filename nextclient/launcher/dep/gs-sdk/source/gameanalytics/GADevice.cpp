//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#if defined(__linux__) || defined(__unix__) || defined(__unix) || defined(unix)
#define IS_LINUX 1
#else
#define IS_LINUX 0
#endif

#if defined(__MACH__) || defined(__APPLE__)
#define IS_MAC 1
#else
#define IS_MAC 0
#endif

#if _WIN32
#define _WIN32_DCOM
#endif

#include "GADevice.h"
#include "GAUtilities.h"
#include <string.h>
#include <stdio.h>
#if USE_UWP
#include <Windows.h>
#include <sstream>
#elif USE_TIZEN
#include <system_info.h>
#include <app_common.h>
#include <algorithm>
#elif _WIN32
#include <direct.h>
#include <windows.h>
#include <VersionHelpers.h>
#include <cerrno>
#if !GA_SHARED_LIB
#include <comdef.h>
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#endif
#else
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <cerrno>
#endif
#if IS_MAC
#include "GADeviceOSX.h"
#include <sys/sysctl.h>
#elif IS_LINUX
#include <sys/utsname.h>
#include <cctype>
#endif

namespace gameanalytics
{
    namespace device
    {
        bool GADevice::_useDeviceInfo = true;
        char GADevice::_writablepath[MAX_PATH_LENGTH] = "";
        int GADevice::_writablepathStatus = 0;
        char GADevice::_buildPlatform[33] = "";
        char GADevice::_osVersion[65] = "";
        char GADevice::_deviceModel[129] = "";
#if USE_UWP
        const std::string GADevice::_advertisingId = utilities::GAUtilities::ws2s(Windows::System::UserProfile::AdvertisingManager::AdvertisingId->Data());
        const std::string GADevice::_deviceId = GADevice::deviceId();
#elif USE_TIZEN
        char GADevice::_deviceId[129] = "GADevice::deviceId()";
#endif
        char GADevice::_deviceManufacturer[129] = "";
        char GADevice::_sdkGameEngineVersion[33] = "";
        char GADevice::_gameEngineVersion[33] = "";
        char GADevice::_connectionType[33] = "";
#if USE_UWP
        const char* GADevice::_sdkWrapperVersion = "uwp_cpp 3.2.6";
#elif USE_TIZEN
        const char* GADevice::_sdkWrapperVersion = "tizen 3.2.6";
#else
        const char* GADevice::_sdkWrapperVersion = "cpp 3.2.6";
#endif

        void GADevice::disableDeviceInfo()
        {
            GADevice::_useDeviceInfo = false;
        }

        void GADevice::setSdkGameEngineVersion(const char* sdkGameEngineVersion)
        {
            snprintf(GADevice::_sdkGameEngineVersion, sizeof(GADevice::_sdkGameEngineVersion), "%s", sdkGameEngineVersion);
        }

        const char* GADevice::getGameEngineVersion()
        {
            return GADevice::_gameEngineVersion;
        }

        void GADevice::setGameEngineVersion(const char* gameEngineVersion)
        {
            snprintf(GADevice::_gameEngineVersion, sizeof(GADevice::_gameEngineVersion), "%s", gameEngineVersion);
        }

        void GADevice::setConnectionType(const char* connectionType)
        {
            snprintf(GADevice::_connectionType, sizeof(GADevice::_connectionType), "%s", connectionType);
        }

        const char* GADevice::getConnectionType()
        {
            return GADevice::_connectionType;
        }

        const char* GADevice::getRelevantSdkVersion()
        {
            if(strlen(GADevice::_sdkGameEngineVersion) > 0)
            {
                return GADevice::_sdkGameEngineVersion;
            }

            return GADevice::_sdkWrapperVersion;
        }

        const char* GADevice::getBuildPlatform()
        {
            if(strlen(GADevice::_buildPlatform) == 0)
            {
                initRuntimePlatform();
            }
            return GADevice::_buildPlatform;
        }

        void GADevice::setBuildPlatform(const char* platform)
        {
            snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "%s", platform);
        }

        const char* GADevice::getOSVersion()
        {
            if(strlen(GADevice::_osVersion) == 0)
            {
                initOSVersion();
            }
            return GADevice::_osVersion;
        }

        void GADevice::setDeviceModel(const char* deviceModel)
        {
            if(strlen(GADevice::_deviceModel) == 0)
            {
                snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "%s", "unknown");
            }
            else
            {
                snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "%s", deviceModel);
            }
        }

        const char* GADevice::getDeviceModel()
        {
            if(strlen(GADevice::_deviceModel) == 0)
            {
                initDeviceModel();
            }
            return GADevice::_deviceModel;
        }

        void GADevice::setDeviceManufacturer(const char* deviceManufacturer)
        {
            if(strlen(GADevice::_deviceModel) == 0)
            {
                snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "%s", "unknown");
            }
            else
            {
                snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "%s", deviceManufacturer);
            }
        }

        const char* GADevice::getDeviceManufacturer()
        {
            if(strlen(GADevice::_deviceModel) == 0)
            {
                initDeviceManufacturer();
            }
            return GADevice::_deviceManufacturer;
        }

        void GADevice::setWritablePath(const char* writablepath)
        {
            snprintf(GADevice::_writablepath, sizeof(GADevice::_writablepath), "%s", writablepath);

#if USE_UWP
#elif USE_TIZEN
#else
#ifdef _WIN32
            int result = _mkdir(GADevice::_writablepath);
            if(result == 0 || errno == EEXIST)
            {
                GADevice::_writablepathStatus = 1;
            }
            else
            {
                GADevice::_writablepathStatus = -1;
            }
#else
            mode_t nMode = 0733;
            int result = mkdir(GADevice::_writablepath, nMode);
            if(result == 0 || errno == EEXIST)
            {
                GADevice::_writablepathStatus = 1;
            }
            else
            {
                GADevice::_writablepathStatus = -1;
            }
#endif
#endif
        }

        const char* GADevice::getWritablePath()
        {
            if(GADevice::_writablepathStatus == 0 && strlen(GADevice::_writablepath) == 0)
            {
                initPersistentPath();
            }
            return GADevice::_writablepath;
        }

        int GADevice::getWritablePathStatus()
        {
            return GADevice::_writablepathStatus;
        }

        void GADevice::UpdateConnectionType()
        {
            snprintf(GADevice::_connectionType, sizeof(GADevice::_connectionType), "%s", "lan");
        }

        void GADevice::initOSVersion()
        {
#if USE_UWP
            auto deviceFamilyVersion = Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamilyVersion;
            std::istringstream stringStream(utilities::GAUtilities::ws2s(deviceFamilyVersion->Data()));
            unsigned long long version;

            stringStream >> version;
            unsigned long long major = (version & 0xFFFF000000000000L) >> 48;
            unsigned long long minor = (version & 0x0000FFFF00000000L) >> 32;
            unsigned long long build = (version & 0x00000000FFFF0000L) >> 16;
            std::ostringstream stream;
            stream << getBuildPlatform() << " " << major << "." << minor << "." << build;
            snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s", stream.str().c_str());
#elif USE_TIZEN
            char *value;
            int ret;
            ret = system_info_get_platform_string("http://tizen.org/feature/platform.version", &value);
            if (ret == SYSTEM_INFO_ERROR_NONE)
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s %s", GADevice::getBuildPlatform(), value);
            }
            else
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s 0.0.0", GADevice::getBuildPlatform());
            }
#else
#ifdef _WIN32
#if (_MSC_VER == 1900)
            if (IsWindows10OrGreater())
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s 10.0", GADevice::getBuildPlatform());
            }
            else
#endif
            if (IsWindows8Point1OrGreater())
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s 6.3", GADevice::getBuildPlatform());
            }
            else if (IsWindows8OrGreater())
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s 6.2", GADevice::getBuildPlatform());
            }
            else if (IsWindows7OrGreater())
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s 6.1", GADevice::getBuildPlatform());
            }
            else if (IsWindowsVistaOrGreater())
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s 6.1", GADevice::getBuildPlatform());
            }
            else if (IsWindowsXPOrGreater())
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s 5.1", GADevice::getBuildPlatform());
            }
            else
            {
                snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s 0.0.0", GADevice::getBuildPlatform());
            }
#elif IS_MAC
            snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s %s", GADevice::getBuildPlatform(), getOSXVersion());
#elif IS_LINUX
            struct utsname info;
            uname(&info);

            std::size_t i;
            char v[65] = "";
            snprintf(v, sizeof(v), "%s", info.release);

            for(i = 0; i < strlen(info.release); ++i)
            {
                if(!isdigit(v[i]) && v[i] != '.')
                {
                    snprintf(v, i, "%s", info.release);
                    break;
                }
            }

            snprintf(GADevice::_osVersion, sizeof(GADevice::_osVersion), "%s %s", GADevice::getBuildPlatform(), v);
#else
            return GADevice::getBuildPlatform() + " 0.0.0";
#endif
#endif
        }

        void GADevice::initDeviceManufacturer()
        {
            if(!GADevice::_useDeviceInfo)
            {
                snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "unknown");
                return;
            }

#if USE_UWP
            auto info = ref new Windows::Security::ExchangeActiveSyncProvisioning::EasClientDeviceInformation();
            snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "%s", utilities::GAUtilities::ws2s(info->SystemManufacturer->Data()).c_str());
#elif USE_TIZEN
            char *value;
            int ret;
            ret = system_info_get_platform_string("http://tizen.org/system/manufacturer", &value);
            if (ret == SYSTEM_INFO_ERROR_NONE)
            {
                snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "%s", value);
            }
            else
            {
                snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "unknown");
            }
#else
#if defined(_WIN32) && !GA_SHARED_LIB
            IWbemLocator *locator = nullptr;
            IWbemServices *services = nullptr;
            auto hResult = CoInitializeEx(0, COINIT_MULTITHREADED);
            if (FAILED(hResult))
            {
                snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "unknown");
            }
            hResult = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&locator);

            auto hasFailed = [&hResult]() {
                if (FAILED(hResult))
                {
                    return true;
                }
                return false;
            };

            auto getValue = [&hResult, &hasFailed](IWbemClassObject *classObject, LPCWSTR property) {
                BSTR propertyValueText = L"unknown";
                VARIANT propertyValue;
                hResult = classObject->Get(property, 0, &propertyValue, 0, 0);
                if (!hasFailed()) {
                    if ((propertyValue.vt == VT_NULL) || (propertyValue.vt == VT_EMPTY)) {
                    }
                    else if (propertyValue.vt & VT_ARRAY) {
                        propertyValueText = L"unknown"; //Array types not supported
                    }
                    else {
                        propertyValueText = propertyValue.bstrVal;
                    }
                }
                VariantClear(&propertyValue);
                return propertyValueText;
            };

            BSTR manufacturer = L"unknown";
            if (!hasFailed()) {
                // Connect to the root\cimv2 namespace with the current user and obtain pointer pSvc to make IWbemServices calls.
                hResult = locator->ConnectServer(L"ROOT\\CIMV2", nullptr, nullptr, 0, NULL, 0, 0, &services);

                if (!hasFailed()) {
                    // Set the IWbemServices proxy so that impersonation of the user (client) occurs.
                    hResult = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

                    if (!hasFailed()) {
                        IEnumWbemClassObject* classObjectEnumerator = nullptr;
                        hResult = services->ExecQuery(L"WQL", L"SELECT * FROM Win32_ComputerSystem", WBEM_FLAG_FORWARD_ONLY |
                            WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &classObjectEnumerator);
                        if (!hasFailed()) {
                            IWbemClassObject *classObject;
                            ULONG uReturn = 0;
                            hResult = classObjectEnumerator->Next(WBEM_INFINITE, 1, &classObject, &uReturn);
                            if (uReturn != 0) {
                                manufacturer = getValue(classObject, (LPCWSTR)L"Manufacturer");
                            }
                            classObject->Release();
                        }
                        classObjectEnumerator->Release();
                    }
                }
            }

            if (locator) {
                locator->Release();
            }
            if (services) {
                services->Release();
            }
            CoUninitialize();

            snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "%s", _com_util::ConvertBSTRToString(manufacturer));
#elif IS_MAC
            snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "%s", "Apple");
#elif IS_LINUX
            snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "%s", "unknown");
#else
            snprintf(GADevice::_deviceManufacturer, sizeof(GADevice::_deviceManufacturer), "%s", "unknown");
#endif
#endif
        }

        void GADevice::initDeviceModel()
        {
            if(!GADevice::_useDeviceInfo)
            {
                snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "unknown");
                return;
            }

#if USE_UWP
            auto info = ref new Windows::Security::ExchangeActiveSyncProvisioning::EasClientDeviceInformation();
            snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "%s", utilities::GAUtilities::ws2s(info->SystemProductName->Data()).c_str());
#elif USE_TIZEN
            char *value;
            int ret;
            ret = system_info_get_platform_string("http://tizen.org/system/model_name", &value);
            if (ret == SYSTEM_INFO_ERROR_NONE)
            {
                snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "%s", value);
            }
            else
            {
                snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "unknown");
            }
#else
#if defined(_WIN32) && !GA_SHARED_LIB
            IWbemLocator *locator = nullptr;
            IWbemServices *services = nullptr;
            auto hResult = CoInitializeEx(0, COINIT_MULTITHREADED);
            if (FAILED(hResult))
            {
                snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "unknown");
            }
            hResult = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&locator);

            auto hasFailed = [&hResult]() {
                if (FAILED(hResult))
                {
                    return true;
                }
                return false;
            };

            auto getValue = [&hResult, &hasFailed](IWbemClassObject *classObject, LPCWSTR property) {
                BSTR propertyValueText = L"unknown";
                VARIANT propertyValue;
                hResult = classObject->Get(property, 0, &propertyValue, 0, 0);
                if (!hasFailed()) {
                    if ((propertyValue.vt == VT_NULL) || (propertyValue.vt == VT_EMPTY)) {
                    }
                    else if (propertyValue.vt & VT_ARRAY) {
                        propertyValueText = L"unknown"; //Array types not supported
                    }
                    else {
                        propertyValueText = propertyValue.bstrVal;
                    }
                }
                VariantClear(&propertyValue);
                return propertyValueText;
            };

            BSTR model = L"unknown";
            if (!hasFailed()) {
                // Connect to the root\cimv2 namespace with the current user and obtain pointer pSvc to make IWbemServices calls.
                hResult = locator->ConnectServer(L"ROOT\\CIMV2", nullptr, nullptr, 0, NULL, 0, 0, &services);

                if (!hasFailed()) {
                    // Set the IWbemServices proxy so that impersonation of the user (client) occurs.
                    hResult = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

                    if (!hasFailed()) {
                        IEnumWbemClassObject* classObjectEnumerator = nullptr;
                        hResult = services->ExecQuery(L"WQL", L"SELECT * FROM Win32_ComputerSystem", WBEM_FLAG_FORWARD_ONLY |
                            WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &classObjectEnumerator);
                        if (!hasFailed()) {
                            IWbemClassObject *classObject;
                            ULONG uReturn = 0;
                            hResult = classObjectEnumerator->Next(WBEM_INFINITE, 1, &classObject, &uReturn);
                            if (uReturn != 0) {
                                model = getValue(classObject, (LPCWSTR)L"Model");
                            }
                            classObject->Release();
                        }
                        classObjectEnumerator->Release();
                    }
                }
            }

            if (locator) {
                locator->Release();
            }
            if (services) {
                services->Release();
            }
            CoUninitialize();

            snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "%s", _com_util::ConvertBSTRToString(model));
#elif IS_MAC
            size_t len = 0;
            sysctlbyname("hw.model", NULL, &len, NULL, 0);

            char* model = (char*)malloc(len + 1);
            memset(model, 0, len + 1);

            sysctlbyname("hw.model", model, &len, NULL, 0);

            snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "%s", model);
            free(model);
#elif IS_LINUX
            snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "unknown");
#else
            snprintf(GADevice::_deviceModel, sizeof(GADevice::_deviceModel), "unknown");
#endif
#endif
        }

#if USE_UWP
        const const char* GADevice::getDeviceId()
        {
            return GADevice::_deviceId.c_str();
        }

        const const char* GADevice::getAdvertisingId()
        {
            return GADevice::_advertisingId.c_str();
        }

        const std::string GADevice::deviceId()
        {
            std::string result = "";

            if (Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.System.Profile.HardwareIdentification"))
            {
                auto token = Windows::System::Profile::HardwareIdentification::GetPackageSpecificToken(nullptr);
                auto hardwareId = token->Id;
                auto hardwareIdString = Windows::Security::Cryptography::CryptographicBuffer::EncodeToHexString(hardwareId);
                result = utilities::GAUtilities::ws2s(hardwareIdString->Data());
            }

            return result;
        }
#elif USE_TIZEN
        const char* GADevice::getDeviceId()
        {
            if(strlen(GADevice::_deviceId) == 0)
            {
                initDeviceId();
            }
            return GADevice::_deviceId;
        }

        void GADevice::initDeviceId()
        {
            std::string result = "";
            char *value;
            int ret;
            ret = system_info_get_platform_string("http://tizen.org/system/tizenid", &value);
            if (ret == SYSTEM_INFO_ERROR_NONE)
            {
                snprintf(GADevice::_deviceId, sizeof(GADevice::_deviceId), "%s", value);
            }
            else
            {
                snprintf(GADevice::_deviceId, sizeof(GADevice::_deviceId), "%s", "unknown");
            }
        }
#endif

        void GADevice::initRuntimePlatform()
        {
#if USE_UWP
            auto deviceFamily = Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily;

            if (deviceFamily == "Windows.Mobile")
            {
                snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "uwp_mobile");
            }
            else if (deviceFamily == "Windows.Desktop")
            {
                snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "uwp_desktop");
            }
            else if (deviceFamily == "Windows.Universal")
            {
                snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "uwp_iot");
            }
            else if (deviceFamily == "Windows.Xbox")
            {
                snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "uwp_console");
            }
            else if (deviceFamily == "Windows.Team")
            {
                snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "uwp_surfacehub");
            }
            else
            {
                snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "%s", utilities::GAUtilities::ws2s(deviceFamily->Data()).c_str());
            }
#elif USE_TIZEN
            std::string result = "tizen";
            char *value;
            int ret;
            ret = system_info_get_platform_string("http://tizen.org/system/platform.name", &value);
            if (ret == SYSTEM_INFO_ERROR_NONE)
            {
                utilities::GAUtilities::lowercaseString(value);
                snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), value);
            }
            else
            {
                snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "tizen");
            }
#else
#if IS_MAC
            snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "mac_osx");
#elif defined(_WIN32)
            snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "windows");
#elif IS_LINUX
            snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "linux");
#else
            snprintf(GADevice::_buildPlatform, sizeof(GADevice::_buildPlatform), "unknown");
#endif
#endif
        }

        void GADevice::initPersistentPath()
        {
#if USE_UWP
            std::string result = utilities::GAUtilities::ws2s(Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data()) + "\\GameAnalytics";
            snprintf(GADevice::_writablepath, sizeof(GADevice::_writablepath), "%s", result.c_str());
#elif USE_TIZEN
            snprintf(GADevice::_writablepath, sizeof(GADevice::_writablepath), "%s", app_get_data_path());
#else
#ifdef _WIN32
            snprintf(GADevice::_writablepath, sizeof(GADevice::_writablepath), "%s%sGameAnalytics", std::getenv("LOCALAPPDATA"), utilities::GAUtilities::getPathSeparator());
            int result = _mkdir(GADevice::_writablepath);
            if(result == 0 || errno == EEXIST)
            {
                GADevice::_writablepathStatus = 1;
            }
            else
            {
                GADevice::_writablepathStatus = -1;
            }
#else
            snprintf(GADevice::_writablepath, sizeof(GADevice::_writablepath), "%s%sGameAnalytics", std::getenv("HOME"), utilities::GAUtilities::getPathSeparator());
            mode_t nMode = 0733;
            int result = mkdir(GADevice::_writablepath, nMode);
            if(result == 0 || errno == EEXIST)
            {
                GADevice::_writablepathStatus = 1;
            }
            else
            {
                GADevice::_writablepathStatus = -1;
            }
#endif
#endif
        }
    }
}
