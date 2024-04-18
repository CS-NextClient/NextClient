//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#include "GameAnalytics.h"

#include "GAThreading.h"
#include "GALogger.h"
#include "GAState.h"
#include "GADevice.h"
#include "GAHTTPApi.h"
#include "GAValidator.h"
#include "GAEvents.h"
#include "GAUtilities.h"
#include "GAStore.h"
#if !USE_UWP && !USE_TIZEN
#include "GAUncaughtExceptionHandler.h"
#endif
#include "rapidjson/document.h"
#include <cstdlib>
#if USE_UWP
#include <thread>
#endif
#include <array>

namespace gameanalytics
{
    bool GameAnalytics::_endThread = false;

    // ----------------------- CONFIGURE ---------------------- //

    void GameAnalytics::configureAvailableCustomDimensions01(const StringVector& customDimensions)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([customDimensions]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Available custom dimensions must be set before SDK is initialized");
                return;
            }
            state::GAState::setAvailableCustomDimensions01(customDimensions);
        });
    }

    void GameAnalytics::configureAvailableCustomDimensions01(const char* customDimensionsJson)
    {
        rapidjson::Document json;
        json.Parse(customDimensionsJson);
        StringVector list;

        for (rapidjson::Value::ConstValueIterator itr = json.Begin(); itr != json.End(); ++itr)
        {
            list.add((*itr).GetString());
        }

        configureAvailableCustomDimensions01(list);
    }

    void GameAnalytics::configureAvailableCustomDimensions02(const StringVector& customDimensions)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([customDimensions]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Available custom dimensions must be set before SDK is initialized");
                return;
            }
            state::GAState::setAvailableCustomDimensions02(customDimensions);
        });
    }

    void GameAnalytics::configureAvailableCustomDimensions02(const char* customDimensionsJson)
    {
        rapidjson::Document json;
        json.Parse(customDimensionsJson);
        StringVector list;

        for (rapidjson::Value::ConstValueIterator itr = json.Begin(); itr != json.End(); ++itr)
        {
            list.add((*itr).GetString());
        }

        configureAvailableCustomDimensions02(list);
    }

    void GameAnalytics::configureAvailableCustomDimensions03(const StringVector& customDimensions)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([customDimensions]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Available custom dimensions must be set before SDK is initialized");
                return;
            }
            state::GAState::setAvailableCustomDimensions03(customDimensions);
        });
    }

    void GameAnalytics::configureAvailableCustomDimensions03(const char* customDimensionsJson)
    {
        rapidjson::Document json;
        json.Parse(customDimensionsJson);
        StringVector list;

        for (rapidjson::Value::ConstValueIterator itr = json.Begin(); itr != json.End(); ++itr)
        {
            list.add((*itr).GetString());
        }

        configureAvailableCustomDimensions03(list);
    }

    void GameAnalytics::configureAvailableResourceCurrencies(const StringVector& resourceCurrencies)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([resourceCurrencies]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Available resource currencies must be set before SDK is initialized");
                return;
            }
            state::GAState::setAvailableResourceCurrencies(resourceCurrencies);
        });
    }

    void GameAnalytics::configureAvailableResourceCurrencies(const char* resourceCurrenciesJson)
    {
        rapidjson::Document json;
        json.Parse(resourceCurrenciesJson);
        StringVector list;

        for (rapidjson::Value::ConstValueIterator itr = json.Begin(); itr != json.End(); ++itr)
        {
            list.add((*itr).GetString());
        }

        configureAvailableResourceCurrencies(list);
    }

    void GameAnalytics::configureAvailableResourceItemTypes(const StringVector& resourceItemTypes)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([resourceItemTypes]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Available resource item types must be set before SDK is initialized");
                return;
            }
            state::GAState::setAvailableResourceItemTypes(resourceItemTypes);
        });
    }

    void GameAnalytics::configureAvailableResourceItemTypes(const char* resourceItemTypesJson)
    {
        rapidjson::Document json;
        json.Parse(resourceItemTypesJson);
        StringVector list;

        for (rapidjson::Value::ConstValueIterator itr = json.Begin(); itr != json.End(); ++itr)
        {
            list.add((*itr).GetString());
        }

        configureAvailableResourceItemTypes(list);
    }

    void GameAnalytics::configureBuild(const char* build_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> build = {'\0'};
        snprintf(build.data(), build.size(), "%s", build_ ? build_ : "");
        threading::GAThreading::performTaskOnGAThread([build]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Build version must be set before SDK is initialized.");
                return;
            }
            if (!validators::GAValidator::validateBuild(build.data()))
            {
                logging::GALogger::i("Validation fail - configure build: Cannot be null, empty or above 32 length. String: %s", build.data());
                return;
            }
            state::GAState::setBuild(build.data());
        });
    }

    void GameAnalytics::configureWritablePath(const char* writablePath_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, MAX_PATH_LENGTH> writablePath = {'\0'};
        snprintf(writablePath.data(), writablePath.size(), "%s", writablePath_ ? writablePath_ : "");
        if (isSdkReady(true, false))
        {
            logging::GALogger::w("Writable path must be set before SDK is initialized.");
            return;
        }
        device::GADevice::setWritablePath(writablePath.data());
#if !USE_UWP && !USE_TIZEN
        logging::GALogger::customInitializeLog();
#endif
    }

    void GameAnalytics::configureBuildPlatform(const char* platform_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> platform = {'\0'};
        snprintf(platform.data(), platform.size(), "%s", platform_ ? platform_ : "");
        threading::GAThreading::performTaskOnGAThread([platform]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Platform must be set before SDK is initialized.");
                return;
            }
            if (!validators::GAValidator::validateShortString(platform.data(), false))
            {
                logging::GALogger::i("Validation fail - configure platform: Cannot be null, empty or above 32 length. String: %s", platform.data());
                return;
            }
            device::GADevice::setBuildPlatform(platform.data());
        });
    }

    void GameAnalytics::configureCustomLogHandler(const LogHandler &logHandler)
    {
        if (_endThread)
        {
            return;
        }

        if (isSdkReady(true, false))
        {
            logging::GALogger::w("Writable path must be set before SDK is initialized.");
            return;
        }

        logging::GALogger::setCustomLogHandler(logHandler);
    }

    void GameAnalytics::disableDeviceInfo()
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Disable device info must be set before SDK is initialized.");
                return;
            }
            device::GADevice::disableDeviceInfo();
        });
    }

    void GameAnalytics::configureDeviceModel(const char* deviceModel_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> deviceModel = {'\0'};
        snprintf(deviceModel.data(), deviceModel.size(), "%s", deviceModel_ ? deviceModel_ : "");
        threading::GAThreading::performTaskOnGAThread([deviceModel]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Device model must be set before SDK is initialized.");
                return;
            }
            if (!validators::GAValidator::validateString(deviceModel.data(), true))
            {
                logging::GALogger::i("Validation fail - configure device model: Cannot be null, empty or above 64 length. String: %s", deviceModel.data());
                return;
            }
            device::GADevice::setDeviceModel(deviceModel.data());
        });
    }

    void GameAnalytics::configureDeviceManufacturer(const char* deviceManufacturer_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> deviceManufacturer = {'\0'};
        snprintf(deviceManufacturer.data(), deviceManufacturer.size(), "%s", deviceManufacturer_ ? deviceManufacturer_ : "");
        threading::GAThreading::performTaskOnGAThread([deviceManufacturer]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("Device manufacturer must be set before SDK is initialized.");
                return;
            }
            if (!validators::GAValidator::validateString(deviceManufacturer.data(), true))
            {
                logging::GALogger::i("Validation fail - configure device manufacturer: Cannot be null, empty or above 64 length. String: %s", deviceManufacturer.data());
                return;
            }
            device::GADevice::setDeviceManufacturer(deviceManufacturer.data());
        });
    }

    void GameAnalytics::configureSdkGameEngineVersion(const char* sdkGameEngineVersion_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> sdkGameEngineVersion = {'\0'};
        snprintf(sdkGameEngineVersion.data(), sdkGameEngineVersion.size(), "%s", sdkGameEngineVersion_ ? sdkGameEngineVersion_ : "");
        threading::GAThreading::performTaskOnGAThread([sdkGameEngineVersion]()
        {
            if (isSdkReady(true, false))
            {
                return;
            }
            if (!validators::GAValidator::validateSdkWrapperVersion(sdkGameEngineVersion.data()))
            {
                logging::GALogger::i("Validation fail - configure sdk version: Sdk version not supported. String: %s", sdkGameEngineVersion.data());
                return;
            }
            device::GADevice::setSdkGameEngineVersion(sdkGameEngineVersion.data());
        });
    }

    void GameAnalytics::configureGameEngineVersion(const char* gameEngineVersion_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> gameEngineVersion = {'\0'};
        snprintf(gameEngineVersion.data(), gameEngineVersion.size(), "%s", gameEngineVersion_ ? gameEngineVersion_ : "");
        threading::GAThreading::performTaskOnGAThread([gameEngineVersion]()
        {
            if (isSdkReady(true, false))
            {
                return;
            }
            if (!validators::GAValidator::validateEngineVersion(gameEngineVersion.data()))
            {
                logging::GALogger::i("Validation fail - configure engine: Engine version not supported. String: %s", gameEngineVersion.data());
                return;
            }
            device::GADevice::setGameEngineVersion(gameEngineVersion.data());
        });
    }

    void GameAnalytics::configureUserId(const char* uId_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 129> uId = {'\0'};
        snprintf(uId.data(), uId.size(), "%s", uId_ ? uId_ : "");
        threading::GAThreading::performTaskOnGAThread([uId]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("A custom user id must be set before SDK is initialized.");
                return;
            }
            if (!validators::GAValidator::validateUserId(uId.data()))
            {
                logging::GALogger::i("Validation fail - configure user_id: Cannot be null, empty or above 64 length. Will use default user_id method. Used string: %s", uId.data());
                return;
            }

            state::GAState::setUserId(uId.data());
        });
    }

    // ----------------------- INITIALIZE ---------------------- //

    void GameAnalytics::initialize(const char* gameKey_, const char* gameSecret_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> gameKey = {'\0'};
        snprintf(gameKey.data(), gameKey.size(), "%s", gameKey_ ? gameKey_ : "");
        std::array<char, 65> gameSecret = {'\0'};
        snprintf(gameSecret.data(), gameSecret.size(), "%s", gameSecret_ ? gameSecret_ : "");
#if USE_UWP
        Windows::ApplicationModel::Core::CoreApplication::Suspending += ref new Windows::Foundation::EventHandler<Windows::ApplicationModel::SuspendingEventArgs^>(&GameAnalytics::OnAppSuspending);
        Windows::ApplicationModel::Core::CoreApplication::Resuming += ref new Windows::Foundation::EventHandler<Platform::Object^>(&GameAnalytics::OnAppResuming);
#endif
        threading::GAThreading::performTaskOnGAThread([gameKey, gameSecret]()
        {
            if (isSdkReady(true, false))
            {
                logging::GALogger::w("SDK already initialized. Can only be called once.");
                return;
            }
#if !USE_UWP && !USE_TIZEN
            errorreporter::GAUncaughtExceptionHandler::setUncaughtExceptionHandlers();
#endif

            if (!validators::GAValidator::validateKeys(gameKey.data(), gameSecret.data()))
            {
                logging::GALogger::w("SDK failed initialize. Game key or secret key is invalid. Can only contain characters A-z 0-9, gameKey is 32 length, gameSecret is 40 length. Failed keys - gameKey: %s, secretKey: %s", gameKey.data(), gameSecret.data());
                return;
            }

            state::GAState::setKeys(gameKey.data(), gameSecret.data());

            if (!store::GAStore::ensureDatabase(false, gameKey.data()))
            {
                logging::GALogger::w("Could not ensure/validate local event database: %s", device::GADevice::getWritablePath());
            }

            state::GAState::internalInitialize();
        });
    }

    // ----------------------- ADD EVENTS ---------------------- //


    void GameAnalytics::addBusinessEvent(
        const char* currency,
        int amount,
        const char* itemType,
        const char* itemId,
        const char* cartType)
    {
        addBusinessEvent(currency, amount, itemType, itemId, cartType, "");
    }

    void GameAnalytics::addBusinessEvent(
        const char* currency_,
        int amount,
        const char* itemType_,
        const char* itemId_,
        const char* cartType_,
        const char* fields_)
    {
        addBusinessEvent(currency_, amount, itemType_, itemId_, cartType_, fields_, false);
    }

    void GameAnalytics::addBusinessEvent(
        const char* currency_,
        int amount,
        const char* itemType_,
        const char* itemId_,
        const char* cartType_,
        const char* fields_,
        bool mergeFields)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> currency = {'\0'};
        snprintf(currency.data(), currency.size(), "%s", currency_ ? currency_ : "");
        std::array<char, 65> itemType = {'\0'};
        snprintf(itemType.data(), itemType.size(), "%s", itemType_ ? itemType_ : "");
        std::array<char, 65> itemId = {'\0'};
        snprintf(itemId.data(), itemId.size(), "%s", itemId_ ? itemId_ : "");
        std::array<char, 65> cartType = {'\0'};
        snprintf(cartType.data(), cartType.size(), "%s", cartType_ ? cartType_ : "");
        std::array<char, 4097> fields = {'\0'};
        snprintf(fields.data(), fields.size(), "%s", fields_ ? fields_ : "");
        threading::GAThreading::performTaskOnGAThread([currency, amount, itemType, itemId, cartType, fields, mergeFields]()
        {
            if (!isSdkReady(true, true, "Could not add business event"))
            {
                return;
            }
            // Send to events
            rapidjson::Document fieldsJson;
            fieldsJson.Parse(fields.data());
            events::GAEvents::addBusinessEvent(currency.data(), amount, itemType.data(), itemId.data(), cartType.data(), fieldsJson, mergeFields);
        });
    }


    void GameAnalytics::addResourceEvent(EGAResourceFlowType flowType, const char* currency, float amount, const char* itemType, const char* itemId)
    {
        addResourceEvent(flowType, currency, amount, itemType, itemId, "");
    }

    void GameAnalytics::addResourceEvent(EGAResourceFlowType flowType, const char* currency_, float amount, const char* itemType_, const char* itemId_, const char* fields_)
    {
        addResourceEvent(flowType, currency_, amount, itemType_, itemId_, fields_, false);
    }

    void GameAnalytics::addResourceEvent(EGAResourceFlowType flowType, const char* currency_, float amount, const char* itemType_, const char* itemId_, const char* fields_, bool mergeFields)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> currency = {'\0'};
        snprintf(currency.data(), currency.size(), "%s", currency_ ? currency_ : "");
        std::array<char, 65> itemType = {'\0'};
        snprintf(itemType.data(), itemType.size(), "%s", itemType_ ? itemType_ : "");
        std::array<char, 65> itemId = {'\0'};
        snprintf(itemId.data(), itemId.size(), "%s", itemId_ ? itemId_ : "");
        std::array<char, 4097> fields = {'\0'};
        snprintf(fields.data(), fields.size(), "%s", fields_ ? fields_ : "");
        threading::GAThreading::performTaskOnGAThread([flowType, currency, amount, itemType, itemId, fields, mergeFields]()
        {
            if (!isSdkReady(true, true, "Could not add resource event"))
            {
                return;
            }

            rapidjson::Document fieldsJson;
            fieldsJson.Parse(fields.data());
            events::GAEvents::addResourceEvent(flowType, currency.data(), amount, itemType.data(), itemId.data(), fieldsJson, mergeFields);
        });
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const char* progression01, const char* progression02, const char* progression03)
    {
        addProgressionEvent(progressionStatus, progression01, progression02, progression03, "");
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const char* progression01_, const char* progression02_, const char* progression03_, const char* fields_)
    {
        addProgressionEvent(progressionStatus, progression01_, progression02_, progression03_, fields_, false);
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const char* progression01_, const char* progression02_, const char* progression03_, const char* fields_, bool mergeFields)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> progression01 = {'\0'};
        snprintf(progression01.data(), progression01.size(), "%s", progression01_ ? progression01_ : "");
        std::array<char, 65> progression02 = {'\0'};
        snprintf(progression02.data(), progression02.size(), "%s", progression02_ ? progression02_ : "");
        std::array<char, 65> progression03 = {'\0'};
        snprintf(progression03.data(), progression03.size(), "%s", progression03_ ? progression03_ : "");
        std::array<char, 4097> fields = {'\0'};
        snprintf(fields.data(), fields.size(), "%s", fields_);
        threading::GAThreading::performTaskOnGAThread([progressionStatus, progression01, progression02, progression03, fields, mergeFields]()
        {
            if (!isSdkReady(true, true, "Could not add progression event"))
            {
                return;
            }

            // Send to events
            rapidjson::Document fieldsJson;
            fieldsJson.Parse(fields.data());
            events::GAEvents::addProgressionEvent(progressionStatus, progression01.data(), progression02.data(), progression03.data(), 0, false, fieldsJson, mergeFields);
        });
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const char* progression01, const char* progression02, const char* progression03, int score)
    {
        addProgressionEvent(progressionStatus, progression01, progression02, progression03, score, "");
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const char* progression01_, const char* progression02_, const char* progression03_, int score, const char* fields_)
    {
        addProgressionEvent(progressionStatus, progression01_, progression02_, progression03_, score, fields_, false);
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const char* progression01_, const char* progression02_, const char* progression03_, int score, const char* fields_, bool mergeFields)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> progression01 = {'\0'};
        snprintf(progression01.data(), progression01.size(), "%s", progression01_ ? progression01_ : "");
        std::array<char, 65> progression02 = {'\0'};
        snprintf(progression02.data(), progression02.size(), "%s", progression02_ ? progression02_ : "");
        std::array<char, 65> progression03 = {'\0'};
        snprintf(progression03.data(), progression03.size(), "%s", progression03_ ? progression03_ : "");
        std::array<char, 4097> fields = {'\0'};
        snprintf(fields.data(), fields.size(), "%s", fields_ ? fields_ : "");
        threading::GAThreading::performTaskOnGAThread([progressionStatus, progression01, progression02, progression03, score, fields, mergeFields]()
        {
            if (!isSdkReady(true, true, "Could not add progression event"))
            {
                return;
            }

            // Send to events
            rapidjson::Document fieldsJson;
            fieldsJson.Parse(fields.data());
            events::GAEvents::addProgressionEvent(progressionStatus, progression01.data(), progression02.data(), progression03.data(), score, true, fieldsJson, mergeFields);
        });
    }

    void GameAnalytics::addDesignEvent(const char* eventId)
    {
        addDesignEvent(eventId, "");
    }

    void GameAnalytics::addDesignEvent(const char* eventId_, const char* fields_)
    {
        addDesignEvent(eventId_, fields_, false);
    }

    void GameAnalytics::addDesignEvent(const char* eventId_, const char* fields_, bool mergeFields)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 400> eventId = {'\0'};
        snprintf(eventId.data(), eventId.size(), "%s", eventId_ ? eventId_ : "");
        std::array<char, 4097> fields = {'\0'};
        snprintf(fields.data(), fields.size(), "%s", fields_ ? fields_ : "");
        threading::GAThreading::performTaskOnGAThread([eventId, fields, mergeFields]()
        {
            if (!isSdkReady(true, true, "Could not add design event"))
            {
                return;
            }
            rapidjson::Document fieldsJson;
            fieldsJson.Parse(fields.data());
            events::GAEvents::addDesignEvent(eventId.data(), 0, false, fieldsJson, mergeFields);
        });
    }

    void GameAnalytics::addDesignEvent(const char* eventId, double value)
    {
        addDesignEvent(eventId, value, "");
    }

    void GameAnalytics::addDesignEvent(const char* eventId_, double value, const char* fields_)
    {
        addDesignEvent(eventId_, value, fields_, false);
    }

    void GameAnalytics::addDesignEvent(const char* eventId_, double value, const char* fields_, bool mergeFields)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 400> eventId = {'\0'};
        snprintf(eventId.data(), eventId.size(), "%s", eventId_ ? eventId_ : "");
        std::array<char, 4097> fields = {'\0'};
        snprintf(fields.data(), fields.size(), "%s", fields_ ? fields_ : "");
        threading::GAThreading::performTaskOnGAThread([eventId, value, fields, mergeFields]()
        {
            if (!isSdkReady(true, true, "Could not add design event"))
            {
                return;
            }
            rapidjson::Document fieldsJson;
            fieldsJson.Parse(fields.data());
            events::GAEvents::addDesignEvent(eventId.data(), value, true, fieldsJson, mergeFields);
        });
    }

    void GameAnalytics::addErrorEvent(EGAErrorSeverity severity, const char* message)
    {
        addErrorEvent(severity, message, "");
    }

    void GameAnalytics::addErrorEvent(EGAErrorSeverity severity, const char* message_, const char* fields_)
    {
        addErrorEvent(severity, message_, fields_, false);
    }

    void GameAnalytics::addErrorEvent(EGAErrorSeverity severity, const char* message_, const char* fields_, bool mergeFields)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 8200> message = {'\0'};
        snprintf(message.data(), message.size(), "%s", message_ ? message_ : "");
        std::array<char, 4097> fields = {'\0'};
        snprintf(fields.data(), fields.size(), "%s", fields_ ? fields_ : "");
        threading::GAThreading::performTaskOnGAThread([severity, message, fields, mergeFields]()
        {
            if (!isSdkReady(true, true, "Could not add error event"))
            {
                return;
            }
            rapidjson::Document fieldsJson;
            fieldsJson.Parse(fields.data());
            events::GAEvents::addErrorEvent(severity, message.data(), fieldsJson, mergeFields);
        });
    }

    // ------------- SET STATE CHANGES WHILE RUNNING ----------------- //

    void GameAnalytics::setEnabledInfoLog(bool flag)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([flag]()
        {
            if (flag)
            {
                logging::GALogger::setInfoLog(flag);
                logging::GALogger::i("Info logging enabled");
            }
            else
            {
                logging::GALogger::i("Info logging disabled");
                logging::GALogger::setInfoLog(flag);
            }
        });
    }

    void GameAnalytics::setEnabledVerboseLog(bool flag)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([flag]()
        {
            if (flag)
            {
                logging::GALogger::setVerboseInfoLog(flag);
                logging::GALogger::i("Verbose logging enabled");
            }
            else
            {
                logging::GALogger::i("Verbose logging disabled");
                logging::GALogger::setVerboseInfoLog(flag);
            }
        });
    }

    void GameAnalytics::setEnabledManualSessionHandling(bool flag)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([flag]()
        {
            state::GAState::setManualSessionHandling(flag);
        });
    }

    void GameAnalytics::setEnabledErrorReporting(bool flag)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([flag]()
        {
            state::GAState::setEnableErrorReporting(flag);
        });
    }

    void GameAnalytics::setEnabledEventSubmission(bool flag)
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([flag]()
        {
            if (flag)
            {
                state::GAState::setEnabledEventSubmission(flag);
                logging::GALogger::i("Event submission enabled");
            }
            else
            {
                logging::GALogger::i("Event submission disabled");
                state::GAState::setEnabledEventSubmission(flag);
            }
        });
    }

    void GameAnalytics::setCustomDimension01(const char* dimension_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> dimension = {'\0'};
        snprintf(dimension.data(), dimension.size(), "%s", dimension_ ? dimension_ : "");
        threading::GAThreading::performTaskOnGAThread([dimension]()
        {
            if (!validators::GAValidator::validateDimension01(dimension.data()))
            {
                logging::GALogger::w("Could not set custom01 dimension value to '%s'. Value not found in available custom01 dimension values", dimension.data());
                return;
            }
            state::GAState::setCustomDimension01(dimension.data());
        });
    }

    void GameAnalytics::setCustomDimension02(const char* dimension_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> dimension = {'\0'};
        snprintf(dimension.data(), dimension.size(), "%s", dimension_ ? dimension_ : "");
        threading::GAThreading::performTaskOnGAThread([dimension]()
        {
            if (!validators::GAValidator::validateDimension02(dimension.data()))
            {
                logging::GALogger::w("Could not set custom02 dimension value to '%s'. Value not found in available custom02 dimension values", dimension.data());
                return;
            }
            state::GAState::setCustomDimension02(dimension.data());
        });
    }

    void GameAnalytics::setCustomDimension03(const char* dimension_)
    {
        if(_endThread)
        {
            return;
        }

        std::array<char, 65> dimension = {'\0'};
        snprintf(dimension.data(), dimension.size(), "%s", dimension_ ? dimension_ : "");
        threading::GAThreading::performTaskOnGAThread([dimension]()
        {
            if (!validators::GAValidator::validateDimension03(dimension.data()))
            {
                logging::GALogger::w("Could not set custom03 dimension value to '%s'. Value not found in available custom02 dimension values", dimension.data());
                return;
            }
            state::GAState::setCustomDimension03(dimension.data());
        });
    }

    void GameAnalytics::setGlobalCustomEventFields(const char *customFields_)
    {
        if (_endThread)
        {
            return;
        }

        std::array<char, 4097> fields = {'\0'};
        snprintf(fields.data(), fields.size(), "%s", customFields_ ? customFields_ : "");
        threading::GAThreading::performTaskOnGAThread([fields]()
        {
            state::GAState::setGlobalCustomEventFields(fields.data());
        });
    }

    std::vector<char> GameAnalytics::getRemoteConfigsValueAsString(const char* key)
    {
        return getRemoteConfigsValueAsString(key, "");
    }

    std::vector<char> GameAnalytics::getRemoteConfigsValueAsString(const char* key, const char* defaultValue)
    {
        return state::GAState::getRemoteConfigsStringValue(key, defaultValue);
    }

    bool GameAnalytics::isRemoteConfigsReady()
    {
        return state::GAState::isRemoteConfigsReady();
    }

    void GameAnalytics::addRemoteConfigsListener(const std::shared_ptr<IRemoteConfigsListener>& listener)
    {
        state::GAState::addRemoteConfigsListener(listener);
    }

    void GameAnalytics::removeRemoteConfigsListener(const std::shared_ptr<IRemoteConfigsListener>& listener)
    {
        state::GAState::removeRemoteConfigsListener(listener);
    }

    std::vector<char> GameAnalytics::getRemoteConfigsContentAsString()
    {
        return state::GAState::getRemoteConfigsContentAsString();
    }

    std::vector<char> GameAnalytics::getABTestingId()
    {
        return state::GAState::getAbId();
    }

    std::vector<char> GameAnalytics::getABTestingVariantId()
    {
        return state::GAState::getAbVariantId();
    }

    void GameAnalytics::startSession()
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([]()
        {
            if(state::GAState::useManualSessionHandling())
            {
                if (!state::GAState::isInitialized())
                {
                    return;
                }

                if(state::GAState::isEnabled() && state::GAState::sessionIsStarted())
                {
                    state::GAState::endSessionAndStopQueue(false);
                }

                state::GAState::resumeSessionAndStartQueue();
            }
        });
    }

    void GameAnalytics::endSession()
    {
        if (state::GAState::useManualSessionHandling())
        {
            onSuspend();
        }
    }


    // -------------- SET GAME STATE CHANGES --------------- //

    void GameAnalytics::onResume()
    {
        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([]()
        {
            if(!state::GAState::useManualSessionHandling())
            {
                state::GAState::resumeSessionAndStartQueue();
            }
        });
    }

    void GameAnalytics::onSuspend()
    {
        if(_endThread)
        {
            return;
        }

        try
        {
            threading::GAThreading::performTaskOnGAThread([]()
            {
                state::GAState::endSessionAndStopQueue(false);
            });
        }
        catch (const std::exception&)
        {
        }
    }

    void GameAnalytics::onQuit()
    {
        if(_endThread)
        {
            return;
        }

        try
        {
            threading::GAThreading::performTaskOnGAThread([]()
            {
                _endThread = true;
                state::GAState::endSessionAndStopQueue(true);
            });

#if !USE_TIZEN
            while (!threading::GAThreading::isThreadFinished())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
#endif
        }
        catch (const std::exception&)
        {
        }
    }

    bool GameAnalytics::isThreadEnding()
    {
        return _endThread || threading::GAThreading::isThreadEnding();
    }

