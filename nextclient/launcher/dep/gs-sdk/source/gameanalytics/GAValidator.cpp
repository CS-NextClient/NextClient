//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#include "GAValidator.h"
#include "GAUtilities.h"
#include "GAEvents.h"
#include "GAState.h"
#include "GALogger.h"
#include "GAHTTPApi.h"
#include <string.h>
#include <stdio.h>

#include <map>

namespace gameanalytics
{
    namespace validators
    {
        void GAValidator::validateBusinessEvent(
            const char* currency,
            long amount,
            const char* cartType,
            const char* itemType,
            const char* itemId,
            ValidationResult& out)
        {
            // validate currency
            if (!GAValidator::validateCurrency(currency))
            {
                logging::GALogger::w("Validation fail - business event - currency: Cannot be (null) and need to be A-Z, 3 characters and in the standard at openexchangerates.org. Failed currency: %s", currency);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::BusinessEvent;
                out.action = http::EGASdkErrorAction::InvalidCurrency;
                out.parameter = http::EGASdkErrorParameter::Currency;
                snprintf(out.reason, 8193, "%s", currency);
                return;
            }

            if (amount < 0)
            {
                logging::GALogger::w("Validation fail - business event - amount. Cannot be less than 0. String: %ld", amount);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::BusinessEvent;
                out.action = http::EGASdkErrorAction::InvalidAmount;
                out.parameter = http::EGASdkErrorParameter::Amount;
                snprintf(out.reason, 8193, "%ld", amount);
                return;
            }

            // validate cartType
            if (!GAValidator::validateShortString(cartType, true))
            {
                logging::GALogger::w("Validation fail - business event - cartType. Cannot be above 32 length. String: %s", cartType);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::BusinessEvent;
                out.action = http::EGASdkErrorAction::InvalidShortString;
                out.parameter = http::EGASdkErrorParameter::CartType;
                snprintf(out.reason, 8193, "%s", cartType);
                return;
            }

            // validate itemType length
            if (!GAValidator::validateEventPartLength(itemType, false))
            {
                logging::GALogger::w("Validation fail - business event - itemType: Cannot be (null), empty or above 64 characters. String: %s", itemType);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::BusinessEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartLength;
                out.parameter = http::EGASdkErrorParameter::ItemType;
                snprintf(out.reason, 8193, "%s", itemType);
                return;
            }

            // validate itemType chars
            if (!GAValidator::validateEventPartCharacters(itemType))
            {
                logging::GALogger::w("Validation fail - business event - itemType: Cannot contain other characters than A-z, 0-9, -_., ()!?. String: %s", itemType);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::BusinessEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartCharacters;
                out.parameter = http::EGASdkErrorParameter::ItemType;
                snprintf(out.reason, 8193, "%s", itemType);
                return;
            }

            // validate itemId
            if (!GAValidator::validateEventPartLength(itemId, false))
            {
                logging::GALogger::w("Validation fail - business event - itemId. Cannot be (null), empty or above 64 characters. String: %s", itemId);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::BusinessEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartLength;
                out.parameter = http::EGASdkErrorParameter::ItemId;
                snprintf(out.reason, 8193, "%s", itemId);
                return;
            }

            if (!GAValidator::validateEventPartCharacters(itemId))
            {
                logging::GALogger::w("Validation fail - business event - itemId: Cannot contain other characters than A-z, 0-9, -_., ()!?. String: %s", itemId);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::BusinessEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartCharacters;
                out.parameter = http::EGASdkErrorParameter::ItemId;
                snprintf(out.reason, 8193, "%s", itemId);
                return;
            }

            out.result = true;
        }

