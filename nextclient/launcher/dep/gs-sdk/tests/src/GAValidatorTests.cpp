//
// GA-SDK-CPP
// Copyright 2015 GameAnalytics. All rights reserved.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <GAValidator.h>
#include <GAState.h>
#include <GALogger.h>
#include <random>
#include <GameAnalytics.h>
#include <GAUtilities.h>

// test helpers
#include "helpers/GATestHelpers.h"
#include "fixtures/GAValidatorTest.h"

TEST(GAValidatorTest, testValidateCurrency)
{
    gameanalytics::logging::GALogger::setInfoLog(true);

    ASSERT_TRUE(gameanalytics::validators::GAValidator::validateCurrency("USD"));
    ASSERT_TRUE(gameanalytics::validators::GAValidator::validateCurrency("XXX"));

    ASSERT_FALSE(gameanalytics::validators::GAValidator::validateCurrency("usd"));
    ASSERT_FALSE(gameanalytics::validators::GAValidator::validateCurrency("US"));
    ASSERT_FALSE(gameanalytics::validators::GAValidator::validateCurrency("KR"));
    ASSERT_FALSE(gameanalytics::validators::GAValidator::validateCurrency("USDOLLARS"));
    ASSERT_FALSE(gameanalytics::validators::GAValidator::validateCurrency("$"));
    ASSERT_FALSE(gameanalytics::validators::GAValidator::validateCurrency(""));
}

TEST(GAValidator, testValidateResourceCurrencies)
{
    // Store result
    bool isValid;

    {
        gameanalytics::StringVector currencies;
        currencies.add("gems").add("gold");

        // Valid resource types
        isValid = gameanalytics::validators::GAValidator::validateResourceCurrencies(currencies);
        ASSERT_TRUE(isValid) << "Valid resource types array should succeed";
    }


    {
        gameanalytics::StringVector currencies;
        currencies.add("").add("gold");

        // Invalid resource types
        isValid = gameanalytics::validators::GAValidator::validateResourceCurrencies(currencies);
        ASSERT_FALSE(isValid) << "Should falset allow empty resource type";
    }

    {
        gameanalytics::StringVector currencies;
        isValid = gameanalytics::validators::GAValidator::validateResourceCurrencies(currencies);
        ASSERT_FALSE(isValid) << "Should falset allow empty array";
    }
}

 TEST(GAValidator, testValidateResourceItemTypes)
 {
     // Store result
     bool isValid;

     {
         gameanalytics::StringVector itemTypes;
         itemTypes.add("gems").add("gold");
         // Valid resource types
         isValid = gameanalytics::validators::GAValidator::validateResourceItemTypes(itemTypes);
         ASSERT_TRUE(isValid) << "Valid resource types array should succeed";
     }


     {
         gameanalytics::StringVector itemTypes;
         itemTypes.add("").add("gold");
         // Invalid resource types
         isValid = gameanalytics::validators::GAValidator::validateResourceItemTypes(itemTypes);
         ASSERT_FALSE(isValid) << "Should falset allow empty resource type";
     }


     {
         gameanalytics::StringVector itemTypes;
         isValid = gameanalytics::validators::GAValidator::validateResourceItemTypes(itemTypes);
         ASSERT_FALSE(isValid) << "Should falset allow empty array";
     }

 }

