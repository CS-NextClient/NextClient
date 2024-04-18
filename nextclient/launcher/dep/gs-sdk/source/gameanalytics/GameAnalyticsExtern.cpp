#if USE_TIZEN || GA_SHARED_LIB

#include "GameAnalytics.h"
#include "GAUtilities.h"
#include "rapidjson/document.h"
#include <vector>

void configureAvailableCustomDimensions01(const char *customDimensionsJson)
{
    gameanalytics::GameAnalytics::configureAvailableCustomDimensions01(customDimensionsJson);
}

void configureAvailableCustomDimensions02(const char *customDimensionsJson)
{
    gameanalytics::GameAnalytics::configureAvailableCustomDimensions02(customDimensionsJson);
}

void configureAvailableCustomDimensions03(const char *customDimensionsJson)
{
    gameanalytics::GameAnalytics::configureAvailableCustomDimensions03(customDimensionsJson);
}

void configureAvailableResourceCurrencies(const char *resourceCurrenciesJson)
{
    gameanalytics::GameAnalytics::configureAvailableResourceCurrencies(resourceCurrenciesJson);
}

void configureAvailableResourceItemTypes(const char *resourceItemTypesJson)
{
    gameanalytics::GameAnalytics::configureAvailableResourceItemTypes(resourceItemTypesJson);
}

void configureBuild(const char *build)
{
    gameanalytics::GameAnalytics::configureBuild(build);
}

void configureWritablePath(const char *writablePath)
{
    gameanalytics::GameAnalytics::configureWritablePath(writablePath);
}

void configureDeviceModel(const char *deviceModel)
{
    gameanalytics::GameAnalytics::configureDeviceModel(deviceModel);
}

void configureDeviceManufacturer(const char *deviceManufacturer)
{
    gameanalytics::GameAnalytics::configureDeviceManufacturer(deviceManufacturer);
}

// the version of SDK code used in an engine. Used for sdk_version field.
// !! if set then it will override the SdkWrapperVersion.
// example "unity 4.6.9"
void configureSdkGameEngineVersion(const char *sdkGameEngineVersion)
{
    gameanalytics::GameAnalytics::configureSdkGameEngineVersion(sdkGameEngineVersion);
}

// the version of the game engine (if used and version is available)
void configureGameEngineVersion(const char *engineVersion)
{
    gameanalytics::GameAnalytics::configureGameEngineVersion(engineVersion);
}

void configureUserId(const char *uId)
{
    gameanalytics::GameAnalytics::configureUserId(uId);
}

// initialize - starting SDK (need configuration before starting)
void initialize(const char *gameKey, const char *gameSecret)
{
    gameanalytics::GameAnalytics::initialize(gameKey, gameSecret);
}

// add events
void addBusinessEvent(const char *currency, double amount, const char *itemType, const char *itemId, const char *cartType, const char *fields, double mergeFields)
{
    gameanalytics::GameAnalytics::addBusinessEvent(currency, (int)amount, itemType, itemId, cartType, fields, mergeFields != 0.0);
}

void addBusinessEventJson(const char *jsonArgs)
{
    rapidjson::Document json;
    json.Parse(jsonArgs);

    if(json.IsArray() && json.Size() == 5)
    {
        gameanalytics::GameAnalytics::addBusinessEvent(json[0].GetString(), (int)(json[1].GetDouble()), json[2].GetString(), json[3].GetString(), json[4].GetString(), json[5].GetString());
    }
}

void addResourceEvent(double flowType, const char *currency, double amount, const char *itemType, const char *itemId, const char *fields, double mergeFields)
{
    int flowTypeInt = (int)flowType;
    gameanalytics::GameAnalytics::addResourceEvent((gameanalytics::EGAResourceFlowType)flowTypeInt, currency, (float)amount, itemType, itemId, fields, mergeFields != 0.0);
}

void addResourceEventJson(const char *jsonArgs)
{
    rapidjson::Document json;
    json.Parse(jsonArgs);

    if(json.IsArray() && json.Size() == 5)
    {
        int flowTypeInt = (int)(json[0].GetDouble());
        gameanalytics::GameAnalytics::addResourceEvent((gameanalytics::EGAResourceFlowType)flowTypeInt, json[1].GetString(), (float)(json[2].GetDouble()), json[3].GetString(), json[4].GetString(), json[5].GetString());
    }
}

