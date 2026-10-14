#include <gtest/gtest.h>
#include "GameUi/PluginsDialogState.h"

TEST(PluginsDialogState, UnchangedPluginsReportActualState)
{
    std::vector<PluginSelection> selection{{"active.dll", true}, {"disabled.dll", false}};
    EXPECT_STREQ(PluginStatusToken(selection, selection, 0, true, false, false), "#NextPlugins_Active");
    EXPECT_STREQ(PluginStatusToken(selection, selection, 1, false, false, false), "#NextPlugins_Disabled");
}

TEST(PluginsDialogState, ToggleAndUndoRestoreStatusAndClearPendingChanges)
{
    std::vector<PluginSelection> initial{{"active.dll", true}, {"disabled.dll", false}};
    auto current = initial;
    current[0].enabled = false;
    current[1].enabled = true;
    EXPECT_NE(initial, current);
    EXPECT_STREQ(PluginStatusToken(initial, current, 0, true, false, false), "#NextPlugins_PendingRestart");
    EXPECT_STREQ(PluginStatusToken(initial, current, 1, false, false, false), "#NextPlugins_PendingRestart");
    current[0].enabled = true;
    current[1].enabled = false;
    EXPECT_EQ(initial, current);
    EXPECT_STREQ(PluginStatusToken(initial, current, 0, true, false, false), "#NextPlugins_Active");
    EXPECT_STREQ(PluginStatusToken(initial, current, 1, false, false, false), "#NextPlugins_Disabled");
}

TEST(PluginsDialogState, ReorderingAndRestoringOrderUpdatesPendingState)
{
    std::vector<PluginSelection> initial{{"first.dll", true}, {"second.dll", true}};
    auto current = initial;
    std::swap(current[0], current[1]);
    EXPECT_NE(initial, current);
    EXPECT_STREQ(PluginStatusToken(initial, current, 0, true, false, false), "#NextPlugins_PendingRestart");
    EXPECT_STREQ(PluginStatusToken(initial, current, 1, true, false, false), "#NextPlugins_PendingRestart");
    std::swap(current[0], current[1]);
    EXPECT_EQ(initial, current);
    EXPECT_STREQ(PluginStatusToken(initial, current, 0, true, false, false), "#NextPlugins_Active");
}

TEST(PluginsDialogState, LoadFailureIsBlockedRatherThanPendingRestart)
{
    std::vector<PluginSelection> selection{{"plugin.dll", true}};
    EXPECT_STREQ(PluginStatusToken(selection, selection, 0, false, false, false), "#NextPlugins_Blocked");
    EXPECT_STREQ(PluginStatusToken(selection, selection, 0, false, false, true), "#NextPlugins_Disabled");
    EXPECT_STREQ(PluginStatusToken(selection, selection, 0, false, true, false), "#NextPlugins_Blocked");
}

TEST(PluginsDialogState, ReorderedDisabledPluginRemainsDisabled)
{
    std::vector<PluginSelection> initial{{"first.dll", false}, {"second.dll", false}};
    auto current = initial;
    std::swap(current[0], current[1]);
    EXPECT_NE(initial, current);
    EXPECT_STREQ(PluginStatusToken(initial, current, 0, false, false, false), "#NextPlugins_Disabled");
}

TEST(PluginsDialogState, RunningPluginWithDiscoveryErrorRetainsActiveAndPendingStatus)
{
    std::vector<PluginSelection> initial{{"active.dll", true}};
    EXPECT_STREQ(PluginStatusToken(initial, initial, 0, true, true, false), "#NextPlugins_Active");
    auto disabled = initial;
    disabled[0].enabled = false;
    EXPECT_STREQ(PluginStatusToken(initial, disabled, 0, true, true, false), "#NextPlugins_PendingRestart");
}

TEST(PluginsDialogState, RecoveryCanBeAcknowledgedWithoutChangingEmptyOrUnchangedSelection)
{
    std::vector<PluginSelection> empty;
    EXPECT_TRUE(PluginCanConfirmSelection(empty, empty, true));
    EXPECT_FALSE(PluginCanConfirmSelection(empty, empty, false));
    std::vector<PluginSelection> initial{{"active.dll", true}};
    EXPECT_TRUE(PluginCanConfirmSelection(initial, initial, true));
    EXPECT_FALSE(PluginCanConfirmSelection(initial, initial, false));
    auto changed = initial;
    changed[0].enabled = false;
    EXPECT_TRUE(PluginCanConfirmSelection(initial, changed, false));
    EXPECT_FALSE(PluginCanConfirmSelection(initial, initial, false));
}
