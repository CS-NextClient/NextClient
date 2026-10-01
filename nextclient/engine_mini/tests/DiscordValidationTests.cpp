#include <string>

#include <gtest/gtest.h>

#include "service/discord/DiscordValidation.h"

TEST(DiscordValidationTest, AcceptsIpAndHostnameAddresses)
{
    EXPECT_TRUE(IsSafeServerAddress("46.174.50.220:27015"));
    EXPECT_TRUE(IsSafeServerAddress("cs.example-server.ru:27015"));
}

TEST(DiscordValidationTest, RejectsAddressesThatInjectCommands)
{
    EXPECT_FALSE(IsSafeServerAddress("1.2.3.4:27015;quit"));
    EXPECT_FALSE(IsSafeServerAddress("1.2.3.4:27015\nquit"));
    EXPECT_FALSE(IsSafeServerAddress("1.2.3.4 27015"));
    EXPECT_FALSE(IsSafeServerAddress("\"1.2.3.4\""));
}

TEST(DiscordValidationTest, RejectsMissingEmptyAndOverlongAddresses)
{
    EXPECT_FALSE(IsSafeServerAddress(nullptr));
    EXPECT_FALSE(IsSafeServerAddress(""));
    EXPECT_TRUE(IsSafeServerAddress(std::string(63, 'a').c_str()));
    EXPECT_FALSE(IsSafeServerAddress(std::string(64, 'a').c_str()));
}

TEST(DiscordValidationTest, RejectsNonAsciiAddresses)
{
    EXPECT_FALSE(IsSafeServerAddress("\xD1\x81\xD0\xB5\xD1\x80\xD0\xB2\xD0\xB5\xD1\x80.\xD1\x80\xD1\x84:27015"));
}

TEST(DiscordValidationTest, AcceptsUtf8OfEveryLength)
{
    EXPECT_TRUE(IsValidUtf8("[RU] Public 24/7"));
    EXPECT_TRUE(IsValidUtf8("[RU] \xD0\x9F\xD0\xB0\xD0\xB1\xD0\xBB\xD0\xB8\xD0\xBA"));
    EXPECT_TRUE(IsValidUtf8("\xE2\x82\xAC"));
    EXPECT_TRUE(IsValidUtf8("Server \xF0\x9F\x94\xA5"));
}

TEST(DiscordValidationTest, RejectsCp1251Names)
{
    // "[RU] Паблик" as the old Russian servers send it
    EXPECT_FALSE(IsValidUtf8("[RU] \xCF\xE0\xE1\xEB\xE8\xEA"));
}

TEST(DiscordValidationTest, RejectsCutAndStrayBytes)
{
    EXPECT_FALSE(IsValidUtf8("abc\xD0"));
    EXPECT_FALSE(IsValidUtf8("\xF0\x9F\x94"));
    EXPECT_FALSE(IsValidUtf8("\x9F" "abc"));
    EXPECT_FALSE(IsValidUtf8("\xFF"));
}