#if USE_UWP
    void GameAnalytics::configureAvailableCustomDimensions01(const std::vector<std::wstring>& customDimensions)
    {
        StringVector list;
        for (const std::wstring& dimension : customDimensions)
        {
            list.add(utilities::GAUtilities::ws2s(dimension).c_str());
        }
        configureAvailableCustomDimensions01(list);
    }

    void GameAnalytics::configureAvailableCustomDimensions02(const std::vector<std::wstring>& customDimensions)
    {
        StringVector list;
        for (const std::wstring& dimension : customDimensions)
        {
            list.add(utilities::GAUtilities::ws2s(dimension).c_str());
        }
        configureAvailableCustomDimensions02(list);
    }

    void GameAnalytics::configureAvailableCustomDimensions03(const std::vector<std::wstring>& customDimensions)
    {
        StringVector list;
        for (const std::wstring& dimension : customDimensions)
        {
            list.add(utilities::GAUtilities::ws2s(dimension).c_str());
        }
        configureAvailableCustomDimensions03(list);
    }

    void GameAnalytics::configureAvailableResourceCurrencies(const std::vector<std::wstring>& resourceCurrencies)
    {
        StringVector list;
        for (const std::wstring& currency : resourceCurrencies)
        {
            list.add(utilities::GAUtilities::ws2s(currency).c_str());
        }
        configureAvailableResourceCurrencies(list);
    }

    void GameAnalytics::configureAvailableResourceItemTypes(const std::vector<std::wstring>& resourceItemTypes)
    {
        StringVector list;
        for (const std::wstring& itemType : resourceItemTypes)
        {
            list.add(utilities::GAUtilities::ws2s(itemType).c_str());
        }
        configureAvailableResourceItemTypes(list);
    }

    void GameAnalytics::configureBuild(const std::wstring& build)
    {
        configureBuild(utilities::GAUtilities::ws2s(build).c_str());
    }

    void GameAnalytics::configureWritablePath(const std::wstring& writablePath)
    {
        configureWritablePath(utilities::GAUtilities::ws2s(writablePath).c_str());
    }

    void GameAnalytics::configureBuildPlatform(const std::wstring& platform)
    {
        configureBuildPlatform(utilities::GAUtilities::ws2s(platform).c_str());
    }

    void GameAnalytics::configureDeviceModel(const std::wstring& deviceModel)
    {
        configureDeviceModel(utilities::GAUtilities::ws2s(deviceModel).c_str());
    }

    void GameAnalytics::configureDeviceManufacturer(const std::wstring& deviceManufacturer)
    {
        configureDeviceManufacturer(utilities::GAUtilities::ws2s(deviceManufacturer).c_str());
    }

    void GameAnalytics::configureSdkGameEngineVersion(const std::wstring& sdkGameEngineVersion)
    {
        configureSdkGameEngineVersion(utilities::GAUtilities::ws2s(sdkGameEngineVersion).c_str());
    }

    void GameAnalytics::configureGameEngineVersion(const std::wstring& engineVersion)
    {
        configureGameEngineVersion(utilities::GAUtilities::ws2s(engineVersion).c_str());
    }

    void GameAnalytics::configureUserId(const std::wstring& uId)
    {
        configureUserId(utilities::GAUtilities::ws2s(uId).c_str());
    }

    void GameAnalytics::initialize(const std::wstring& gameKey, const std::wstring& gameSecret)
    {
        initialize(utilities::GAUtilities::ws2s(gameKey).c_str(), utilities::GAUtilities::ws2s(gameSecret).c_str());
    }

    void GameAnalytics::addBusinessEvent(const std::wstring& currency, int amount, const std::wstring& itemType, const std::wstring& itemId, const std::wstring& cartType)
    {
        addBusinessEvent(currency, amount, itemType, itemId, cartType, L"");
    }

    void GameAnalytics::addBusinessEvent(const std::wstring& currency, int amount, const std::wstring& itemType, const std::wstring& itemId, const std::wstring& cartType, const std::wstring& fields)
    {
        addBusinessEvent(utilities::GAUtilities::ws2s(currency).c_str(), amount, utilities::GAUtilities::ws2s(itemType).c_str(), utilities::GAUtilities::ws2s(itemId).c_str(), utilities::GAUtilities::ws2s(cartType).c_str(), utilities::GAUtilities::ws2s(fields).c_str());
    }

    void GameAnalytics::addBusinessEvent(const std::wstring& currency, int amount, const std::wstring& itemType, const std::wstring& itemId, const std::wstring& cartType, const std::wstring& fields, bool mergeFields)
    {
        addBusinessEvent(utilities::GAUtilities::ws2s(currency).c_str(), amount, utilities::GAUtilities::ws2s(itemType).c_str(), utilities::GAUtilities::ws2s(itemId).c_str(), utilities::GAUtilities::ws2s(cartType).c_str(), utilities::GAUtilities::ws2s(fields).c_str(), mergeFields);
    }

    void GameAnalytics::addResourceEvent(EGAResourceFlowType flowType, const std::wstring& currency, float amount, const std::wstring&itemType, const std::wstring& itemId)
    {
        addResourceEvent(flowType, currency, amount, itemType, itemId, L"");
    }

    void GameAnalytics::addResourceEvent(EGAResourceFlowType flowType, const std::wstring& currency, float amount, const std::wstring&itemType, const std::wstring& itemId, const std::wstring& fields)
    {
        addResourceEvent(flowType, utilities::GAUtilities::ws2s(currency).c_str(), amount, utilities::GAUtilities::ws2s(itemType).c_str(), utilities::GAUtilities::ws2s(itemId).c_str(), utilities::GAUtilities::ws2s(fields).c_str());
    }

    void GameAnalytics::addResourceEvent(EGAResourceFlowType flowType, const std::wstring& currency, float amount, const std::wstring&itemType, const std::wstring& itemId, const std::wstring& fields, bool mergeFields)
    {
        addResourceEvent(flowType, utilities::GAUtilities::ws2s(currency).c_str(), amount, utilities::GAUtilities::ws2s(itemType).c_str(), utilities::GAUtilities::ws2s(itemId).c_str(), utilities::GAUtilities::ws2s(fields).c_str(), mergeFields);
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const std::wstring& progression01, const std::wstring& progression02, const std::wstring& progression03)
    {
        addProgressionEvent(progressionStatus, progression01, progression02, progression03, L"");
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const std::wstring& progression01, const std::wstring& progression02, const std::wstring& progression03, const std::wstring& fields)
    {
        addProgressionEvent(progressionStatus, utilities::GAUtilities::ws2s(progression01).c_str(), utilities::GAUtilities::ws2s(progression02).c_str(), utilities::GAUtilities::ws2s(progression03).c_str(), utilities::GAUtilities::ws2s(fields).c_str());
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const std::wstring& progression01, const std::wstring& progression02, const std::wstring& progression03, const std::wstring& fields, bool mergeFields)
    {
        addProgressionEvent(progressionStatus, utilities::GAUtilities::ws2s(progression01).c_str(), utilities::GAUtilities::ws2s(progression02).c_str(), utilities::GAUtilities::ws2s(progression03).c_str(), utilities::GAUtilities::ws2s(fields).c_str(), mergeFields);
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const std::wstring& progression01, const std::wstring& progression02, const std::wstring& progression03, int score)
    {
        addProgressionEvent(progressionStatus, progression01, progression02, progression03, score, L"");
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const std::wstring& progression01, const std::wstring& progression02, const std::wstring& progression03, int score, const std::wstring& fields)
    {
        addProgressionEvent(progressionStatus, utilities::GAUtilities::ws2s(progression01).c_str(), utilities::GAUtilities::ws2s(progression02).c_str(), utilities::GAUtilities::ws2s(progression03).c_str(), score, utilities::GAUtilities::ws2s(fields).c_str());
    }

    void GameAnalytics::addProgressionEvent(EGAProgressionStatus progressionStatus, const std::wstring& progression01, const std::wstring& progression02, const std::wstring& progression03, int score, const std::wstring& fields, bool mergeFields)
    {
        addProgressionEvent(progressionStatus, utilities::GAUtilities::ws2s(progression01).c_str(), utilities::GAUtilities::ws2s(progression02).c_str(), utilities::GAUtilities::ws2s(progression03).c_str(), score, utilities::GAUtilities::ws2s(fields).c_str(), mergeFields);
    }

    void GameAnalytics::addDesignEvent(const std::wstring& eventId)
    {
        addDesignEvent(eventId, L"");
    }

    void GameAnalytics::addDesignEvent(const std::wstring& eventId, const std::wstring& fields)
    {
        addDesignEvent(utilities::GAUtilities::ws2s(eventId).c_str(), utilities::GAUtilities::ws2s(fields).c_str());
    }

    void GameAnalytics::addDesignEvent(const std::wstring& eventId, const std::wstring& fields, bool mergeFields)
    {
        addDesignEvent(utilities::GAUtilities::ws2s(eventId).c_str(), utilities::GAUtilities::ws2s(fields).c_str(), mergeFields);
    }

    void GameAnalytics::addDesignEvent(const std::wstring& eventId, double value)
    {
        addDesignEvent(eventId, value, L"");
    }

    void GameAnalytics::addDesignEvent(const std::wstring& eventId, double value, const std::wstring& fields)
    {
        addDesignEvent(utilities::GAUtilities::ws2s(eventId).c_str(), value, utilities::GAUtilities::ws2s(fields).c_str());
    }

    void GameAnalytics::addDesignEvent(const std::wstring& eventId, double value, const std::wstring& fields, bool mergeFields)
    {
        addDesignEvent(utilities::GAUtilities::ws2s(eventId).c_str(), value, utilities::GAUtilities::ws2s(fields).c_str(), mergeFields);
    }

    void GameAnalytics::addErrorEvent(EGAErrorSeverity severity, const std::wstring& message)
    {
        addErrorEvent(severity, message, L"");
    }

    void GameAnalytics::addErrorEvent(EGAErrorSeverity severity, const std::wstring& message, const std::wstring& fields)
    {
        addErrorEvent(severity, utilities::GAUtilities::ws2s(message).c_str(), utilities::GAUtilities::ws2s(fields).c_str());
    }

    void GameAnalytics::addErrorEvent(EGAErrorSeverity severity, const std::wstring& message, const std::wstring& fields, bool mergeFields)
    {
        addErrorEvent(severity, utilities::GAUtilities::ws2s(message).c_str(), utilities::GAUtilities::ws2s(fields).c_str(), mergeFields);
    }

    void GameAnalytics::setCustomDimension01(const std::wstring& dimension01)
    {
        setCustomDimension01(utilities::GAUtilities::ws2s(dimension01).c_str());
    }

    void GameAnalytics::setCustomDimension02(const std::wstring& dimension02)
    {
        setCustomDimension02(utilities::GAUtilities::ws2s(dimension02).c_str());
    }

    void GameAnalytics::setCustomDimension03(const std::wstring& dimension03)
    {
        setCustomDimension03(utilities::GAUtilities::ws2s(dimension03).c_str());
    }

    void GameAnalytics::setGlobalCustomEventFields(const std::wstring &customFields)
    {
        setGlobalCustomEventFields(utilities::GAUtilities::ws2s(customFields).c_str());
    }
