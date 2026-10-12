#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "hud/radar/render/text/HudTextBuffer.h"

namespace
{
    struct Draw
    {
        int x, y, width;
        int r, g, b;
    };

    HudTextBuffer g_Buffer;
    std::vector<Draw> g_Draws;
    int g_R, g_G, g_B;

    void SetColor(int r, int g, int b)
    {
        g_R = r;
        g_G = g;
        g_B = b;
        g_Buffer.SetColor(r, g, b);
    }

    void Record(int x, int y, int width, int r, int g, int b)
    {
        if (y >= 0)
        {
            g_Draws.push_back({x, y, width, r, g, b});
        }
    }

    int ConsoleString(int x, int y, char* text)
    {
        int width = (int)std::strlen(text) * 8;
        Record(x, y, width, g_R, g_G, g_B);
        SetColor(255, 180, 30);
        return x + width;
    }

    int String(int x, int y, const char* text, int r, int g, int b)
    {
        int width = (int)std::strlen(text) * 12;
        Record(x, y, width, r, g, b);
        return width;
    }

    int StringReverse(int x, int y, const char* text, int r, int g, int b)
    {
        int width = (int)std::strlen(text) * 12;
        Record(x - width, y, width, r, g, b);
        return width + 24;
    }

    int Character(int x, int y, int ch, int r, int g, int b)
    {
        Record(x, y, 8, r, g, b);
        return 8;
    }

    int VguiCharacter(int x, int y, int ch, unsigned int font)
    {
        return Character(x, y, ch, g_R, g_G, g_B);
    }

    int VguiCharacterAdd(int x, int y, int ch, int r, int g, int b, unsigned int font)
    {
        return Character(x, y, ch, r, g, b);
    }
} // namespace

class HudTextBufferTest : public testing::Test
{
protected:
    void SetUp() override
    {
        goldsrc::cl_enginefunc_t engine{};
        engine.pfnDrawConsoleString = ConsoleString;
        engine.pfnDrawString = String;
        engine.pfnDrawStringReverse = StringReverse;
        engine.pfnDrawCharacter = Character;
        engine.pfnVGUI2DrawCharacter = VguiCharacter;
        engine.pfnVGUI2DrawCharacterAdd = VguiCharacterAdd;
        g_Buffer.Initialize(engine, SetColor);
        g_Draws.clear();
        SetColor(255, 180, 30);
        HudTextZone_BeginFrame(1024, 768);
        HudTextZone_SetPassthrough(false);
        HudTextZone_SetZone(20, 20, 180);
    }

    void TearDown() override
    {
        g_Buffer.Reset();
        HudTextZone_ClearZone();
    }
};

TEST_F(HudTextBufferTest, ConsoleFragmentsKeepSpacingAndTheirOwnColors)
{
    char name[] = "NAME ", body[] = "message";
    SetColor(255, 0, 0);
    int next_x = g_Buffer.DrawConsoleString(14, 100, name);
    EXPECT_EQ(next_x, 54);
    EXPECT_EQ(g_Buffer.DrawConsoleString(next_x, 100, body), 110);
    EXPECT_TRUE(g_Draws.empty());
    g_Buffer.Flush();

    ASSERT_EQ(g_Draws.size(), 2);
    EXPECT_EQ(g_Draws[0].x, 212);
    EXPECT_EQ(g_Draws[1].x, 252);
    EXPECT_EQ(g_Draws[0].g, 0);
    EXPECT_EQ(g_Draws[1].g, 180);
    EXPECT_EQ(g_Draws[1].b, 30);
}

TEST_F(HudTextBufferTest, UnbufferedConsoleResetAndReplayPreserveAmbientColor)
{
    char text[] = "hello";
    SetColor(255, 0, 0);
    HudTextZone_SetPassthrough(true);
    g_Buffer.DrawConsoleString(400, 400, text);
    HudTextZone_SetPassthrough(false);
    g_Buffer.VGUI2DrawCharacter(14, 100, 'A', 1);
    SetColor(10, 20, 30);
    g_Buffer.Flush();

    ASSERT_EQ(g_Draws.size(), 2);
    EXPECT_EQ(g_Draws[1].g, 180);
    EXPECT_EQ(g_Draws[1].b, 30);
    EXPECT_EQ(g_R, 10);
    EXPECT_EQ(g_G, 20);
    EXPECT_EQ(g_B, 30);
}

TEST_F(HudTextBufferTest, CreditsFontWidthDeterminesWhetherTheLineFits)
{
    std::string text(70, 'A');
    EXPECT_EQ(g_Buffer.DrawString(14, 100, text.c_str(), 1, 2, 3), 840);
    g_Buffer.Flush();

    ASSERT_EQ(g_Draws.size(), 1);
    EXPECT_EQ(g_Draws[0].x, 14);
    EXPECT_EQ(g_Draws[0].y, 212);
}

TEST_F(HudTextBufferTest, ReverseFragmentsUseCreditsMetricsAndKeepReturnValues)
{
    EXPECT_EQ(g_Buffer.DrawStringReverse(86, 100, "def", 1, 2, 3), 60);
    EXPECT_EQ(g_Buffer.DrawStringReverse(50, 100, "abc", 4, 5, 6), 60);
    g_Buffer.Flush();

    ASSERT_EQ(g_Draws.size(), 2);
    EXPECT_EQ(g_Draws[0].x, 248);
    EXPECT_EQ(g_Draws[1].x, 212);
}

TEST_F(HudTextBufferTest, GlyphAndStringFragmentsMoveTogetherWithAdjacentRows)
{
    char text[] = "abc";
    g_Buffer.DrawCharacter(14, 100, 'A', 255, 0, 0);
    g_Buffer.DrawConsoleString(22, 100, text);
    g_Buffer.DrawConsoleString(14, 124, text);
    g_Buffer.Flush();

    ASSERT_EQ(g_Draws.size(), 3);
    EXPECT_EQ(g_Draws[0].x, 212);
    EXPECT_EQ(g_Draws[1].x, 220);
    EXPECT_EQ(g_Draws[2].x, 212);
    EXPECT_EQ(g_Draws[2].y - g_Draws[0].y, 24);
}