        void GAValidator::validateResourceEvent(
            EGAResourceFlowType flowType,
            const char* currency,
            double amount,
            const char* itemType,
            const char* itemId,
            ValidationResult& out
            )
        {
            char resourceFlowTypeString[10] = "";
            events::GAEvents::resourceFlowTypeString(flowType, resourceFlowTypeString);
            if (strlen(resourceFlowTypeString) == 0)
            {
                logging::GALogger::w("Validation fail - resource event - flowType: Invalid flow type.");
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::InvalidFlowType;
                out.parameter = http::EGASdkErrorParameter::FlowType;
                snprintf(out.reason, 8193, "%s", "");
                return;
            }
            if (utilities::GAUtilities::isStringNullOrEmpty(currency))
            {
                logging::GALogger::w("Validation fail - resource event - currency: Cannot be (null)");
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::StringEmptyOrNull;
                out.parameter = http::EGASdkErrorParameter::Currency;
                snprintf(out.reason, 8193, "%s", "");
                return;
            }
            if (!state::GAState::hasAvailableResourceCurrency(currency))
            {
                logging::GALogger::w("Validation fail - resource event - currency: Not found in list of pre-defined available resource currencies. String: %s", currency);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::NotFoundInAvailableCurrencies;
                out.parameter = http::EGASdkErrorParameter::Currency;
                snprintf(out.reason, 8193, "%s", currency);
                return;
            }
            if (!(amount > 0))
            {
                logging::GALogger::w("Validation fail - resource event - amount: Float amount cannot be 0 or negative. Value: %f", amount);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::InvalidAmount;
                out.parameter = http::EGASdkErrorParameter::Amount;
                snprintf(out.reason, 8193, "%f", amount);
                return;
            }
            if (utilities::GAUtilities::isStringNullOrEmpty(itemType))
            {
                logging::GALogger::w("Validation fail - resource event - itemType: Cannot be (null)");
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::StringEmptyOrNull;
                out.parameter = http::EGASdkErrorParameter::ItemType;
                snprintf(out.reason, 8193, "%s", "");
                return;
            }
            if (!GAValidator::validateEventPartLength(itemType, false))
            {
                logging::GALogger::w("Validation fail - resource event - itemType: Cannot be (null), empty or above 64 characters. String: %s", itemType);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartLength;
                out.parameter = http::EGASdkErrorParameter::ItemType;
                snprintf(out.reason, 8193, "%s", itemType);
                return;
            }
            if (!GAValidator::validateEventPartCharacters(itemType))
            {
                logging::GALogger::w("Validation fail - resource event - itemType: Cannot contain other characters than A-z, 0-9, -_., ()!?. String: %s", itemType);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartCharacters;
                out.parameter = http::EGASdkErrorParameter::ItemType;
                snprintf(out.reason, 8193, "%s", itemType);
                return;
            }
            if (!state::GAState::hasAvailableResourceItemType(itemType))
            {
                logging::GALogger::w("Validation fail - resource event - itemType: Not found in list of pre-defined available resource itemTypes. String: %s", itemType);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::NotFoundInAvailableItemTypes;
                out.parameter = http::EGASdkErrorParameter::ItemType;
                snprintf(out.reason, 8193, "%s", itemType);
                return;
            }
            if (!GAValidator::validateEventPartLength(itemId, false))
            {
                logging::GALogger::w("Validation fail - resource event - itemId: Cannot be (null), empty or above 64 characters. String: %s", itemId);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartLength;
                out.parameter = http::EGASdkErrorParameter::ItemId;
                snprintf(out.reason, 8193, "%s", itemId);
                return;
            }
            if (!GAValidator::validateEventPartCharacters(itemId))
            {
                logging::GALogger::w("Validation fail - resource event - itemId: Cannot contain other characters than A-z, 0-9, -_., ()!?. String: %s", itemId);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ResourceEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartCharacters;
                out.parameter = http::EGASdkErrorParameter::ItemId;
                snprintf(out.reason, 8193, "%s", itemId);
                return;
            }
            out.result = true;
        }

