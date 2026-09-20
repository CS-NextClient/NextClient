#include <string>

#include <gtest/gtest.h>

#include "service/matchmaking/master/MasterListCache.h"

namespace
{
    void AppendAddress(std::string& data, uint32_t ip, uint16_t port)
    {
        data.append(reinterpret_cast<const char*>(&ip), sizeof(ip));
        data.append(reinterpret_cast<const char*>(&port), sizeof(port));
    }
} // namespace

TEST(MasterListCacheTest, EncodeDecodeRoundTripKeepsAddressesModesAndCountries)
{
    std::vector<MasterServerEntry> server_list{
        MasterServerEntry{netadr_t(0x01020304, 27015), MasterDetails{"Zombie", "RU"}},
        MasterServerEntry{netadr_t(0xC0A80001, 27016)},
    };

    std::vector<MasterServerEntry> decoded = MasterListCache_Decode(MasterListCache_Encode(server_list));

    ASSERT_EQ(decoded.size(), 2u);
    EXPECT_EQ(decoded[0].address, server_list[0].address);
    EXPECT_EQ(decoded[0].details.game_mode, "Zombie");
    EXPECT_EQ(decoded[0].details.country_code, "RU");
    EXPECT_EQ(decoded[1].address, server_list[1].address);
    EXPECT_TRUE(decoded[1].details.game_mode.empty());
    EXPECT_TRUE(decoded[1].details.country_code.empty());
}

TEST(MasterListCacheTest, LegacyAddressRecordsDecodeWithoutModeOrCountry)
{
    std::string data;
    AppendAddress(data, 0x01020304, 27015);

    std::vector<MasterServerEntry> decoded = MasterListCache_Decode(data);

    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(decoded[0].address, netadr_t(0x01020304, 27015));
    EXPECT_TRUE(decoded[0].details.game_mode.empty());
}

TEST(MasterListCacheTest, LegacyDataDecodesOnlyWholeRecords)
{
    std::string data;
    AppendAddress(data, 0x01020304, 27015);
    AppendAddress(data, 0x05060708, 27016);
    data.push_back('\x01');

    std::vector<MasterServerEntry> decoded = MasterListCache_Decode(data);

    ASSERT_EQ(decoded.size(), 2u);
    EXPECT_EQ(decoded[1].address, netadr_t(0x05060708, 27016));
}

TEST(MasterListCacheTest, HeaderOnlyDecodesToNothing)
{
    EXPECT_TRUE(MasterListCache_Decode(MasterListCache_Encode({})).empty());
}

TEST(MasterListCacheTest, UnknownLayoutVersionDecodesToNothing)
{
    std::string data = MasterListCache_Encode({MasterServerEntry{netadr_t(0x01020304, 27015), MasterDetails{"Surf"}}});
    data[4] = 9;

    EXPECT_TRUE(MasterListCache_Decode(data).empty());
}

TEST(MasterListCacheTest, TruncatedRecordIsIgnored)
{
    std::string data = MasterListCache_Encode({MasterServerEntry{netadr_t(0x01020304, 27015), MasterDetails{"Surf", "UA"}}});
    data.append("\x05\x06\x07\x08\x09\x0a\x03\x41", 8);

    std::vector<MasterServerEntry> decoded = MasterListCache_Decode(data);

    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(decoded[0].details.game_mode, "Surf");
    EXPECT_EQ(decoded[0].details.country_code, "UA");
}

TEST(MasterListCacheTest, RecordEndingAfterOrInsideItsAddressIsIgnored)
{
    std::string complete = MasterListCache_Encode({MasterServerEntry{netadr_t(0x01020304, 27015), MasterDetails{"Surf", "UA"}}});

    std::string ending_after_address = complete;
    AppendAddress(ending_after_address, 0x05060708, 27016);

    std::string ending_inside_address = complete;
    ending_inside_address.append("\x05\x06\x07", 3);

    EXPECT_EQ(MasterListCache_Decode(ending_after_address).size(), 1u);
    EXPECT_EQ(MasterListCache_Decode(ending_inside_address).size(), 1u);
}

TEST(MasterListCacheTest, DecodedModesAndCountriesKeepOnlyTheirMasterDetailsForms)
{
    std::vector<MasterServerEntry> server_list{
        MasterServerEntry{netadr_t(0x01020304, 27015), MasterDetails{"!img:1", "ua"}},
        MasterServerEntry{netadr_t(0x01020304, 27016), MasterDetails{std::string(300, 'A'), "\xD0\xAF"}},
        MasterServerEntry{netadr_t(0x01020304, 27017), MasterDetails{"\xD0\x97\xD0\xBE\xD0\xBC\xD0\xB1\xD0\xB8", "RUS"}},
    };

    std::vector<MasterServerEntry> decoded = MasterListCache_Decode(MasterListCache_Encode(server_list));

    ASSERT_EQ(decoded.size(), 3u);
    EXPECT_TRUE(decoded[0].details.game_mode.empty());
    EXPECT_EQ(decoded[0].details.country_code, "UA");
    EXPECT_TRUE(decoded[1].details.game_mode.empty());
    EXPECT_TRUE(decoded[1].details.country_code.empty());
    EXPECT_EQ(decoded[2].address, netadr_t(0x01020304, 27017));
    EXPECT_TRUE(decoded[2].details.game_mode.empty());
    EXPECT_TRUE(decoded[2].details.country_code.empty());
}
