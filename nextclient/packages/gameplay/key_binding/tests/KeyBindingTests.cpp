#include <gtest/gtest.h>

#include <key_binding/key_binding.h>

using key_binding::IsMultiButton;
using key_binding::PressCommands;
using key_binding::ReleaseCommands;
using key_binding::SplitCommands;

TEST(KeyBindingSplit, SplitsAtSemicolonsAndNewlines)
{
    std::vector<std::string_view> expected = {"+a", "+b", "say hi"};
    EXPECT_EQ(SplitCommands("+a;+b\nsay hi"), expected);
}

TEST(KeyBindingSplit, TrimsBlanksAndDropsEmptyCommands)
{
    std::vector<std::string_view> expected = {"+a", "+b"};
    EXPECT_EQ(SplitCommands("  +a ; ;\t+b ;"), expected);
    EXPECT_TRUE(SplitCommands("").empty());
    EXPECT_TRUE(SplitCommands(" ; ").empty());
}

TEST(KeyBindingSplit, KeepsSemicolonsInsideQuotes)
{
    std::vector<std::string_view> expected = {"+a", "say \"x;y\""};
    EXPECT_EQ(SplitCommands("+a;say \"x;y\""), expected);
}

TEST(KeyBindingSplit, NewlineEndsAnUnclosedQuote)
{
    std::vector<std::string_view> expected = {"say \"x", "+b"};
    EXPECT_EQ(SplitCommands("say \"x\n+b"), expected);
}

TEST(KeyBindingMultiButton, NeedsALeadingPlusAndAnotherCommand)
{
    EXPECT_TRUE(IsMultiButton("+a;+b"));
    EXPECT_TRUE(IsMultiButton("+a;say hi"));
    EXPECT_FALSE(IsMultiButton("+attack"));
    EXPECT_FALSE(IsMultiButton("+attack;"));
    EXPECT_FALSE(IsMultiButton(""));
}

TEST(KeyBindingMultiButton, LeavesBindingsTheEngineNeverReleases)
{
    EXPECT_FALSE(IsMultiButton("wait;+attack"));
    EXPECT_FALSE(IsMultiButton(" +a;+b"));
}

TEST(KeyBindingPress, GivesEveryButtonTheKey)
{
    EXPECT_EQ(PressCommands("+a;+b", 102), "+a 102;+b 102\n");
    EXPECT_EQ(PressCommands("+a; say hi", 102), "+a 102;say hi\n");
    EXPECT_EQ(PressCommands("+a 5;+b", 102), "+a 5 102;+b 102\n");
}

TEST(KeyBindingRelease, ReleasesEveryButtonAndNothingElse)
{
    EXPECT_EQ(ReleaseCommands("+a;+b", 102), "-a 102;-b 102\n");
    EXPECT_EQ(ReleaseCommands("+a;say hi;+b", 102), "-a 102;-b 102\n");
    EXPECT_EQ(ReleaseCommands("+a 5; +b ", 102), "-a 5 102;-b 102\n");
}

TEST(KeyBindingRelease, EmptyWithoutButtons)
{
    EXPECT_EQ(ReleaseCommands("say hi;slot1", 102), "");
    EXPECT_EQ(PressCommands("", 102), "");
}
