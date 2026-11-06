#include <gtest/gtest.h>

#include <vgui/IInputInternal.h>
#include <vgui/ISurfaceNext.h>

#include "GameUi/Controls/DragValueLabel.h"

namespace
{
    class Input : public vgui2::IInputInternal
    {
    public:
        vgui2::VPANEL capture{};
        int cursor_x = 100;
        int cursor_y = 100;

        void SetMouseCapture(vgui2::VPANEL panel) override
        {
            capture = panel;
        }

        vgui2::VPANEL GetMouseCapture() override
        {
            return capture;
        }

        void GetCursorPos(int& x, int& y) override
        {
            x = cursor_x;
            y = cursor_y;
        }

        void SetCursorPos(int x, int y) override
        {
            cursor_x = x;
            cursor_y = y;
        }

        bool IsKeyDown(vgui2::KeyCode) override
        {
            return false;
        }
    };

    class Surface : public vgui2::ISurfaceNext
    {
    public:
        void GetScreenSize(int& width, int& height) override
        {
            width = 1920;
            height = 1080;
        }
    };

    class Listener : public CDragValueLabel::IListener
    {
    public:
        int begin_count{};
        int end_count{};
        int pixels{};

        void OnDragValueBegin(int index) override
        {
            EXPECT_EQ(index, 7);
            ++begin_count;
        }

        void OnDragValueEnd(int index) override
        {
            EXPECT_EQ(index, 7);
            ++end_count;
        }

        void OnDragValue(int index, int delta, bool fine, bool coarse) override
        {
            EXPECT_EQ(index, 7);
            EXPECT_FALSE(fine);
            EXPECT_FALSE(coarse);
            pixels += delta;
        }
    };

    Input g_Input;
    Surface g_Surface;

    class DragValueLabelTest : public testing::Test
    {
    protected:
        Listener listener;
        CDragValueLabel label{nullptr, "value", "Value", &listener, 7};

        void SetUp() override
        {
            g_Input.capture = 0;
            g_Input.cursor_x = 100;
            g_Input.cursor_y = 100;
        }

        void Move(int x)
        {
            g_Input.cursor_x = x;
            label.OnCursorMoved(x, 100);
        }
    };
}

vgui2::IInputInternal* vgui2::input()
{
    return &g_Input;
}

vgui2::ISurfaceNext* vgui2::surface()
{
    return &g_Surface;
}

TEST_F(DragValueLabelTest, CaptureLossEndsDragBeforeTheCursorReturns)
{
    label.OnMousePressed(vgui2::MOUSE_LEFT);
    Move(110);
    ASSERT_EQ(listener.pixels, 10);

    label.OnMousePressed(vgui2::MOUSE_RIGHT);
    g_Input.capture = 0;
    label.OnMouseCaptureLost();
    Move(150);
    EXPECT_EQ(listener.pixels, 10);
    EXPECT_EQ(listener.begin_count, 1);
    EXPECT_EQ(listener.end_count, 1);

    label.OnMouseReleased(vgui2::MOUSE_LEFT);
    label.OnMouseCaptureLost();
    EXPECT_EQ(listener.end_count, 1);
}

TEST_F(DragValueLabelTest, CaptureLossKeepsTheOtherOwnersCapture)
{
    vgui2::Panel other;
    label.OnMousePressed(vgui2::MOUSE_LEFT);
    g_Input.capture = other.GetVPanel();
    label.OnMouseCaptureLost();
    Move(130);
    label.OnMouseReleased(vgui2::MOUSE_LEFT);

    EXPECT_EQ(listener.pixels, 0);
    EXPECT_EQ(listener.end_count, 1);
    EXPECT_EQ(g_Input.capture, other.GetVPanel());
}

TEST_F(DragValueLabelTest, QueuedCaptureLossAfterReleaseEndsOnlyOnceAndKeepsTheNewOwner)
{
    label.OnMousePressed(vgui2::MOUSE_LEFT);
    label.OnMouseReleased(vgui2::MOUSE_LEFT);
    ASSERT_EQ(g_Input.capture, 0u);
    ASSERT_EQ(listener.end_count, 1);

    vgui2::Panel other;
    g_Input.capture = other.GetVPanel();
    label.OnMouseCaptureLost();
    Move(130);
    EXPECT_EQ(listener.pixels, 0);
    EXPECT_EQ(listener.end_count, 1);
    EXPECT_EQ(g_Input.capture, other.GetVPanel());
}

TEST_F(DragValueLabelTest, ReleaseBeforeQueuedCaptureLossKeepsTheOtherOwnersCapture)
{
    label.OnMousePressed(vgui2::MOUSE_LEFT);
    vgui2::Panel other;
    g_Input.capture = other.GetVPanel();
    label.OnMouseReleased(vgui2::MOUSE_LEFT);
    label.OnMouseCaptureLost();
    Move(130);

    EXPECT_EQ(listener.pixels, 0);
    EXPECT_EQ(listener.end_count, 1);
    EXPECT_EQ(g_Input.capture, other.GetVPanel());
}