        void GAValidator::validateProgressionEvent(
            EGAProgressionStatus progressionStatus,
            const char* progression01,
            const char* progression02,
            const char* progression03,
            ValidationResult& out
            )
        {
            char progressionStatusString[10] = "";
            events::GAEvents::progressionStatusString(progressionStatus, progressionStatusString);
            if (strlen(progressionStatusString) == 0)
            {
                logging::GALogger::w("Validation fail - progression event: Invalid progression status.");
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ProgressionEvent;
                out.action = http::EGASdkErrorAction::InvalidProgressionStatus;
                out.parameter = http::EGASdkErrorParameter::ProgressionStatus;
                snprintf(out.reason, 8193, "%s", "");
                return;
            }

            // Make sure progressions are defined as either 01, 01+02 or 01+02+03
            if (!utilities::GAUtilities::isStringNullOrEmpty(progression03) && !(!utilities::GAUtilities::isStringNullOrEmpty(progression02) || utilities::GAUtilities::isStringNullOrEmpty(progression01)))
            {
                logging::GALogger::w("Validation fail - progression event: 03 found but 01+02 are invalid. Progression must be set as either 01, 01+02 or 01+02+03.");
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ProgressionEvent;
                out.action = http::EGASdkErrorAction::WrongProgressionOrder;
                out.parameter = (http::EGASdkErrorParameter)0;
                snprintf(out.reason, 8193, "%s:%s:%s", utilities::GAUtilities::isStringNullOrEmpty(progression01) ? "" : progression01, utilities::GAUtilities::isStringNullOrEmpty(progression02) ? "" : progression02, progression03);
                return;
            }
            else if (!utilities::GAUtilities::isStringNullOrEmpty(progression02) && utilities::GAUtilities::isStringNullOrEmpty(progression01))
            {
                logging::GALogger::w("Validation fail - progression event: 02 found but not 01. Progression must be set as either 01, 01+02 or 01+02+03");
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ProgressionEvent;
                out.action = http::EGASdkErrorAction::WrongProgressionOrder;
                out.parameter = (http::EGASdkErrorParameter)0;
                snprintf(out.reason, 8193, ":%s", progression02);
                return;
            }
            else if (utilities::GAUtilities::isStringNullOrEmpty(progression01))
            {
                logging::GALogger::w("Validation fail - progression event: progression01 not valid. Progressions must be set as either 01, 01+02 or 01+02+03");
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ProgressionEvent;
                out.action = http::EGASdkErrorAction::WrongProgressionOrder;
                out.parameter = (http::EGASdkErrorParameter)0;
                snprintf(out.reason, 8193, "%s", "");
                return;
            }

            // progression01 (required)
            if (!GAValidator::validateEventPartLength(progression01, false))
            {
                logging::GALogger::w("Validation fail - progression event - progression01: Cannot be (null), empty or above 64 characters. String: %s", progression01);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ProgressionEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartLength;
                out.parameter = http::EGASdkErrorParameter::Progression01;
                snprintf(out.reason, 8193, "%s", progression01);
                return;
            }
            if (!GAValidator::validateEventPartCharacters(progression01))
            {
                logging::GALogger::w("Validation fail - progression event - progression01: Cannot contain other characters than A-z, 0-9, -_., ()!?. String: %s", progression01);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ProgressionEvent;
                out.action = http::EGASdkErrorAction::InvalidEventPartCharacters;
                out.parameter = http::EGASdkErrorParameter::Progression01;
                snprintf(out.reason, 8193, "%s", progression01);
                return;
            }
            // progression02
            if (strlen(progression02) > 0)
            {
                if (!GAValidator::validateEventPartLength(progression02, true))
                {
                    logging::GALogger::w("Validation fail - progression event - progression02: Cannot be empty or above 64 characters. String: %s", progression02);
                    out.category = http::EGASdkErrorCategory::EventValidation;
                    out.area = http::EGASdkErrorArea::ProgressionEvent;
                    out.action = http::EGASdkErrorAction::InvalidEventPartLength;
                    out.parameter = http::EGASdkErrorParameter::Progression02;
                    snprintf(out.reason, 8193, "%s", progression02);
                    return;
                }
                if (!GAValidator::validateEventPartCharacters(progression02))
                {
                    logging::GALogger::w("Validation fail - progression event - progression02: Cannot contain other characters than A-z, 0-9, -_., ()!?. String: %s", progression02);
                    out.category = http::EGASdkErrorCategory::EventValidation;
                    out.area = http::EGASdkErrorArea::ProgressionEvent;
                    out.action = http::EGASdkErrorAction::InvalidEventPartCharacters;
                    out.parameter = http::EGASdkErrorParameter::Progression02;
                    snprintf(out.reason, 8193, "%s", progression02);
                    return;
                }
            }
            // progression03
            if (strlen(progression03) > 0)
            {
                if (!GAValidator::validateEventPartLength(progression03, true))
                {
                    logging::GALogger::w("Validation fail - progression event - progression03: Cannot be empty or above 64 characters. String: %s", progression03);
                    out.category = http::EGASdkErrorCategory::EventValidation;
                    out.area = http::EGASdkErrorArea::ProgressionEvent;
                    out.action = http::EGASdkErrorAction::InvalidEventPartLength;
                    out.parameter = http::EGASdkErrorParameter::Progression03;
                    snprintf(out.reason, 8193, "%s", progression03);
                    return;
                }
                if (!GAValidator::validateEventPartCharacters(progression03))
                {
                    logging::GALogger::w("Validation fail - progression event - progression03: Cannot contain other characters than A-z, 0-9, -_., ()!?. String: %s", progression03);
                    out.category = http::EGASdkErrorCategory::EventValidation;
                    out.area = http::EGASdkErrorArea::ProgressionEvent;
                    out.action = http::EGASdkErrorAction::InvalidEventPartCharacters;
                    out.parameter = http::EGASdkErrorParameter::Progression03;
                    snprintf(out.reason, 8193, "%s", progression03);
                    return;
                }
            }
            out.result = true;
        }


