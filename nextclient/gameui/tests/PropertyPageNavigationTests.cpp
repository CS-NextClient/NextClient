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

TEST_F(PropertyPageNavigationTest, RecreatedNavigationPreservesDisabledPageIdentity)
{
    navigation_.SetPageEnabled(plugins_, false);
    PropertyPageNavigation picker;
    for (vgui2::Panel* page : {first_, plugins_, last_})
    {
        picker.AddPage(page);
        picker.SetPageEnabled(page, navigation_.is_page_enabled(page));
    }
    EXPECT_TRUE(picker.is_page_enabled(first_));
    EXPECT_FALSE(picker.is_page_enabled(plugins_));
    EXPECT_TRUE(picker.is_page_enabled(last_));
    EXPECT_EQ(picker.FindEnabled(1), 2);
}
