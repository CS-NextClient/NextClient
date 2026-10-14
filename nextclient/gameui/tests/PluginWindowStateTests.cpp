#include <gtest/gtest.h>
#include "GameUi/PluginWindowState.h"
#include "GameUi/GameMenuOrder.h"
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <type_traits>

TEST(PluginWindowInput, ListUsesIntegerSelectionEvenThoughComboBoxIsATextEntry)
{
    static_assert(std::is_base_of_v<vgui2::TextEntry, vgui2::ComboBox>);
    struct Reader
    {
        int textReads{};
        bool Checked()
        {
            return true;
        }
        int SliderValue()
        {
            return 10;
        }
        int Selection()
        {
            return 2;
        }
        std::string Text()
        {
            ++textReads;
            return "caption";
        }
    } reader;
    const auto selected = ReadPluginInput("list", reader);
    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->as<int>(), 2);
    EXPECT_EQ(reader.textReads, 0);
    EXPECT_EQ(*ReadPluginInput("checkbox", reader), true);
    EXPECT_EQ(*ReadPluginInput("slider", reader), 10);
    EXPECT_EQ(*ReadPluginInput("text", reader), "caption");
    EXPECT_FALSE(ReadPluginInput("label", reader));
}

TEST(PluginWindowInput, Utf8ByteLimitPreservesWholeCharacters)
{
    std::string cyrillic;
    for (int i = 0; i < 513; ++i)
        cyrillic += "\xd0\xb0";
    EXPECT_EQ(PluginInputText(cyrillic), cyrillic.substr(0, 1024));
    const auto partial = std::string(1023, 'a') + "\xf0\x9f\x98\x80";
    EXPECT_EQ(PluginInputText(partial), std::string(1023, 'a'));
    EXPECT_EQ(PluginInputText(std::string(1025, 'b')).size(), 1024u);
}

TEST(PluginWindowLayout, FontAliasesShareResourcesAndLargeContentFits)
{
    auto font = tao::json::from_string(R"({"id":"one","name":"Arial","height":72,"weight":400})");
    const auto key = PluginFontKey(font);
    font["id"] = "another-window-local-id";
    EXPECT_EQ(PluginFontKey(font), key);
    font["height"] = 24;
    EXPECT_NE(PluginFontKey(font), key);
    EXPECT_GE(PluginRowHeight(72) - 4, 72);
    EXPECT_GE(PluginRowHeight(64) - 4, 64);
    EXPECT_EQ(PluginRowHeight(24), 32);
}

TEST(GameMenuOrder, DeletionAndReusedIdsPreserveEveryRemainingItem)
{
    GameMenuOrder order;
    std::vector<int> displayed{0, 1, 2, 3, 4};
    for (auto id : displayed)
        order.Add(id);
    order.Remove(3);
    std::erase(displayed, 3);
    auto move = [&](int id, int before) {
        ASSERT_NE(std::find(displayed.begin(), displayed.end(), before), displayed.end());
        std::erase(displayed, id);
        displayed.insert(std::find(displayed.begin(), displayed.end(), before), id);
    };
    auto priority = [](int id) { return -id; };
    order.Apply(false, priority, move);
    EXPECT_EQ(displayed, (std::vector<int>{0, 1, 2, 4}));
    order.Apply(true, priority, move);
    EXPECT_EQ(displayed, (std::vector<int>{4, 2, 1, 0}));
    order.Add(3);
    displayed.push_back(3);
    order.Apply(false, priority, move);
    EXPECT_EQ(displayed, (std::vector<int>{0, 1, 2, 4, 3}));
}