// Events

 TEST(GAValidator, testValidateProgressionEvent)
 {
     // Store result
     bool isValid;

     // Valids
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateProgressionEvent(gameanalytics::Start, "world_001", "level_001", "phase_001", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Should allow progression 01-02-03";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateProgressionEvent(gameanalytics::Start, "world_001", "level_001", "", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Should allow progression 01-02";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateProgressionEvent(gameanalytics::Start, "world_001", "", "", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Should allow progression 01";
     }

     // Invalids (TODO)
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateProgressionEvent(gameanalytics::Start, "", "", "", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Should falset allow false progressions";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateProgressionEvent(gameanalytics::Start, "world_001", "", "phase_001", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Should falset allow progression 01-03";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateProgressionEvent(gameanalytics::Start, "", "level_001", "phase_001", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Should falset allow progression 02-03";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateProgressionEvent(gameanalytics::Start, "", "level_001", "", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Should falset allow progression 02";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateProgressionEvent(gameanalytics::Start, "", "", "phase_001", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Should falset allow progression 03";
     }
 }

 TEST(GAValidator, testValidateBusinessEvent)
 {
     // Store result
     bool isValid;

     // Valid business events
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateBusinessEvent("USD", 99, "cartType", "itemType", "itemId", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Valid business event should succeed";
     }

     // Should allow false carttype (optional)
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateBusinessEvent("USD", 99, "", "itemType", "itemId", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Business event cartType should be optional";
     }

     // Should allow 0 amount
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateBusinessEvent("USD", 0, "", "itemType", "itemId", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Business event should allow amount 0";
     }

     // Should not allow negative amount
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateBusinessEvent("USD", -99, "", "itemType", "itemId", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Business event should allow amount less than 0";
     }

     // Should fail on empty item type
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateBusinessEvent("USD", 99, "", "", "itemId", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Business event should not allow empty item type";
     }

     // Should fail on empty item id
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateBusinessEvent("USD", 99, "", "itemType", "", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Business event should not allow empty item id";
     }
 }

 TEST(GAValidator, testValidateResourceSourceEvent)
 {
     // Set available list
     gameanalytics::StringVector currencies;
     currencies.add("gems").add("gold");
     gameanalytics::state::GAState::setAvailableResourceCurrencies(currencies);
     gameanalytics::StringVector itemTypes;
     itemTypes.add("guns").add("powerups");
     gameanalytics::state::GAState::setAvailableResourceItemTypes(itemTypes);

     // Store result
     bool isValid;

     // Valid resource source events
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Source, "gems", 100, "guns", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Valid resource source event should succeed";
     }

     // Valid resource source events
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Source, "gold", 100, "powerups", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Valid resource source event should succeed";
     }

     // falset defined resource type should fail
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Source, "iron", 100, "guns", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow falsen defined resource types";
     }

     // falset defined item type should fail
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Source, "gems", 100, "cows", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow falsen defined item types";
     }

     // Should falset allow negative amount
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Source, "gems", -10, "guns", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow negative amount";
     }

     // Should falset allow false item id
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Source, "gems", 10, "guns", "", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow empty item id";
     }

     // Should falset allow false item type
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Source, "gems", 10, "", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow empty item type";
     }
 }

 TEST(GAValidator, testValidateResourceSinkEvent)
 {
     // Set available list
     gameanalytics::StringVector currencies;
     currencies.add("gems").add("gold");
     gameanalytics::state::GAState::setAvailableResourceCurrencies(currencies);
     gameanalytics::StringVector itemTypes;
     itemTypes.add("guns").add("powerups");
     gameanalytics::state::GAState::setAvailableResourceItemTypes(itemTypes);

     // Store result
     bool isValid;

     // Valid resource source events
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Sink, "gems", 100, "guns", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Valid resource source event should succeed";
     }

     // Valid resource source events
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Sink, "gold", 100, "powerups", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Valid resource source event should succeed";
     }

     // falset defined resource type should fail
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Sink, "iron", 100, "guns", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow falsen defined resource types";
     }

     // falset defined item type should fail
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Sink, "gems", 100, "cows", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow falsen defined item types";
     }

     // Should falset allow 0 amount
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Sink, "gems", 0, "guns", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow 0 amount";
     }

     // Should falset allow negative amount
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Sink, "gems", -10, "guns", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow negative amount";
     }

     // Should falset allow false item id
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Sink, "gems", 10, "guns", "", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow empty item id";
     }

     // Should falset allow false item type
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateResourceEvent(gameanalytics::Sink, "gems", 10, "", "item", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Resource event should falset allow empty item id";
     }
 }

 TEST(GAValidator, testValidateDesignEvent)
 {
     // Store result
     bool isValid;

     // Valid
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateDesignEvent("name:name", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Design event should allow nil value";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateDesignEvent("name:name:name", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Design event should allow nil value";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateDesignEvent("name:name:name:name", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Design event should allow nil value";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateDesignEvent("name:name:name:name:name", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Design event should allow nil value";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateDesignEvent("name:name", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Design event should allow nil value";
     }

     // Invalid
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateDesignEvent("", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Design event should falset allow empty event string";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateDesignEvent("name:name:name:name:name:name", validationResult);
         isValid = validationResult.result;
         ASSERT_FALSE(isValid) << "Design event should falset allow more than 5 values in event string";
     }
 }

 TEST(GAValidator, testValidateErrorEvent)
 {
     // Store result
     bool isValid;

     // Valid
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateErrorEvent(gameanalytics::Error, "This is a message", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Error event should validate";
     }

     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateErrorEvent(gameanalytics::Error, "", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Error event should allow empty message";
     }
 }

 TEST(GAValidator, testValidateSdkErrorEvent)
 {
     // Store result
     bool isValid;

     // Valid
     {
         gameanalytics::validators::ValidationResult validationResult;
         gameanalytics::validators::GAValidator::validateErrorEvent(gameanalytics::Error, "", validationResult);
         isValid = validationResult.result;
         ASSERT_TRUE(isValid) << "Error event should allow empty message";
     }
     isValid = gameanalytics::validators::GAValidator::validateSdkErrorEvent("c6cfc80ff69d1e7316bf1e0c8194eda6", "e0ae4809f70e2fa96916c7060f417ae53895f18d", gameanalytics::http::EventValidation, gameanalytics::http::ResourceEvent, gameanalytics::http::InvalidFlowType);
     ASSERT_TRUE(isValid) << "Error event should validate";
 }

// Dimensions

 TEST(GAValidator, testCustomDimensionsValidator)
 {
     // Store result
     bool isValid;

     {
         gameanalytics::StringVector dimensions;
         dimensions.add("abc").add("def").add("ghi");
         // Valid
         isValid = gameanalytics::validators::GAValidator::validateCustomDimensions(dimensions);
         ASSERT_TRUE(isValid) << "Should validate custom dimensions";
     }

     {
         gameanalytics::StringVector dimensions;
         dimensions.add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def").add("abc").add("def");
         // Invalid
         isValid = gameanalytics::validators::GAValidator::validateCustomDimensions(dimensions);
         ASSERT_FALSE(isValid) << "Should falset allow more than 20 custom dimensions";
     }

     {
         gameanalytics::StringVector dimensions;
         dimensions.add("abc").add("");
         isValid = gameanalytics::validators::GAValidator::validateCustomDimensions(dimensions);
         ASSERT_FALSE(isValid) << "Should falset allow empty custom dimension value";
     }

     // canfalset happen in c++
     //isValid = GAValidator::validateCustomDimensionsWithCustomDimensions({"abc", 10]];
     //ASSERT_FALSE(isValid) << "Should falset allow falsen string custom dimension value";
 }

 // SDK wrapper version
 TEST(GAValidator, testValidateSdkWrapperVersion)
 {
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("123"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("test 1.2.x"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("unkfalsewn 1.5.6"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("unreal 1.2.3.4"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("Unreal 1.2"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("corona1.2.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("unreal x.2.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("unreal 1.x.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("unreal 1.2.x"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("marmalade 1.2.3"));

     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("unreal 1.2.3"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("corona 1.2"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("lumberyard 1"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("cocos2d 1.2.3"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("unreal 1233.101.0"));
 }

 // build
 TEST(GAValidator, testValidateBuild)
 {
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateBuild(""));
     //ASSERT_FALSE(GAValidator::validateBuild(0));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateBuild(GATestHelpers::get40CharsString().c_str()));

     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateBuild("alpha 1.2.3"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateBuild("ALPHA 1.2.3"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateBuild("TES# sdf.fd3"));
 }

 // engine version
 TEST(GAValidator, testValidateEngineVersion)
 {
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion(""));
     //ASSERT_FALSE(GAValidator::validateEngineVersion(0));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion(GATestHelpers::get40CharsString().c_str()));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion("uni 1.2.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion("unity 123456.2.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion("unity1.2.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion("unreal 1.2.3.4"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion("Unreal 1.2.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion("UNREAL 1.2.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion("marmalade 1.2.3"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEngineVersion("xamarin 1.2.3"));

     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEngineVersion("unreal 1.2.3"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEngineVersion("unreal 1.2"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEngineVersion("unreal 1"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEngineVersion("cocos2d 1.2.3"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEngineVersion("unreal 1.2.3"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("corona 1.2.3"));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateSdkWrapperVersion("lumberyard 1.2.3"));
 }

 // event params
 TEST(GAValidator, testValidateKeys)
 {
     auto validGameKey = "123456789012345678901234567890ab";
     auto validSecretKey = "123456789012345678901234567890123456789a";

     auto tooLongKey = "123456789012345678901234567890123456789abcdefg";

     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateKeys(validGameKey, ""));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateKeys(validGameKey, "123"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateKeys(validGameKey, tooLongKey));

     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateKeys("", validSecretKey));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateKeys("123", validSecretKey));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateKeys(tooLongKey, validSecretKey));

     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateKeys(validGameKey, validSecretKey));
 }

 TEST(GAValidator, testValidateEventPartLength)
 {
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventPartLength(GATestHelpers::get40CharsString().c_str(), true));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventPartLength(GATestHelpers::get40CharsString().c_str(), false));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventPartLength(GATestHelpers::get80CharsString().c_str(), true));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventPartLength(GATestHelpers::get80CharsString().c_str(), false));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventPartLength("", false));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventPartLength("", true));

     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventPartLength("sdfdf", false));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventPartLength(GATestHelpers::get32CharsString().c_str(), true));
 }

 TEST(GAValidator, testValidateEventPartCharacters)
 {
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventPartCharacters("øææ"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventPartCharacters(""));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventPartCharacters("*"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventPartCharacters("))&%"));

     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventPartCharacters("sdfdffdgdfg"));
 }

 TEST(GAValidator, testValidateEventIdLength)
 {
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventIdLength(GATestHelpers::get40CharsString().c_str()));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventIdLength(GATestHelpers::get32CharsString().c_str()));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventIdLength("sdfdf"));

     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventIdLength(GATestHelpers::get80CharsString().c_str()));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventIdLength(""));
 }

 TEST(GAValidator, testValidateEventIdCharacters)
 {
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateEventIdCharacters("GHj:df(g?h d_fk7-58.9)3!47"));

     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventIdCharacters("GHj:df(g?h d_fk,7-58.9)3!47"));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateEventIdCharacters(""));
 }

 TEST(GAValidator, testValidateShortString)
 {
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateShortString(GATestHelpers::getRandomString(32).c_str(), false));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateShortString(GATestHelpers::getRandomString(32).c_str(), true));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateShortString(GATestHelpers::getRandomString(10).c_str(), false));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateShortString(GATestHelpers::getRandomString(10).c_str(), true));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateShortString("", true));

     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateShortString(GATestHelpers::getRandomString(40).c_str(), false));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateShortString(GATestHelpers::getRandomString(40).c_str(), true));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateShortString("", false));
 }

 TEST(GAValidator, testValidateString)
 {
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(64).c_str(), false));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(64).c_str(), true));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(10).c_str(), false));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(10).c_str(), true));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString("", true));

     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(80).c_str(), false));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(80).c_str(), true));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateString("", false));
 }

 TEST(GAValidator, testValidateLongString)
 {
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(64).c_str(), false));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(64).c_str(), true));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(10).c_str(), false));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(10).c_str(), true));
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateString("", true));

     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(80).c_str(), false));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateString(GATestHelpers::getRandomString(80).c_str(), true));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateString("", false));
 }

 // array of strings
 TEST(GAValidator, testValidateArrayOfStringsWithArray)
 {
     {
         gameanalytics::StringVector strings;
         strings.add(GATestHelpers::getRandomString(3).c_str()).add(GATestHelpers::getRandomString(10).c_str()).add(GATestHelpers::getRandomString(7).c_str());
         ASSERT_TRUE(gameanalytics::validators::GAValidator::validateArrayOfStrings(strings, 3, 10, false, "test"));
     }
     {
         gameanalytics::StringVector strings;
         ASSERT_TRUE(gameanalytics::validators::GAValidator::validateArrayOfStrings(strings, 3, 10, true, "test"));
     }

     {
         gameanalytics::StringVector strings;
         strings.add(GATestHelpers::getRandomString(3).c_str()).add(GATestHelpers::getRandomString(12).c_str()).add(GATestHelpers::getRandomString(7).c_str());
         ASSERT_FALSE(gameanalytics::validators::GAValidator::validateArrayOfStrings(strings, 3, 10, false, "test"));
     }
     {
         gameanalytics::StringVector strings;
         strings.add(GATestHelpers::getRandomString(3).c_str()).add("").add(GATestHelpers::getRandomString(7).c_str());
         ASSERT_FALSE(gameanalytics::validators::GAValidator::validateArrayOfStrings(strings, 3, 10, false, "test"));
     }
     {
         gameanalytics::StringVector strings;
         strings.add(GATestHelpers::getRandomString(3).c_str()).add(GATestHelpers::getRandomString(10).c_str()).add(GATestHelpers::getRandomString(7).c_str());
         ASSERT_FALSE(gameanalytics::validators::GAValidator::validateArrayOfStrings(strings, 2, 10, false, "test"));
     }
     {
         gameanalytics::StringVector strings;
         ASSERT_FALSE(gameanalytics::validators::GAValidator::validateArrayOfStrings(strings, 3, 10, false, "test"));
     }

 }

 TEST(GAValidator, testValidateClientTs)
 {
     ASSERT_TRUE(gameanalytics::validators::GAValidator::validateClientTs(gameanalytics::utilities::GAUtilities::timeIntervalSince1970()));

     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateClientTs(std::numeric_limits<long>::min()));
     ASSERT_FALSE(gameanalytics::validators::GAValidator::validateClientTs(std::numeric_limits<long>::max()));
 }

TEST(GAValidator, testValidateUserId)
{
    ASSERT_TRUE(gameanalytics::validators::GAValidator::validateUserId("fhjkdfghdfjkgh"));

    ASSERT_FALSE(gameanalytics::validators::GAValidator::validateUserId(""));
}