        void GAValidator::validateDesignEvent(const char* eventId, ValidationResult& out)
        {
            if (!GAValidator::validateEventIdLength(eventId))
            {
                logging::GALogger::w("Validation fail - design event - eventId: Cannot be (null) or empty. Only 5 event parts allowed seperated by :. Each part need to be 64 characters or less. String: %s", eventId);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::DesignEvent;
                out.action = http::EGASdkErrorAction::InvalidEventIdLength;
                out.parameter = http::EGASdkErrorParameter::EventId;
                snprintf(out.reason, 8193, "%s", eventId);
                return;
            }
            if (!GAValidator::validateEventIdCharacters(eventId))
            {
                logging::GALogger::w("Validation fail - design event - eventId: Non valid characters. Only allowed A-z, 0-9, -_., ()!?. String: %s", eventId);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::DesignEvent;
                out.action = http::EGASdkErrorAction::InvalidEventIdCharacters;
                out.parameter = http::EGASdkErrorParameter::EventId;
                snprintf(out.reason, 8193, "%s", eventId);
                return;
            }
            // value: allow 0, negative and nil (not required)
            out.result = true;
        }


        void GAValidator::validateErrorEvent(EGAErrorSeverity severity, const char* message, ValidationResult& out)
        {
            char errorSeverityString[10] = "";
            events::GAEvents::errorSeverityString(severity, errorSeverityString);
            if (strlen(errorSeverityString) == 0)
            {
                logging::GALogger::w("Validation fail - error event - severity: Severity was unsupported value.");
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ErrorEvent;
                out.action = http::EGASdkErrorAction::InvalidSeverity;
                out.parameter = http::EGASdkErrorParameter::Severity;
                snprintf(out.reason, 8193, "%s", "");
                return;
            }
            if (!GAValidator::validateLongString(message, true))
            {
                logging::GALogger::w("Validation fail - error event - message: Message cannot be above 8192 characters. message=%s", message);
                out.category = http::EGASdkErrorCategory::EventValidation;
                out.area = http::EGASdkErrorArea::ErrorEvent;
                out.action = http::EGASdkErrorAction::InvalidLongString;
                out.parameter = http::EGASdkErrorParameter::Message;
                snprintf(out.reason, 8193, "%s", message);
                return;
            }
            out.result = true;
        }

