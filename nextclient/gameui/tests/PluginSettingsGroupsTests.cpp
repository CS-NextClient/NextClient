#include <gtest/gtest.h>

#include "GameUi/PluginSettingsGroupsState.h"
#include "GameUi/PluginText.h"

namespace
{
    tao::json::value Groups()
    {
        return tao::json::from_string(R"([
            {"id":"second-name","name":"Zebra","tabs":[],"translations":{"ru":{"name":"Localized"}},
             "controls":[{"id":"option","tab":"plugins","kind":1,"value":0}]},
            {"id":"first-name","name":"Alpha","tabs":[],
             "controls":[{"id":"option","tab":"plugins","kind":1,"value":0}]}
        ])");
    }
} // namespace

TEST(PluginSettingsGroups, OnlySharedControlsContributeAndButtonsCount)
{
    const auto plugins = tao::json::from_string(R"([
        {"id":"empty","name":"Empty","tabs":[],"controls":[]},
        {"id":"other","name":"Other","tabs":[{"id":"tools"}],
         "controls":[{"id":"a","tab":"tools","value":0},{"id":"b","tab":"mouse","value":1}]},
        {"id":"legacy","name":"Legacy","tabs":[{"id":"plugins"}],
         "controls":[{"id":"a","tab":"plugins","value":0}]},
        {"id":"action","name":"Action","tabs":[],
         "controls":[{"id":"run","tab":"plugins","kind":4,"value":0}]}
    ])");
    PluginSettingsGroupsState state(plugins);
    ASSERT_EQ(state.groups().size(), 1u);
    EXPECT_EQ(state.groups()[0].owner, "action");
    EXPECT_EQ(state.groups()[0].controls.at(0).at("owner"), "action");
    EXPECT_TRUE(state.has_available_controls());
    EXPECT_TRUE(state.is_expanded("action"));
}

TEST(PluginSettingsGroups, NativeSettingsStaySharedWhenTheOwnerHasACustomPluginsTab)
{
    const auto plugins = tao::json::from_string(R"([
        {"id":"owner","name":"Owner","tabs":[{"id":"plugins"}],
         "controls":[{"id":"custom","tab":"plugins","value":0},
                     {"id":"shared","tab":"","value":1}]}
    ])");
    PluginSettingsGroupsState state(plugins);
    ASSERT_EQ(state.groups().size(), 1u);
    ASSERT_EQ(state.groups()[0].controls.get_array().size(), 1u);
    EXPECT_EQ(state.groups()[0].controls.at(0).at("id"), "shared");
    EXPECT_EQ(state.groups()[0].controls.at(0).at("owner"), "owner");
    EXPECT_TRUE(state.has_available_controls());
    EXPECT_TRUE(state.is_expanded("owner"));
}

TEST(PluginSettingsGroups, EmptyDestinationStaysUnavailable)
{
    PluginSettingsGroupsState state(tao::json::empty_array);
    EXPECT_TRUE(state.groups().empty());
    EXPECT_FALSE(state.has_available_controls());
    state.Toggle("missing");
    state.BeginSession(PluginSettingsSnapshot{});
    EXPECT_FALSE(state.is_expanded("missing"));
}

TEST(PluginSettingsGroups, LoadOrderLocalizationAndExclusiveExpansion)
{
    const auto plugins = Groups();
    PluginSettingsGroupsState state(plugins);
    ASSERT_EQ(state.groups().size(), 2u);
    EXPECT_EQ(state.groups()[0].owner, "second-name");
    EXPECT_EQ(PluginMetadataText(state.groups()[0].metadata, "name", "ru"), "Localized");
    EXPECT_EQ(PluginMetadataText(state.groups()[1].metadata, "name", "ru"), "Alpha");
    EXPECT_TRUE(state.is_expanded("second-name"));
    state.Toggle("first-name");
    EXPECT_FALSE(state.is_expanded("second-name"));
    EXPECT_TRUE(state.is_expanded("first-name"));
    state.RefreshAvailability(PluginSettingsSnapshot(plugins));
    EXPECT_TRUE(state.is_expanded("first-name"));
    state.Toggle("first-name");
    EXPECT_FALSE(state.is_expanded("first-name"));
    EXPECT_FALSE(state.is_expanded("second-name"));
    state.Toggle("missing");
    EXPECT_FALSE(state.is_expanded("missing"));
    state.BeginSession(PluginSettingsSnapshot(plugins));
    EXPECT_TRUE(state.is_expanded("second-name"));
}