void addProgressionEvent(double progressionStatus, const char *progression01, const char *progression02, const char *progression03, const char *fields, double mergeFields)
{
    int progressionStatusInt = (int)progressionStatus;
    gameanalytics::GameAnalytics::addProgressionEvent((gameanalytics::EGAProgressionStatus)progressionStatusInt, progression01, progression02, progression03, fields, mergeFields != 0.0);
}

void addProgressionEventJson(const char *jsonArgs)
{
    rapidjson::Document json;
    json.Parse(jsonArgs);

    if(json.IsArray() && json.Size() == 4)
    {
        int progressionStatusInt = (int)(json[0].GetDouble());
        gameanalytics::GameAnalytics::addProgressionEvent((gameanalytics::EGAProgressionStatus)progressionStatusInt, json[1].GetString(), json[2].GetString(), json[3].GetString(), json[4].GetString());
    }
}

void addProgressionEventWithScore(double progressionStatus, const char *progression01, const char *progression02, const char *progression03, double score, const char *fields, double mergeFields)
{
    int progressionStatusInt = (int)progressionStatus;
    gameanalytics::GameAnalytics::addProgressionEvent((gameanalytics::EGAProgressionStatus)progressionStatusInt, progression01, progression02, progression03, (int)score, fields, mergeFields != 0.0);
}

void addProgressionEventWithScoreJson(const char *jsonArgs)
{
    rapidjson::Document json;
    json.Parse(jsonArgs);

    if(json.IsArray() && json.Size() == 5)
    {
        int progressionStatusInt = (int)(json[0].GetDouble());
        gameanalytics::GameAnalytics::addProgressionEvent((gameanalytics::EGAProgressionStatus)progressionStatusInt, json[1].GetString(), json[2].GetString(), json[3].GetString(), (int)(json[4].GetDouble()), json[5].GetString());
    }
}

void addDesignEvent(const char *eventId, const char *fields, double mergeFields)
{
    gameanalytics::GameAnalytics::addDesignEvent(eventId, fields, mergeFields != 0.0);
}

void addDesignEventWithValue(const char *eventId, double value, const char *fields, double mergeFields)
{
    gameanalytics::GameAnalytics::addDesignEvent(eventId, value, fields, mergeFields != 0.0);
}

void addErrorEvent(double severity, const char *message, const char *fields, double mergeFields)
{
    int severityInt = (int)severity;
    gameanalytics::GameAnalytics::addErrorEvent((gameanalytics::EGAErrorSeverity)severityInt, message, fields, mergeFields != 0.0);
}

// set calls can be changed at any time (pre- and post-initialize)
// some calls only work after a configure is called (setCustomDimension)

void setEnabledInfoLog(double flag)
{
    gameanalytics::GameAnalytics::setEnabledInfoLog(flag != 0.0);
}

void setEnabledVerboseLog(double flag)
{
    gameanalytics::GameAnalytics::setEnabledVerboseLog(flag != 0.0);
}

void setEnabledManualSessionHandling(double flag)
{
    gameanalytics::GameAnalytics::setEnabledManualSessionHandling(flag != 0.0);
}

void setEnabledErrorReporting(double flag)
{
    gameanalytics::GameAnalytics::setEnabledErrorReporting(flag != 0.0);
}

void setEnabledEventSubmission(double flag)
{
    gameanalytics::GameAnalytics::setEnabledEventSubmission(flag != 0.0);
}

void setCustomDimension01(const char *dimension01)
{
    gameanalytics::GameAnalytics::setCustomDimension01(dimension01);
}

void setCustomDimension02(const char *dimension02)
{
    gameanalytics::GameAnalytics::setCustomDimension02(dimension02);
}

void setCustomDimension03(const char *dimension03)
{
    gameanalytics::GameAnalytics::setCustomDimension03(dimension03);
}

void setGlobalCustomEventFields(const char *customFields)
{
    gameanalytics::GameAnalytics::setGlobalCustomEventFields(customFields);
}

void gameAnalyticsStartSession()
{
    gameanalytics::GameAnalytics::startSession();
}

void gameAnalyticsEndSession()
{
    gameanalytics::GameAnalytics::endSession();
}

// game state changes
// will affect how session is started / ended
void onResume()
{
    gameanalytics::GameAnalytics::onResume();
}

void onSuspend()
{
    gameanalytics::GameAnalytics::onSuspend();
}

void onQuit()
{
    gameanalytics::GameAnalytics::onQuit();
}

