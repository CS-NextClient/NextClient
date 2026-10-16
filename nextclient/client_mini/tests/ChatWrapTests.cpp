#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "chat_wrap.h"

namespace
{
    // ^N stands for color code N
    std::wstring Coded(std::wstring_view text)
    {
        std::wstring result;
        for (size_t i = 0; i < text.size(); i++)
        {
            if (text[i] == L'^' && i + 1 < text.size())
            {
                result += static_cast<wchar_t>(text[++i] - L'0');
            }
            else
            {
                result += text[i];
            }
        }
        return result;
    }

    std::vector<std::wstring> Coded(std::initializer_list<std::wstring_view> pieces)
    {
        std::vector<std::wstring> result;
        for (std::wstring_view piece : pieces)
        {
            result.push_back(Coded(piece));
        }
        return result;
    }

    std::vector<std::wstring> Split(
        std::wstring_view line,
        int max_width,
        size_t max_pieces = 5,
        const std::function<int(wchar_t)>& char_width = [](wchar_t) { return 1; }
    )
    {
        return ChatWrap_Split(Coded(line), max_width, char_width, max_pieces);
    }
} // namespace

TEST(ChatWrap, LineThatFitsStaysWhole)
{
    EXPECT_EQ(Split(L"^1hello world", 11), Coded({L"^1hello world"}));
}

TEST(ChatWrap, ColorCodesTakeNoWidth)
{
    EXPECT_EQ(Split(L"^3ab^1^4cd", 4), Coded({L"^3ab^1^4cd"}));
}

TEST(ChatWrap, BreaksAfterTheLastSpaceThatFits)
{
    EXPECT_EQ(Split(L"^1aaa bbb ccc", 8), Coded({L"^1aaa bbb", L"^1ccc"}));
}

TEST(ChatWrap, NextPieceKeepsTheColorOfTheBreak)
{
    // The issue #104 message: the green part used to fall back to the default color.
    EXPECT_EQ(Split(L"^1ab ^4cd ef.", 6), Coded({L"^1ab ^4cd", L"^4ef."}));
}

TEST(ChatWrap, TeamColorCarriesOver)
{
    EXPECT_EQ(Split(L"^1(CT) ^3Long Name^1 : hi", 12), Coded({L"^1(CT) ^3Long", L"^3Name^1 : hi"}));
}

TEST(ChatWrap, CodeRightAfterTheBreakIsNotDoubled)
{
    EXPECT_EQ(Split(L"^1abc ^4de", 4), Coded({L"^1abc", L"^4de"}));
}

TEST(ChatWrap, WordLongerThanALineIsCut)
{
    EXPECT_EQ(Split(L"^1abcdefghij", 4), Coded({L"^1abcd", L"^1efgh", L"^1ij"}));
}

TEST(ChatWrap, CharacterWiderThanTheLineStillMovesOn)
{
    auto wide = [](wchar_t c) { return c == L'W' ? 10 : 1; };
    EXPECT_EQ(Split(L"^1WWa", 4, 5, wide), Coded({L"^1W", L"^1W", L"^1a"}));
}

TEST(ChatWrap, LeadingSpaceIsNotABreak)
{
    EXPECT_EQ(Split(L"^1 abcdef", 4), Coded({L"^1 abc", L"^1def"}));
}

TEST(ChatWrap, UncodedLineContinuesInTheDefaultColor)
{
    // The stock chat reads codes only in a line that starts with one; 2 colors the sender's name.
    EXPECT_EQ(Split(L"^2Bob : aaa bbb", 9), Coded({L"^2Bob : aaa", L"^1bbb"}));
    EXPECT_EQ(Split(L"aaa ^4bbb", 3), Coded({L"aaa", L"^1^4bbb"}));
}

TEST(ChatWrap, TailWithNothingToDrawIsDropped)
{
    EXPECT_EQ(Split(L"^1abcd ^4", 4), Coded({L"^1abcd"}));
}

TEST(ChatWrap, StopsAtMaxPieces)
{
    EXPECT_EQ(Split(L"^1aa bb cc dd", 2, 3), Coded({L"^1aa", L"^1bb", L"^1cc"}));
}

TEST(ChatWrap, VariableWidths)
{
    auto width = [](wchar_t c) { return c == L'm' ? 3 : 1; };
    EXPECT_EQ(Split(L"^1mm ii mm", 9, 5, width), Coded({L"^1mm ii", L"^1mm"}));
}