TEST(PluginSettingsGroups, RetirementRemovesAvailabilityAndReopenSelectsFirstSurvivor)
{
    auto plugins = Groups();
    PluginSettingsGroupsState state(plugins);
    plugins.get_array().erase(plugins.get_array().begin());
    EXPECT_TRUE(state.RefreshAvailability(PluginSettingsSnapshot(plugins)));
    EXPECT_FALSE(state.groups()[0].available);
    EXPECT_TRUE(state.groups()[1].available);
    EXPECT_TRUE(state.has_available_controls());
    state.Toggle("second-name");
    EXPECT_FALSE(state.is_expanded("second-name"));
    state.BeginSession(PluginSettingsSnapshot(plugins));
    EXPECT_TRUE(state.is_expanded("first-name"));
    state.RefreshAvailability(PluginSettingsSnapshot{});
    EXPECT_FALSE(state.has_available_controls());
    EXPECT_FALSE(state.is_expanded("first-name"));
}

TEST(PluginSettingsGroups, CollapsedEditsRemainCollectableWithoutOverwritingLiveValues)
{
    auto plugins = Groups();
    PluginSettingsGroupsState groups(plugins);
    const auto& first = groups.groups()[0].controls.at(0);
    const auto& second = groups.groups()[1].controls.at(0);
    PluginSettingsState first_editor, second_editor;
    first_editor.ResetControl(first, PluginSettingsSnapshot(plugins));
    second_editor.ResetControl(second, PluginSettingsSnapshot(plugins));
    groups.Toggle("first-name");
    plugins.at(1).at("controls").at(0)["value"] = 1;
    const PluginSettingsSnapshot current(plugins);
    tao::json::value values = tao::json::empty_array;
    first_editor.Collect(values, first, 1, current);
    second_editor.Collect(values, second, 0, current);
    ASSERT_EQ(values.get_array().size(), 1u);
    EXPECT_EQ(values.at(0).at("owner"), "second-name");

    // A failed save leaves the baseline intact, allowing the same edit to retry.
    values = tao::json::empty_array;
    first_editor.Collect(values, first, 1, current);
    ASSERT_EQ(values.get_array().size(), 1u);
    first_editor.ResetControl(first, current);
    values = tao::json::empty_array;
    first_editor.Collect(values, first, 0, current);
    EXPECT_TRUE(values.get_array().empty());
    groups.Toggle("first-name");
    EXPECT_FALSE(groups.is_expanded("first-name"));
}

TEST(PluginSettingsGroups, OwnerIdentitySeparatesEqualNamesAndControlIds)
{
    auto plugins = Groups();
    plugins.at(1)["name"] = "Zebra";
    PluginSettingsGroupsState groups(plugins);
    groups.Toggle("first-name");
    EXPECT_TRUE(groups.is_expanded("first-name"));
    EXPECT_FALSE(groups.is_expanded("second-name"));
    const PluginSettingsSnapshot current(plugins);
    PluginSettingsState editor;
    const auto& control = groups.groups()[1].controls.at(0);
    editor.ResetControl(control, current);
    tao::json::value values = tao::json::empty_array;
    editor.Collect(values, control, 1, current);
    ASSERT_EQ(values.get_array().size(), 1u);
    EXPECT_EQ(values.at(0).at("owner"), "first-name");
}