const char* getRemoteConfigsValueAsString(const char *key)
{
    std::vector<char> returnValue = gameanalytics::GameAnalytics::getRemoteConfigsValueAsString(key);
    char* result = new char[returnValue.size()];
    snprintf(result, returnValue.size(), "%s", returnValue.data());
    return result;
}

const char* getRemoteConfigsValueAsStringWithDefaultValue(const char *key, const char *defaultValue)
{
    std::vector<char> returnValue = gameanalytics::GameAnalytics::getRemoteConfigsValueAsString(key, defaultValue);
    char* result = new char[returnValue.size()];
    snprintf(result, returnValue.size(), "%s", returnValue.data());
    return result;
}

double isRemoteConfigsReady()
{
    return gameanalytics::GameAnalytics::isRemoteConfigsReady() ? 1 : 0;
}

const char* getRemoteConfigsContentAsString()
{
    std::vector<char> returnValue = gameanalytics::GameAnalytics::getRemoteConfigsContentAsString();
    char* result = new char[returnValue.size()];
    snprintf(result, returnValue.size(), "%s", returnValue.data());
    return result;
}

const char* getABTestingId()
{
    std::vector<char> returnValue = gameanalytics::GameAnalytics::getABTestingId();
    char* result = new char[returnValue.size()];
    snprintf(result, returnValue.size(), "%s", returnValue.data());
    return result;
}

const char* getABTestingVariantId()
{
    std::vector<char> returnValue = gameanalytics::GameAnalytics::getABTestingVariantId();
    char* result = new char[returnValue.size()];
    snprintf(result, returnValue.size(), "%s", returnValue.data());
    return result;
}

#if USE_UWP
void configureAvailableCustomDimensions01UWP(const wchar_t *customDimensionsJson)
{
    gameanalytics::GameAnalytics::configureAvailableCustomDimensions01(gameanalytics::utilities::GAUtilities::ws2s(customDimensionsJson).c_str());
}

void configureAvailableCustomDimensions02UWP(const wchar_t *customDimensionsJson)
{
    gameanalytics::GameAnalytics::configureAvailableCustomDimensions02(gameanalytics::utilities::GAUtilities::ws2s(customDimensionsJson).c_str());
}

void configureAvailableCustomDimensions03UWP(const wchar_t *customDimensionsJson)
{
    gameanalytics::GameAnalytics::configureAvailableCustomDimensions03(gameanalytics::utilities::GAUtilities::ws2s(customDimensionsJson).c_str());
}

void configureAvailableResourceCurrenciesUWP(const wchar_t *resourceCurrenciesJson)
{
    gameanalytics::GameAnalytics::configureAvailableResourceCurrencies(gameanalytics::utilities::GAUtilities::ws2s(resourceCurrenciesJson).c_str());
}

void configureAvailableResourceItemTypesUWP(const wchar_t *resourceItemTypesJson)
{
    gameanalytics::GameAnalytics::configureAvailableResourceItemTypes(gameanalytics::utilities::GAUtilities::ws2s(resourceItemTypesJson).c_str());
}

void configureBuildUWP(const wchar_t *build)
{
    gameanalytics::GameAnalytics::configureBuild(build);
}

void configureWritablePathUWP(const wchar_t *writablePath)
{
    gameanalytics::GameAnalytics::configureWritablePath(writablePath);
}

void configureDeviceModelUWP(const wchar_t *deviceModel)
{
    gameanalytics::GameAnalytics::configureDeviceModel(deviceModel);
}

void configureDeviceManufacturerUWP(const wchar_t *deviceManufacturer)
{
    gameanalytics::GameAnalytics::configureDeviceManufacturer(deviceManufacturer);
}

void configureSdkGameEngineVersionUWP(const wchar_t *sdkGameEngineVersion)
{
    gameanalytics::GameAnalytics::configureSdkGameEngineVersion(sdkGameEngineVersion);
}

void configureGameEngineVersionUWP(const wchar_t *engineVersion)
{
    gameanalytics::GameAnalytics::configureGameEngineVersion(engineVersion);
}

void configureUserIdUWP(const wchar_t *uId)
{
    gameanalytics::GameAnalytics::configureUserId(uId);
}

void initializeUWP(const wchar_t *gameKey, const wchar_t *gameSecret)
{
    gameanalytics::GameAnalytics::initialize(gameKey, gameSecret);
}

void setCustomDimension01UWP(const wchar_t *dimension01)
{
    gameanalytics::GameAnalytics::setCustomDimension01(dimension01);
}

void setCustomDimension02UWP(const wchar_t *dimension02)
{
    gameanalytics::GameAnalytics::setCustomDimension02(dimension02);
}

