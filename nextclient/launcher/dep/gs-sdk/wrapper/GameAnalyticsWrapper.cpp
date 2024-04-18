#include "GameAnalytics.h"
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
void addBusinessEvent(const char *currency, int amount, const char *itemType, const char *itemId, const char *cartType)
{
    gameanalytics::GameAnalytics::addBusinessEvent(currency, amount, itemType, itemId, cartType);
}

void addResourceEvent(int flowType, const char *currency, float amount, const char *itemType, const char *itemId)
{
    gameanalytics::GameAnalytics::addResourceEvent((gameanalytics::EGAResourceFlowType)flowType, currency, amount, itemType, itemId);
}

void addProgressionEvent(int progressionStatus, const char *progression01, const char *progression02, const char *progression03)
{
    gameanalytics::GameAnalytics::addProgressionEvent((gameanalytics::EGAProgressionStatus)progressionStatus, progression01, progression02, progression03);
}

void addProgressionEventWithScore(int progressionStatus, const char *progression01, const char *progression02, const char *progression03, int score)
{
    gameanalytics::GameAnalytics::addProgressionEvent((gameanalytics::EGAProgressionStatus)progressionStatus, progression01, progression02, progression03, score);
}

void addDesignEvent(const char *eventId)
{
    gameanalytics::GameAnalytics::addDesignEvent(eventId);
}

void addDesignEventWithValue(const char *eventId, double value)
{
    gameanalytics::GameAnalytics::addDesignEvent(eventId, value);
}

void addErrorEvent(int severity, const char *message)
{
    gameanalytics::GameAnalytics::addErrorEvent((gameanalytics::EGAErrorSeverity)severity, message);
}

// set calls can be changed at any time (pre- and post-initialize)
// some calls only work after a configure is called (setCustomDimension)

void setEnabledInfoLog(bool flag)
{
    gameanalytics::GameAnalytics::setEnabledInfoLog(flag);
}

void setEnabledVerboseLog(bool flag)
{
    gameanalytics::GameAnalytics::setEnabledVerboseLog(flag);
}

void setEnabledManualSessionHandling(bool flag)
{
    gameanalytics::GameAnalytics::setEnabledManualSessionHandling(flag);
}

void setEnabledErrorReporting(bool flag)
{
    gameanalytics::GameAnalytics::setEnabledErrorReporting(flag);
}

void setEnabledEventSubmission(bool flag)
{
    gameanalytics::GameAnalytics::setEnabledEventSubmission(flag);
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

bool isRemoteConfigsReady()
{
    return gameanalytics::GameAnalytics::isRemoteConfigsReady();
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
