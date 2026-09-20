#include <utility>

#include <gtest/gtest.h>

#include "service/matchmaking/MasterDetailsByAddress.h"

using service::matchmaking::MasterDetailsByAddress;

namespace
{
    MasterServerEntry Entry(uint32_t ip, uint16_t port, MasterDetails details)
    {
        return MasterServerEntry{netadr_t(ip, port), std::move(details)};
    }
} // namespace

TEST(MasterDetailsByAddressTest, FindsDetailsByAddressAndPort)
{
    MasterDetailsByAddress index;
    index.Rebuild({Entry(0x01020304, 27015, MasterDetails{"GunGame", "RU"})});

    const MasterDetails* details = index.Find(0x01020304, 27015);

    ASSERT_NE(details, nullptr);
    EXPECT_EQ(details->game_mode, "GunGame");
    EXPECT_EQ(details->country_code, "RU");
    EXPECT_EQ(index.Find(0x01020304, 27016), nullptr);
    EXPECT_EQ(index.Find(0x01020305, 27015), nullptr);
}

TEST(MasterDetailsByAddressTest, RebuildDropsWhatTheNewListNoLongerReports)
{
    MasterDetailsByAddress index;
    index.Rebuild({Entry(0x01020304, 27015, MasterDetails{"GunGame", "RU"}), Entry(0x05060708, 27015, MasterDetails{"Surf"})});
    index.Rebuild({Entry(0x01020304, 27015, MasterDetails{})});

    EXPECT_EQ(index.Find(0x01020304, 27015), nullptr);
    EXPECT_EQ(index.Find(0x05060708, 27015), nullptr);
}

TEST(MasterDetailsByAddressTest, EntriesWithoutDetailsAreLeftOut)
{
    MasterDetailsByAddress index;
    index.Rebuild({Entry(0x01020304, 27015, MasterDetails{}), Entry(0x05060708, 27015, MasterDetails{"", "DE"})});

    EXPECT_EQ(index.Find(0x01020304, 27015), nullptr);
    ASSERT_NE(index.Find(0x05060708, 27015), nullptr);
    EXPECT_EQ(index.Find(0x05060708, 27015)->country_code, "DE");
}
