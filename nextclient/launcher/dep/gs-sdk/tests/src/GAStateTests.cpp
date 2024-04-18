//
// GA-SDK-CPP
// Copyright 2015 GameAnalytics. All rights reserved.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <GAState.h>
#include "rapidjson/document.h"

#include "helpers/GATestHelpers.h"

TEST(GAStateTest, testValidateAndCleanCustomFields)
{
    rapidjson::Document map;
    rapidjson::Value v;

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        while(map.MemberCount() < 100)
        {
            map.AddMember(rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), a);
        }
    }
    ASSERT_EQ(100, map.MemberCount());
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 50);

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        while(map.MemberCount() < 50)
        {
            map.AddMember(rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), a);
        }
    }
    ASSERT_EQ(50, map.MemberCount());
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_EQ(50, v.MemberCount());

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        map.AddMember(rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), rapidjson::Value("", a), a);
    }
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 0);

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        map.AddMember(rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), rapidjson::Value(GATestHelpers::getRandomString(257).c_str(), a), a);
    }
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 0);

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        map.AddMember("", rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), a);
    }
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 0);

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        map.AddMember(rapidjson::Value("___", a), rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), a);
    }
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 1);

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        map.AddMember(rapidjson::Value("_&_", a), rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), a);
    }
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 0);

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        map.AddMember(rapidjson::Value(GATestHelpers::getRandomString(65).c_str(), a), rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), a);
    }
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 0);

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        map.AddMember(rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), rapidjson::Value(100), a);
    }
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 1);

    {
        v = rapidjson::Value();
        map.SetObject();
        rapidjson::Document::AllocatorType& a = map.GetAllocator();
        map.AddMember(rapidjson::Value(GATestHelpers::getRandomString(4).c_str(), a), rapidjson::Value(true), a);
    }
    gameanalytics::state::GAState::validateAndCleanCustomFields(map, v);
    ASSERT_TRUE(v.MemberCount() == 0);
}
