#include <gtest/gtest.h>

#include "GameUi/Controls/PropertyPageNavigation.h"

namespace
{
    class PropertyPageNavigationTest : public ::testing::Test
    {
    protected:
        int identities_[3]{};
        vgui2::Panel* first_ = reinterpret_cast<vgui2::Panel*>(&identities_[0]);
        vgui2::Panel* plugins_ = reinterpret_cast<vgui2::Panel*>(&identities_[1]);
        vgui2::Panel* last_ = reinterpret_cast<vgui2::Panel*>(&identities_[2]);
        PropertyPageNavigation navigation_;

        void SetUp() override
        {
            navigation_.AddPage(first_);
            navigation_.AddPage(plugins_);
            navigation_.AddPage(last_);
        }
    };
} // namespace

TEST_F(PropertyPageNavigationTest, DisabledPageIsSkippedInBothDirectionsAndAtWraparound)
{
    navigation_.SetPageEnabled(plugins_, false);
    EXPECT_FALSE(navigation_.is_page_enabled(plugins_));
    EXPECT_EQ(navigation_.FindEnabled(1), 2);
    EXPECT_EQ(navigation_.FindEnabled(1, -1), 0);
    EXPECT_EQ(navigation_.FindEnabled(3), 0);
    EXPECT_EQ(navigation_.FindEnabled(-1, -1), 2);
    navigation_.SetPageEnabled(first_, false);
    EXPECT_EQ(navigation_.FindEnabled(0), 2);
    navigation_.SetPageEnabled(last_, false);
    EXPECT_EQ(navigation_.FindEnabled(0), -1);
}

TEST_F(PropertyPageNavigationTest, StateFollowsPageIdentityAfterRemovalAndCanBeReenabled)
{
    navigation_.SetPageEnabled(plugins_, false);
    navigation_.RemovePage(first_);
    EXPECT_FALSE(navigation_.is_page_enabled(first_));
    EXPECT_FALSE(navigation_.is_page_enabled(plugins_));
    EXPECT_TRUE(navigation_.is_page_enabled(last_));
    EXPECT_EQ(navigation_.FindEnabled(0), 1);
    navigation_.SetPageEnabled(plugins_, true);
    EXPECT_EQ(navigation_.FindEnabled(0), 0);
}

TEST_F(PropertyPageNavigationTest, ExactFitDoesNotReserveSpaceForMore)
{
    const PropertyPageTabLayout layout = navigation_.CalculateLayout({40, 30, 20}, first_, 92, 20, 1);
    EXPECT_EQ(layout.tab_widths, (std::vector<int>{40, 30, 20}));
    EXPECT_EQ(layout.more_width, 0);
}

TEST_F(PropertyPageNavigationTest, OnePixelOfOverflowReservesMoreBeforeChoosingTheLeadingTabs)
{
    const PropertyPageTabLayout layout = navigation_.CalculateLayout({40, 30, 20}, first_, 91, 20, 1);
    EXPECT_EQ(layout.tab_widths, (std::vector<int>{40, 0, 0}));
    EXPECT_EQ(layout.more_width, 50);
}

TEST_F(PropertyPageNavigationTest, RevealingTheLastPageEvictsTheLastVisibleTabAndPreservesOrder)
{
    const PropertyPageTabLayout first = navigation_.CalculateLayout({80, 70, 60}, first_, 200, 40, 1);
    EXPECT_EQ(first.tab_widths, (std::vector<int>{80, 70, 0}));
    EXPECT_EQ(first.more_width, 48);
    const PropertyPageTabLayout last = navigation_.CalculateLayout({80, 70, 60}, last_, 200, 40, 1);
    EXPECT_EQ(last.tab_widths, (std::vector<int>{80, 0, 60}));
    EXPECT_EQ(last.more_width, 58);
}

TEST_F(PropertyPageNavigationTest, MoreFillsAdditionalWidthWithoutChangingTheVisibleTabs)
{
    const PropertyPageTabLayout original = navigation_.CalculateLayout({80, 70, 60}, first_, 200, 40, 1);
    const PropertyPageTabLayout wider = navigation_.CalculateLayout({80, 70, 60}, first_, 205, 40, 1);
    EXPECT_EQ(wider.tab_widths, original.tab_widths);
    EXPECT_EQ(original.more_width, 48);
    EXPECT_EQ(wider.more_width, 53);
}

TEST_F(PropertyPageNavigationTest, LaterSmallTabsDoNotFillGapsBeyondTheLeadingSequence)
{
    const PropertyPageTabLayout layout = navigation_.CalculateLayout({200, 20, 20}, last_, 100, 30, 1);
    EXPECT_EQ(layout.tab_widths, (std::vector<int>{0, 0, 20}));
    EXPECT_EQ(layout.more_width, 79);
}