#endif

    // --------------PRIVATE HELPERS -------------- //

    bool GameAnalytics::isSdkReady(bool needsInitialized)
    {
        return isSdkReady(needsInitialized, true);
    }

    bool GameAnalytics::isSdkReady(bool needsInitialized, bool warn)
    {
        return isSdkReady(needsInitialized, warn, "");
    }

    bool GameAnalytics::isSdkReady(bool needsInitialized, bool warn, const char* message)
    {
        char m[33] = "";
        if (strlen(message) > 0)
        {
            snprintf(m, sizeof(m), "%s: ", message);
        }

        // Make sure database is ready
        if (!store::GAStore::getTableReady())
        {
            if (warn)
            {
                logging::GALogger::w("%sDatastore not initialized", m);
            }
            return false;
        }
        // Is SDK initialized
        if (needsInitialized && !state::GAState::isInitialized())
        {
            if (warn)
            {
                logging::GALogger::w("%sSDK is not initialized", m);
            }
            return false;
        }
        // Is SDK enabled
        if (needsInitialized && !state::GAState::isEnabled())
        {
            if (warn)
            {
                logging::GALogger::w("%sSDK is disabled", m);
            }
            return false;
        }

        // Is session started
        if (needsInitialized && !state::GAState::sessionIsStarted())
        {
            if (warn)
            {
                logging::GALogger::w("%sSession has not started yet", m);
            }
            return false;
        }
        return true;
    }

#if USE_UWP
    void GameAnalytics::OnAppSuspending(Platform::Object ^sender, Windows::ApplicationModel::SuspendingEventArgs ^e)
    {
        (void)sender;    // Unused parameter

        auto deferral = e->SuspendingOperation->GetDeferral();

        Concurrency::create_task([deferral]()
        {
            if (!state::GAState::useManualSessionHandling())
            {
                onSuspend();

                while(!threading::GAThreading::isThreadFinished())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else
            {
                logging::GALogger::i("OnSuspending: Not calling GameAnalytics.OnSuspend() as using manual session handling");
            }
            deferral->Complete();
        });


    }

    void GameAnalytics::OnAppResuming(Platform::Object ^sender, Platform::Object ^args)
    {
        (void)sender;    // Unused parameter

        if(_endThread)
        {
            return;
        }

        threading::GAThreading::performTaskOnGAThread([]()
        {
            if (!state::GAState::useManualSessionHandling())
            {
                onResume();
            }
            else
            {
                logging::GALogger::i("OnResuming: Not calling GameAnalytics.OnResume() as using manual session handling");
            }
        });
    }

#endif

} // namespace gameanalytics