        bool GAValidator::validateSdkErrorEvent(const char* gameKey, const char* gameSecret, http::EGASdkErrorCategory category, http::EGASdkErrorArea area, http::EGASdkErrorAction action)
        {
            if(!validateKeys(gameKey, gameSecret))
            {
                 logging::GALogger::w("validateSdkErrorEvent failed. Game key or secret key is invalid. Can only contain characters A-z 0-9, gameKey is 32 length, gameSecret is 40 length. Failed keys - gameKey: %s, secretKey: %s", gameKey, gameSecret);
                return false;
            }

            char categoryString[40] = "";
            http::GAHTTPApi::sdkErrorCategoryString(category, categoryString);
            if (strlen(categoryString) == 0)
            {
                logging::GALogger::w("Validation fail - sdk error event - category: Category was unsupported value.");
                return false;
            }

            char areaString[40] = "";
            http::GAHTTPApi::sdkErrorAreaString(area, areaString);
            if (strlen(areaString) == 0)
            {
                logging::GALogger::w("Validation fail - sdk error event - area: Area was unsupported value.");
                return false;
            }

            char actionString[40] = "";
            http::GAHTTPApi::sdkErrorActionString(action, actionString);
            if (strlen(actionString) == 0)
            {
                logging::GALogger::w("Validation fail - sdk error event - action: Action was unsupported value.");
                return false;
            }
            return true;
        }


        // event params
        bool GAValidator::validateKeys(const char* gameKey, const char* gameSecret)
        {
            if (utilities::GAUtilities::stringMatch(gameKey, "^[A-z0-9]{32}$"))
            {
                if (utilities::GAUtilities::stringMatch(gameSecret, "^[A-z0-9]{40}$"))
                {
                    return true;
                }
            }
            return false;
        }

