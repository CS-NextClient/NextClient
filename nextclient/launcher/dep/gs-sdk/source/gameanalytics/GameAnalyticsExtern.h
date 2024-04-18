#ifdef __cplusplus
extern "C" {
#endif

#if GA_SHARED_LIB
#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
#else
#define EXPORT
#endif

EXPORT void configureAvailableCustomDimensions01(const char *customDimensionsJson);
EXPORT void configureAvailableCustomDimensions02(const char *customDimensionsJson);
EXPORT void configureAvailableCustomDimensions03(const char *customDimensionsJson);
EXPORT void configureAvailableResourceCurrencies(const char *resourceCurrenciesJson);
EXPORT void configureAvailableResourceItemTypes(const char *resourceItemTypesJson);
EXPORT void configureBuild(const char *build);
EXPORT void configureWritablePath(const char *writablePath);
EXPORT void configureDeviceModel(const char *deviceModel);
EXPORT void configureDeviceManufacturer(const char *deviceManufacturer);

// the version of SDK code used in an engine. Used for sdk_version field.
// !! if set then it will override the SdkWrapperVersion.
// example "unity 4.6.9"
EXPORT void configureSdkGameEngineVersion(const char *sdkGameEngineVersion);
// the version of the game engine (if used and version is available)
EXPORT void configureGameEngineVersion(const char *engineVersion);

EXPORT void configureUserId(const char *uId);

// initialize - starting SDK (need configuration before starting)
EXPORT void initialize(const char *gameKey, const char *gameSecret);

// add events
EXPORT void addBusinessEvent(const char *currency, double amount, const char *itemType, const char *itemId, const char *cartType, const char *customFields, double mergeFields);
EXPORT void addBusinessEventJson(const char *jsonArgs);

EXPORT void addResourceEvent(double flowType, const char *currency, double amount, const char *itemType, const char *itemId, const char *customFields, double mergeFields);
EXPORT void addResourceEventJson(const char *jsonArgs);

EXPORT void addProgressionEvent(double progressionStatus, const char *progression01, const char *progression02, const char *progression03, const char *customFields, double mergeFields);
EXPORT void addProgressionEventJson(const char *jsonArgs);

EXPORT void addProgressionEventWithScore(double progressionStatus, const char *progression01, const char *progression02, const char *progression03, double score, const char *customFields, double mergeFields);
EXPORT void addProgressionEventWithScoreJson(const char *jsonArgs);

EXPORT void addDesignEvent(const char *eventId, const char *customFields, double mergeFields);
EXPORT void addDesignEventWithValue(const char *eventId, double value, const char *customFields, double mergeFields);
EXPORT void addErrorEvent(double severity, const char *message, const char *customFields, double mergeFields);

// set calls can be changed at any time (pre- and post-initialize)
// some calls only work after a configure is called (setCustomDimension)
EXPORT void setEnabledInfoLog(double flag);
EXPORT void setEnabledVerboseLog(double flag);
EXPORT void setEnabledManualSessionHandling(double flag);
EXPORT void setEnabledErrorReporting(double flag);
EXPORT void setEnabledEventSubmission(double flag);
EXPORT void setCustomDimension01(const char *dimension01);
EXPORT void setCustomDimension02(const char *dimension02);
EXPORT void setCustomDimension03(const char *dimension03);

EXPORT void setGlobalCustomEventFields(const char *customFields);

EXPORT void gameAnalyticsStartSession();
EXPORT void gameAnalyticsEndSession();

// game state changes
// will affect how session is started / ended
EXPORT void onResume();
EXPORT void onSuspend();
EXPORT void onQuit();

EXPORT const char* getRemoteConfigsValueAsString(const char *key);
EXPORT const char* getRemoteConfigsValueAsStringWithDefaultValue(const char *key, const char *defaultValue);
EXPORT double isRemoteConfigsReady();
EXPORT const char* getRemoteConfigsContentAsString();

EXPORT const char* getABTestingId();
EXPORT const char* getABTestingVariantId();

#if USE_UWP
EXPORT void configureAvailableCustomDimensions01UWP(const wchar_t *customDimensionsJson);
EXPORT void configureAvailableCustomDimensions02UWP(const wchar_t *customDimensionsJson);
EXPORT void configureAvailableCustomDimensions03UWP(const wchar_t *customDimensionsJson);
EXPORT void configureAvailableResourceCurrenciesUWP(const wchar_t *resourceCurrenciesJson);
EXPORT void configureAvailableResourceItemTypesUWP(const wchar_t *resourceItemTypesJson);
EXPORT void configureBuildUWP(const wchar_t *build);
EXPORT void configureWritablePathUWP(const wchar_t *writablePath);
EXPORT void configureDeviceModelUWP(const wchar_t *deviceModel);
EXPORT void configureDeviceManufacturerUWP(const wchar_t *deviceManufacturer);

// the version of SDK code used in an engine. Used for sdk_version field.
// !! if set then it will override the SdkWrapperVersion.
// example "unity 4.6.9"
EXPORT void configureSdkGameEngineVersionUWP(const wchar_t *sdkGameEngineVersion);
// the version of the game engine (if used and version is available)
EXPORT void configureGameEngineVersionUWP(const wchar_t *engineVersion);

EXPORT void configureUserIdUWP(const wchar_t *uId);

EXPORT void initializeUWP(const wchar_t *gameKey, const wchar_t *gameSecret);

EXPORT void setCustomDimension01UWP(const wchar_t *dimension01);
EXPORT void setCustomDimension02UWP(const wchar_t *dimension02);
EXPORT void setCustomDimension03UWP(const wchar_t *dimension03);

EXPORT void setGlobalCustomEventFieldsUWP(const wchar_t *customFields);

EXPORT void addBusinessEventUWP(const wchar_t *currency, double amount, const wchar_t *itemType, const wchar_t *itemId, const wchar_t *cartType, const wchar_t *fields, double mergeFields);
EXPORT void addResourceEventUWP(double flowType, const wchar_t *currency, double amount, const wchar_t *itemType, const wchar_t *itemId, const wchar_t *fields, double mergeFields);
EXPORT void addProgressionEventUWP(double progressionStatus, const wchar_t *progression01, const wchar_t *progression02, const wchar_t *progression03, const wchar_t *fields, double mergeFields);
EXPORT void addProgressionEventWithScoreUWP(double progressionStatus, const wchar_t *progression01, const wchar_t *progression02, const wchar_t *progression03, double score, const wchar_t *fields, double mergeFields);

EXPORT void addDesignEventUWP(const wchar_t *eventId, const wchar_t *fields, double mergeFields);
EXPORT void addDesignEventWithValueUWP(const wchar_t *eventId, double value, const wchar_t *fields, double mergeFields);
EXPORT void addErrorEventUWP(double severity, const wchar_t *message, const wchar_t *fields, double mergeFields);

EXPORT void getRemoteConfigsValueAsStringWithDefaultValueUWP(const wchar_t *key, const wchar_t *defaultValue, wchar_t *out);
EXPORT void getRemoteConfigsContentAsStringUWP(wchar_t *out);

EXPORT void getABTestingIdUWP(wchar_t *out);
EXPORT void getABTestingVariantIdUWP(wchar_t *out);
#endif

#ifdef __cplusplus
}
#endif
