
#include <gtest/gtest.h>
#include <strtools.h>

#include "service/discord/DiscordValidation.h"

TEST(DiscordValidationTest, AcceptsIpWithPort)
{
    EXPECT_TRUE(Discord_IsSafeJoinAddress("46.174.50.220:27015"));
    EXPECT_TRUE(Discord_IsSafeJoinAddress("0.0.0.0:0"));
    EXPECT_TRUE(Discord_IsSafeJoinAddress("255.255.255.255:65535"));
}

TEST(DiscordValidationTest, RejectsAddressesThatInjectCommands)
{
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3.4:27015;quit"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3.4:27015\nquit"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3.4 27015"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("\"1.2.3.4:27015\""));
}

TEST(DiscordValidationTest, RejectsAnythingButIpWithPort)
{
    EXPECT_FALSE(Discord_IsSafeJoinAddress(nullptr));
    EXPECT_FALSE(Discord_IsSafeJoinAddress(""));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("::::"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("cs.example-server.ru:27015"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3.4"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3.4:"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3:27015"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3.4.5:27015"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1..3.4:27015"));
}

TEST(DiscordValidationTest, RejectsOutOfRangeNumbers)
{
    EXPECT_FALSE(Discord_IsSafeJoinAddress("256.1.1.1:27015"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3.4:65536"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("1.2.3.4:4294967297"));
}

TEST(DiscordValidationTest, RejectsNonAsciiAddresses)
{
    EXPECT_FALSE(Discord_IsSafeJoinAddress("\xD1\x81\xD0\xB5\xD1\x80\xD0\xB2\xD0\xB5\xD1\x80.\xD1\x80\xD1\x84:27015"));
    EXPECT_FALSE(Discord_IsSafeJoinAddress("\xD9\xA1.2.3.4:27015"));
}

TEST(DiscordValidationTest, AcceptsUtf8OfEveryLength)
{
    EXPECT_TRUE(Q_UnicodeValidate("[RU] Public 24/7"));
    EXPECT_TRUE(Q_UnicodeValidate("[RU] \xD0\x9F\xD0\xB0\xD0\xB1\xD0\xBB\xD0\xB8\xD0\xBA"));
    EXPECT_TRUE(Q_UnicodeValidate("\xE2\x82\xAC"));
    EXPECT_TRUE(Q_UnicodeValidate("Server \xF0\x9F\x94\xA5"));
}

TEST(DiscordValidationTest, RejectsCp1251Names)
{
    // "[RU] Паблик" as the old Russian servers send it
    EXPECT_FALSE(Q_UnicodeValidate("[RU] \xCF\xE0\xE1\xEB\xE8\xEA"));
}

TEST(DiscordValidationTest, RejectsCutAndStrayBytes)
{
    EXPECT_FALSE(Q_UnicodeValidate("abc\xD0"));
    EXPECT_FALSE(Q_UnicodeValidate("\xF0\x9F\x94"));
    EXPECT_FALSE(Q_UnicodeValidate("\x9F" "abc"));
    EXPECT_FALSE(Q_UnicodeValidate("\xFF"));
}

TEST(DiscordValidationTest, RejectsInvalidCodePoints)
{
    EXPECT_FALSE(Q_UnicodeValidate("\xC0\xAF"));
    EXPECT_FALSE(Q_UnicodeValidate("\xED\xA0\x80"));
    EXPECT_FALSE(Q_UnicodeValidate("\xF4\x90\x80\x80"));
    EXPECT_TRUE(Q_UnicodeValidate("\xEF\xBF\xBD"));
    EXPECT_TRUE(Q_UnicodeValidate("\xF4\x8F\xBF\xBD"));
}