void setCustomDimension03UWP(const wchar_t *dimension03)
{
    gameanalytics::GameAnalytics::setCustomDimension03(dimension03);
}

void setGlobalCustomEventFieldsUWP(const wchar_t *customFields)
{
    gameanalytics::GameAnalytics::setGlobalCustomEventFields(customFields);
}

void addBusinessEventUWP(const wchar_t *currency, double amount, const wchar_t *itemType, const wchar_t *itemId, const wchar_t *cartType, const wchar_t *fields, double mergeFields)
{
    gameanalytics::GameAnalytics::addBusinessEvent(currency, (int)amount, itemType, itemId, cartType, fields, mergeFields != 0.0);
}

void addResourceEventUWP(double flowType, const wchar_t *currency, double amount, const wchar_t *itemType, const wchar_t *itemId, const wchar_t *fields, double mergeFields)
{
    int flowTypeInt = (int)flowType;
    gameanalytics::GameAnalytics::addResourceEvent((gameanalytics::EGAResourceFlowType)flowTypeInt, currency, (float)amount, itemType, itemId, fields, mergeFields != 0.0);
}

void addProgressionEventUWP(double progressionStatus, const wchar_t *progression01, const wchar_t *progression02, const wchar_t *progression03, const wchar_t *fields, double mergeFields)
{
    int progressionStatusInt = (int)progressionStatus;
    gameanalytics::GameAnalytics::addProgressionEvent((gameanalytics::EGAProgressionStatus)progressionStatusInt, progression01, progression02, progression03, fields, mergeFields != 0.0);
}

void addProgressionEventWithScoreUWP(double progressionStatus, const wchar_t *progression01, const wchar_t *progression02, const wchar_t *progression03, double score, const wchar_t *fields, double mergeFields)
{
    int progressionStatusInt = (int)progressionStatus;
    gameanalytics::GameAnalytics::addProgressionEvent((gameanalytics::EGAProgressionStatus)progressionStatusInt, progression01, progression02, progression03, (int)score, fields, mergeFields != 0.0);
}

void addDesignEventUWP(const wchar_t *eventId, const wchar_t *fields, double mergeFields)
{
    gameanalytics::GameAnalytics::addDesignEvent(eventId, fields, mergeFields != 0.0);
}

void addDesignEventWithValueUWP(const wchar_t *eventId, double value, const wchar_t *fields, double mergeFields)
{
    gameanalytics::GameAnalytics::addDesignEvent(eventId, value, fields, mergeFields != 0.0);
}

void addErrorEventUWP(double severity, const wchar_t *message, const wchar_t *fields, double mergeFields)
{
    int severityInt = (int)severity;
    gameanalytics::GameAnalytics::addErrorEvent((gameanalytics::EGAErrorSeverity)severityInt, message, fields, mergeFields != 0.0);
}

void getRemoteConfigsValueAsStringWithDefaultValueUWP(const wchar_t *key, const wchar_t *defaultValue, wchar_t *out)
{
    std::string returnValue = gameanalytics::GameAnalytics::getRemoteConfigsValueAsString(gameanalytics::utilities::GAUtilities::ws2s(key).c_str(), gameanalytics::utilities::GAUtilities::ws2s(defaultValue).c_str()).data();
    std::wstring result = gameanalytics::utilities::GAUtilities::s2ws(returnValue);
    wcscpy_s(out, result.length() + 1, result.c_str());
}

void getRemoteConfigsContentAsStringUWP(wchar_t *out)
{
    std::string returnValue = gameanalytics::GameAnalytics::getRemoteConfigsContentAsString().data();
    std::wstring result = gameanalytics::utilities::GAUtilities::s2ws(returnValue);
    wcscpy_s(out, result.length() + 1, result.c_str());
}

void getABTestingIdUWP(wchar_t *out)
{
    std::string returnValue = gameanalytics::GameAnalytics::getABTestingId().data();
    std::wstring result = gameanalytics::utilities::GAUtilities::s2ws(returnValue);
    wcscpy_s(out, result.length() + 1, result.c_str());
}

void getABTestingVariantIdUWP(wchar_t *out)
{
    std::string returnValue = gameanalytics::GameAnalytics::getABTestingVariantId().data();
    std::wstring result = gameanalytics::utilities::GAUtilities::s2ws(returnValue);
    wcscpy_s(out, result.length() + 1, result.c_str());
}
#endif

#endif