        bool GAValidator::validateCurrency(const char* currency)
        {
            if (strlen(currency) == 0)
            {
                return false;
            }
            if (!utilities::GAUtilities::stringMatch(currency, "^[A-Z]{3}$"))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateEventPartLength(const char* eventPart, bool allowNull)
        {
            size_t size = strlen(eventPart);
            if (allowNull == true && size == 0)
            {
                return true;
            }

            if (size == 0)
            {
                return false;
            }

            if (size > 64)
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateEventPartCharacters(const char* eventPart)
        {
            if (!utilities::GAUtilities::stringMatch(eventPart, "^[A-Za-z0-9\\s\\-_\\.\\(\\)\\!\\?]{1,64}$"))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateEventIdLength(const char* eventId)
        {
            if (strlen(eventId) == 0)
            {
                return false;
            }

            if (!utilities::GAUtilities::stringMatch(eventId, "^[^:]{1,64}(?::[^:]{1,64}){0,4}$"))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateEventIdCharacters(const char* eventId)
        {
            if (strlen(eventId) == 0)
            {
                return false;
            }

            if (!utilities::GAUtilities::stringMatch(eventId, "^[A-Za-z0-9\\s\\-_\\.\\(\\)\\!\\?]{1,64}(:[A-Za-z0-9\\s\\-_\\.\\(\\)\\!\\?]{1,64}){0,4}$"))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateShortString(const char* shortString, bool canBeEmpty = false)
        {
            size_t size = strlen(shortString);
            // String is allowed to be empty or nil
            if (canBeEmpty && size == 0)
            {
                return true;
            }

            if (size == 0 || size > 32)
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateString(const char* string, bool canBeEmpty = false)
        {
            size_t size = strlen(string);
            // String is allowed to be empty or nil
            if (canBeEmpty && size == 0)
            {
                return true;
            }

            if (size == 0 || size > 64)
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateLongString(const char* longString, bool canBeEmpty = false)
        {
            size_t size = strlen(longString);
            // String is allowed to be empty
            if (canBeEmpty && size == 0)
            {
                return true;
            }

            if (size == 0 || size > 8192)
            {
                return false;
            }
            return true;
        }

        // validate wrapper version, build, engine version, store
        bool GAValidator::validateSdkWrapperVersion(const char* wrapperVersion)
        {
            if (!utilities::GAUtilities::stringMatch(wrapperVersion, "^(unity|unreal|corona|cocos2d|lumberyard|air|gamemaker|defold|godot) [0-9]{0,5}(\\.[0-9]{0,5}){0,2}$"))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateBuild(const char* build)
        {
            if (!GAValidator::validateShortString(build))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateEngineVersion(const char* engineVersion)
        {
            if (!utilities::GAUtilities::stringMatch(engineVersion, "^(unity|unreal|corona|cocos2d|lumberyard|gamemaker|defold|godot) [0-9]{0,5}(\\.[0-9]{0,5}){0,2}$"))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateStore(const char* store)
        {
            return utilities::GAUtilities::stringMatch(store, "^(apple|google_play)$");
        }

        bool GAValidator::validateConnectionType(const char* connectionType)
        {
            return utilities::GAUtilities::stringMatch(connectionType, "^(wwan|wifi|lan|offline)$");
        }

        // dimensions
        bool GAValidator::validateCustomDimensions(const StringVector& customDimensions)
        {
            return GAValidator::validateArrayOfStrings(customDimensions, 20, 32, false, "custom dimensions");

        }

        bool GAValidator::validateResourceCurrencies(const StringVector& resourceCurrencies)
        {
            if (!GAValidator::validateArrayOfStrings(resourceCurrencies, 20, 64, false, "resource currencies"))
            {
                return false;
            }

            // validate each string for regex
            for (CharArray resourceCurrency : resourceCurrencies.getVector())
            {
                if (!utilities::GAUtilities::stringMatch(resourceCurrency.array, "^[A-Za-z]+$"))
                {
                    logging::GALogger::w("resource currencies validation failed: a resource currency can only be A-Z, a-z. String was: %s", resourceCurrency.array);
                    return false;
                }
            }
            return true;
        }

        bool GAValidator::validateResourceItemTypes(const StringVector& resourceItemTypes)
        {
            if (!GAValidator::validateArrayOfStrings(resourceItemTypes, 20, 32, false, "resource item types"))
            {
                return false;
            }

            // validate each resourceItemType for eventpart validation
            for (CharArray resourceItemType : resourceItemTypes.getVector())
            {
                if (!GAValidator::validateEventPartCharacters(resourceItemType.array))
                {
                    logging::GALogger::w("resource item types validation failed: a resource item type cannot contain other characters than A-z, 0-9, -_., ()!?. String was: %s", resourceItemType.array);
                    return false;
                }
            }
            return true;
        }


        bool GAValidator::validateDimension01(const char* dimension01)
        {
            // allow nil
            if (utilities::GAUtilities::isStringNullOrEmpty(dimension01))
            {
                return true;
            }
            if (!state::GAState::hasAvailableCustomDimensions01(dimension01))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateDimension02(const char* dimension02)
        {
            // allow nil
            if (utilities::GAUtilities::isStringNullOrEmpty(dimension02))
            {
                return true;
            }
            if (!state::GAState::hasAvailableCustomDimensions02(dimension02))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateDimension03(const char* dimension03)
        {
            // allow nil
            if (utilities::GAUtilities::isStringNullOrEmpty(dimension03))
            {
                return true;
            }
            if (!state::GAState::hasAvailableCustomDimensions03(dimension03))
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateArrayOfStrings(
            const StringVector& arrayOfStrings,
            size_t maxCount,
            size_t maxStringLength,
            bool allowNoValues,
            const char* logTag
            )
        {
            char arrayTag[33] = "";
            snprintf(arrayTag, sizeof(arrayTag), "%s", logTag);

            // use arrayTag to annotate warning log
            if (strlen(arrayTag) == 0)
            {
                snprintf(arrayTag, sizeof(arrayTag), "%s", "Array");
            }

            // check if empty
            if (allowNoValues == false && arrayOfStrings.getVector().size() == 0)
            {
                logging::GALogger::w("%s validation failed: array cannot be empty.", arrayTag);
                return false;
            }

            // check if exceeding max count
            if (maxCount && maxCount > static_cast<size_t>(0) && arrayOfStrings.getVector().size() > maxCount)
            {
                logging::GALogger::w("%s alidation failed: array cannot exceed %lu values. It has %lu values.", arrayTag, maxCount, arrayOfStrings.getVector().size());
                return false;
            }

            // validate each string
            for (CharArray arrayString : arrayOfStrings.getVector())
            {
                size_t stringLength = strlen(arrayString.array);
                // check if empty (not allowed)
                if (stringLength == 0)
                {
                    logging::GALogger::w("%s validation failed: contained an empty string.", arrayTag);
                    return false;
                }

                // check if exceeding max length
                if (maxStringLength && maxStringLength > static_cast<size_t>(0) && stringLength > maxStringLength)
                {
                    logging::GALogger::w("%s validation failed: a string exceeded max allowed length (which is: %lu). String was: %s", arrayTag, maxStringLength, arrayString.array);
                    return false;
                }
            }
            return true;
        }

        bool GAValidator::validateClientTs(int64_t clientTs)
        {
            if (clientTs < 0 || clientTs > 99999999999)
            {
                return false;
            }
            return true;
        }

        bool GAValidator::validateUserId(const char* uId)
        {
            if (strlen(uId) == 0)
            {
                logging::GALogger::w("Validation fail - user id cannot be empty.");
                return false;
            }
            return true;
        }

        void GAValidator::validateAndCleanInitRequestResponse(const rapidjson::Value& initResponse, rapidjson::Document& out, bool configsCreated)
        {
            // make sure we have a valid dict
            if (initResponse.IsNull())
            {
                logging::GALogger::w("validateInitRequestResponse failed - no response dictionary.");
                rapidjson::Value v(rapidjson::kObjectType);
                out.SetNull();
                return;
            }

            out.SetObject();
            rapidjson::Document::AllocatorType& allocator = out.GetAllocator();

            // validate server_ts
            if (initResponse.HasMember("server_ts") && initResponse["server_ts"].IsNumber())
            {
                int64_t serverTsNumber = initResponse["server_ts"].GetInt64();
                if (serverTsNumber > 0)
                {
                    out.AddMember("server_ts", serverTsNumber, allocator);
                }
            }

            if(configsCreated)
            {
                if (initResponse.HasMember("configs") && initResponse["configs"].IsArray())
                {
                    rapidjson::Value configurations = rapidjson::Value(initResponse["configs"], allocator);
                    out.AddMember("configs", configurations, allocator);
                }
                if (initResponse.HasMember("configs_hash") && initResponse["configs_hash"].IsString())
                {
                    rapidjson::Value configs_hash = rapidjson::Value(initResponse["configs_hash"].GetString(), allocator);
                    out.AddMember("configs_hash", configs_hash, allocator);
                }
                if (initResponse.HasMember("ab_id") && initResponse["ab_id"].IsString())
                {
                    rapidjson::Value ab_id = rapidjson::Value(initResponse["ab_id"].GetString(), allocator);
                    out.AddMember("ab_id", ab_id, allocator);
                }
                if (initResponse.HasMember("ab_variant_id") && initResponse["ab_variant_id"].IsString())
                {
                    rapidjson::Value ab_variant_id = rapidjson::Value(initResponse["ab_variant_id"].GetString(), allocator);
                    out.AddMember("ab_variant_id", ab_variant_id, allocator);
                }
            }
        }
    }
}