TEST_F(PropertyPageNavigationTest, VeryLongActiveTitleIsClampedWithoutHidingTheActiveTab)
{
    const PropertyPageTabLayout layout = navigation_.CalculateLayout({80, 70, 2000}, last_, 200, 40, 1);
    EXPECT_EQ(layout.tab_widths, (std::vector<int>{0, 0, 159}));
    EXPECT_EQ(layout.more_width, 40);
}

TEST_F(PropertyPageNavigationTest, DisabledPagesStillOccupyNavigationSpaceAndKeepTheirIdentity)
{
    navigation_.SetPageEnabled(plugins_, false);
    const PropertyPageTabLayout visible = navigation_.CalculateLayout({40, 30, 20}, first_, 92, 20, 1);
    EXPECT_EQ(visible.tab_widths[1], 30);
    const PropertyPageTabLayout hidden = navigation_.CalculateLayout({40, 30, 20}, last_, 80, 20, 1);
    EXPECT_EQ(hidden.tab_widths[1], 0);
    EXPECT_FALSE(navigation_.is_page_enabled(plugins_));
    EXPECT_EQ(navigation_.FindEnabled(1), 2);
    navigation_.SetPageEnabled(plugins_, true);
    EXPECT_EQ(navigation_.FindEnabled(1), 1);
}

TEST_F(PropertyPageNavigationTest, RepeatedLayoutWithNewFontWidthsDoesNotChangeSelectionEligibility)
{
    navigation_.SetPageEnabled(plugins_, false);
    EXPECT_EQ(navigation_.CalculateLayout({20, 20, 20}, last_, 100, 25, 1).more_width, 0);
    for (int pass = 0; pass < 3; ++pass)
    {
        EXPECT_EQ(navigation_.CalculateLayout({80, 70, 60}, last_, 200, 40, 1).tab_widths, (std::vector<int>{80, 0, 60}));
        EXPECT_FALSE(navigation_.is_page_enabled(plugins_));
        EXPECT_TRUE(navigation_.is_page_enabled(last_));
    }
    EXPECT_EQ(navigation_.CalculateLayout({20, 20, 20}, last_, 100, 25, 1).more_width, 0);
}

TEST_F(PropertyPageNavigationTest, RemovingAHiddenOwnerRejectsStaleSelectionAndKeepsRemainingOrder)
{
    navigation_.RemovePage(plugins_);
    EXPECT_FALSE(navigation_.is_page_enabled(plugins_));
    const PropertyPageTabLayout layout = navigation_.CalculateLayout({80, 60}, last_, 100, 30, 1);
    EXPECT_EQ(layout.tab_widths, (std::vector<int>{0, 60}));
    EXPECT_EQ(navigation_.FindEnabled(0), 0);
}

TEST(PropertyPageNavigation, EmptySheetHasNoOverflow)
{
    PropertyPageNavigation navigation;
    const PropertyPageTabLayout layout = navigation.CalculateLayout({}, nullptr, 200, 40, 1);
    EXPECT_TRUE(layout.tab_widths.empty());
    EXPECT_EQ(layout.more_width, 0);
}

TEST(PropertyPageNavigation, SingleLongTabUsesTheRowWithoutAnEmptyMenu)
{
    int identity = 0;
    vgui2::Panel* page = reinterpret_cast<vgui2::Panel*>(&identity);
    PropertyPageNavigation navigation;
    navigation.AddPage(page);
    const PropertyPageTabLayout layout = navigation.CalculateLayout({2000}, page, 200, 40, 1);
    EXPECT_EQ(layout.tab_widths, (std::vector<int>{200}));
    EXPECT_EQ(layout.more_width, 0);
}

TEST(PropertyPageNavigation, ManyTabsKeepTheActivePageAndStayWithinTheAvailableWidth)
{
    int identities[30]{};
    PropertyPageNavigation navigation;
    for (int& identity : identities)
    {
        navigation.AddPage(reinterpret_cast<vgui2::Panel*>(&identity));
    }
    vgui2::Panel* active = reinterpret_cast<vgui2::Panel*>(&identities[25]);
    const PropertyPageTabLayout layout = navigation.CalculateLayout(std::vector<int>(30, 80), active, 580, 60, 1);
    EXPECT_EQ(layout.tab_widths[25], 80);
    int occupied = layout.more_width;
    int visible = 0;
    for (int index = 0; index < 30; ++index)
    {
        if (layout.tab_widths[index])
        {
            occupied += layout.tab_widths[index] + 1;
            ++visible;
        }
    }
    EXPECT_EQ(visible, 6);
    EXPECT_EQ(occupied, 580);
    EXPECT_EQ(layout.tab_widths[5], 0);
}
