#ifdef __cplusplus
extern "C" {
#endif

void configureAvailableCustomDimensions01(const char *customDimensionsJson);
void configureAvailableCustomDimensions02(const char *customDimensionsJson);
void configureAvailableCustomDimensions03(const char *customDimensionsJson);
void configureAvailableResourceCurrencies(const char *resourceCurrenciesJson);
void configureAvailableResourceItemTypes(const char *resourceItemTypesJson);
void configureBuild(const char *build);
void configureWritablePath(const char *writablePath);
void configureDeviceModel(const char *deviceModel);
void configureDeviceManufacturer(const char *deviceManufacturer);

// the version of SDK code used in an engine. Used for sdk_version field.
// !! if set then it will override the SdkWrapperVersion.
// example "unity 4.6.9"
void configureSdkGameEngineVersion(const char *sdkGameEngineVersion);
// the version of the game engine (if used and version is available)
void configureGameEngineVersion(const char *engineVersion);

void configureUserId(const char *uId);

// initialize - starting SDK (need configuration before starting)
void initialize(const char *gameKey, const char *gameSecret);

// add events
void addBusinessEvent(const char *currency, int amount, const char *itemType, const char *itemId, const char *cartType);

void addResourceEvent(int flowType, const char *currency, float amount, const char *itemType, const char *itemId);

void addProgressionEvent(int progressionStatus, const char *progression01, const char *progression02, const char *progression03);

void addProgressionEventWithScore(int progressionStatus, const char *progression01, const char *progression02, const char *progression03, int score);

void addDesignEvent(const char *eventId);
void addDesignEventWithValue(const char *eventId, double value);
void addErrorEvent(int severity, const char *message);

// set calls can be changed at any time (pre- and post-initialize)
// some calls only work after a configure is called (setCustomDimension)
void setEnabledInfoLog(bool flag);
void setEnabledVerboseLog(bool flag);
void setEnabledManualSessionHandling(bool flag);
void setEnabledErrorReporting(bool flag);
void setEnabledEventSubmission(bool flag);
void setCustomDimension01(const char *dimension01);
void setCustomDimension02(const char *dimension02);
void setCustomDimension03(const char *dimension03);

void gameAnalyticsStartSession();
void gameAnalyticsEndSession();

// game state changes
// will affect how session is started / ended
void onResume();
void onSuspend();
void onQuit();

const char* getRemoteConfigsValueAsString(const char *key);
const char* getRemoteConfigsValueAsStringWithDefaultValue(const char *key, const char *defaultValue);
bool isRemoteConfigsReady();
const char* getRemoteConfigsContentAsString();

const char* getABTestingId();
const char* getABTestingVariantId();

#ifdef __cplusplus
}
#endif
