#include <array>

#include <gtest/gtest.h>

#include "graphics/text/text_surface.h"

namespace
{
    unsigned long g_NextFont = 1000;
    unsigned long g_SelectedFont;
    unsigned long g_LastFont;
    int g_Flags, g_Blur, g_Scanlines, g_Low, g_High;
    int g_X, g_Y, g_PrintCalls;
    bool g_AddResult;

    unsigned long __fastcall CreateFont(void*, void*)
    {
        return ++g_NextFont;
    }

    bool __fastcall AddGlyphSet(
        void*,
        void*,
        unsigned long font,
        const char*,
        int,
        int,
        int blur,
        int scanlines,
        int flags,
        int low,
        int high
    )
    {
        g_LastFont = font;
        g_Flags = flags;
        g_Blur = blur;
        g_Scanlines = scanlines;
        g_Low = low;
        g_High = high;
        return g_AddResult;
    }

    int __fastcall GetFontTall(void*, void*, unsigned long font)
    {
        g_LastFont = font;
        return 97;
    }

    void __fastcall SetFont(void*, void*, unsigned long font)
    {
        g_SelectedFont = font;
    }

    void __fastcall SetPos(void*, void*, int x, int y)
    {
        g_X = x;
        g_Y = y;
    }

    void __fastcall GetPos(void*, void*, int* x, int* y)
    {
        *x = g_X;
        *y = g_Y;
    }

    void __fastcall Print(void*, void*, const wchar_t*, int length)
    {
        g_PrintCalls++;
        g_X += length * 11;
    }
} // namespace

class TextSurfaceTest : public testing::Test
{
protected:
    std::array<void*, 90> vtable_{};
    void** surface_{};

    template <class Result, class... Args>
    Result Invoke(int slot, Args... args)
    {
        auto function = reinterpret_cast<Result(__fastcall*)(void*, void*, Args...)>(vtable_[slot]);
        return function(&surface_, nullptr, args...);
    }

    void SetUp() override
    {
        vtable_[13] = reinterpret_cast<void*>(&SetFont);
        vtable_[16] = reinterpret_cast<void*>(&SetPos);
        vtable_[17] = reinterpret_cast<void*>(&GetPos);
        vtable_[18] = reinterpret_cast<void*>(&Print);
        vtable_[59] = reinterpret_cast<void*>(&CreateFont);
        vtable_[60] = reinterpret_cast<void*>(&AddGlyphSet);
        vtable_[62] = reinterpret_cast<void*>(&GetFontTall);
        surface_ = vtable_.data();
        g_PrintCalls = 0;
        g_AddResult = true;
        TextSurface_Install(&surface_);
    }

    void TearDown() override
    {
        TextSurface_Uninstall();
        EXPECT_EQ(vtable_[59], reinterpret_cast<void*>(&CreateFont));
    }

    bool Add(unsigned long font, int flags, int blur = 0, int scanlines = 0, int low = 0, int high = 0xFFFF)
    {
        return Invoke<bool>(60, font, "Arial", 16, 400, blur, scanlines, flags, low, high);
    }
};

TEST_F(TextSurfaceTest, UnsupportedStylesRetainNativeMetricsAndDrawing)
{
    for (int flags : {0x01, 0x02, 0x04, 0x08, 0x200})
    {
        unsigned long font = Invoke<unsigned long>(59);
        unsigned long native_font = g_NextFont;
        ASSERT_TRUE(Add(font, flags));
        EXPECT_EQ(g_Flags, flags);
        EXPECT_EQ(g_LastFont, native_font);
        EXPECT_EQ(Invoke<int>(62, font), 97);
        Invoke<void>(13, font);
        EXPECT_EQ(g_SelectedFont, native_font);
        Invoke<void>(16, 10, 20);
        Invoke<void>(18, L"abc", 3);
        int x = 0, y = 0;
        Invoke<void>(17, &x, &y);
        EXPECT_EQ(x, 43);
        EXPECT_EQ(y, 20);
    }

    EXPECT_EQ(g_PrintCalls, 5);
}

TEST_F(TextSurfaceTest, BlurScanlinesAndRangesReachTheNativeRenderer)
{
    for (const std::array<int, 4>& params : {std::array{2, 0, 0, 65535}, std::array{0, 3, 0, 65535}, std::array{0, 0, 32, 127}})
    {
        unsigned long font = Invoke<unsigned long>(59);
        ASSERT_TRUE(Add(font, 0x10, params[0], params[1], params[2], params[3]));
        EXPECT_EQ(g_Blur, params[0]);
        EXPECT_EQ(g_Scanlines, params[1]);
        EXPECT_EQ(g_Low, params[2]);
        EXPECT_EQ(g_High, params[3]);
        EXPECT_EQ(Invoke<int>(62, font), 97);
        ASSERT_TRUE(Add(font, 0x10));
        EXPECT_EQ(Invoke<int>(62, font), 97);
    }
}

TEST_F(TextSurfaceTest, SupportedFontUsesAtlasMetricsAndPreservesNativeFailure)
{
    unsigned long font = Invoke<unsigned long>(59);
    ASSERT_TRUE(Add(font, 0x90));
    int tall = Invoke<int>(62, font);
    EXPECT_GT(tall, 0);
    EXPECT_NE(tall, 97);
    g_AddResult = false;
    EXPECT_FALSE(Add(font, 0x90));
}
