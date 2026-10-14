#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "GameUi/Controls/PropertyPageOverflow.h"

namespace
{
    struct ParentProbe
    {
        bool visible = true;
        bool focus_requested{};
        ParentProbe* parent{};

        bool IsVisible() const
        {
            return visible;
        }
        ParentProbe* GetParent() const
        {
            return parent;
        }
        void RequestFocus()
        {
            focus_requested = true;
        }
    };

    struct MenuItemProbe
    {
        bool enabled = true;
        int page{};
        std::wstring caption;

        bool IsEnabled() const
        {
            return enabled;
        }
    };

    class MenuProbe
    {
    public:
        virtual ~MenuProbe() = default;
        std::vector<MenuItemProbe> items{{true, 1, L"Same title"}, {true, 2, L"Plugins"}, {true, 3, L"Same title"}};
        std::vector<int> picked;
        ParentProbe parent;
        int highlighted = -1;
        bool visible{}, focused{};

        int GetItemCount() const
        {
            return static_cast<int>(items.size());
        }
        int GetMenuID(int row) const
        {
            return row >= 0 && row < GetItemCount() ? 10 + row * 2 : -1;
        }
        int GetRowByItemId(int id) const
        {
            return id >= 10 && id % 2 == 0 && (id - 10) / 2 < GetItemCount() ? (id - 10) / 2 : -1;
        }
        int GetCurrentlyHighlightedItem() const
        {
            return highlighted;
        }
        void SetCurrentlyHighlightedItem(int id)
        {
            highlighted = id;
        }
        void ClearCurrentlyHighlightedItem()
        {
            highlighted = -1;
        }
        MenuItemProbe* GetMenuItem(int id)
        {
            return &items.at(GetRowByItemId(id));
        }
        void SetVisible(bool state)
        {
            visible = state;
        }
        bool IsVisible() const
        {
            return visible;
        }
        bool HasFocus() const
        {
            return focused;
        }
        ParentProbe* GetParent()
        {
            return &parent;
        }
        virtual void OnKeyCodeTyped(vgui2::KeyCode code)
        {
            int row = GetRowByItemId(highlighted);
            switch (code)
            {
                case vgui2::KEY_HOME:
                    row = 0;
                    break;
                case vgui2::KEY_END:
                    row = GetItemCount() - 1;
                    break;
                case vgui2::KEY_UP:
                case vgui2::KEY_XBUTTON_UP:
                case vgui2::KEY_XSTICK1_UP:
                case vgui2::KEY_PAGEUP:
                    --row;
                    break;
                case vgui2::KEY_DOWN:
                case vgui2::KEY_XBUTTON_DOWN:
                case vgui2::KEY_XSTICK1_DOWN:
                case vgui2::KEY_PAGEDOWN:
                    ++row;
                    break;
                case vgui2::KEY_ENTER:
                case vgui2::KEY_XBUTTON_A:
                    if (row >= 0)
                    {
                        picked.push_back(items[row].page);
                        visible = false;
                    }
                    return;
                default:
                    return;
            }
            highlighted = GetMenuID(row < 0 ? GetItemCount() - 1 : row >= GetItemCount() ? 0 : row);
        }
    };

    class ButtonProbe
    {
    public:
        virtual ~ButtonProbe() = default;
        MenuProbe menu;
        ParentProbe parent;
        int clicks{};
        int forwarded_keys{};
        bool enabled = true;
        bool visible = true;
        bool focus_requested{};

        bool IsEnabled() const
        {
            return enabled;
        }
        bool IsVisible() const
        {
            return visible;
        }
        ParentProbe* GetParent()
        {
            return &parent;
        }
        MenuProbe* GetMenu()
        {
            return &menu;
        }
        void RequestFocus()
        {
            focus_requested = true;
        }
        void DoClick()
        {
            ++clicks;
            menu.visible = !menu.visible;
        }
        virtual void HideMenu()
        {
            menu.visible = false;
        }
        virtual void OnKeyCodePressed(vgui2::KeyCode)
        {
            ++forwarded_keys;
        }
        virtual void OnKeyCodeTyped(vgui2::KeyCode)
        {
            ++forwarded_keys;
        }
        virtual void OnKeyCodeReleased(vgui2::KeyCode)
        {
            ++forwarded_keys;
        }
    };
} // namespace

TEST(PropertyPageOverflowMenu, OpeningHighlightsTheFirstEnabledItemWithoutSelectingAPage)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    menu.items[0].enabled = false;
    menu.HighlightFirstEnabled();
    EXPECT_EQ(menu.highlighted, menu.GetMenuID(1));
    EXPECT_TRUE(menu.picked.empty());
}

TEST(PropertyPageOverflowMenu, KeyboardNavigationSkipsDisabledItemsInBothDirectionsAndWraps)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    menu.items[1].enabled = false;
    menu.HighlightFirstEnabled();
    menu.OnKeyCodeTyped(vgui2::KEY_DOWN);
    EXPECT_EQ(menu.highlighted, menu.GetMenuID(2));
    menu.OnKeyCodeTyped(vgui2::KEY_DOWN);
    EXPECT_EQ(menu.highlighted, menu.GetMenuID(0));
    menu.OnKeyCodeTyped(vgui2::KEY_UP);
    EXPECT_EQ(menu.highlighted, menu.GetMenuID(2));
    menu.OnKeyCodeTyped(vgui2::KEY_XSTICK1_UP);
    EXPECT_EQ(menu.highlighted, menu.GetMenuID(0));
    EXPECT_TRUE(menu.picked.empty());
}

TEST(PropertyPageOverflowMenu, HomeEndAndPagingSkipDisabledItems)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    menu.items[0].enabled = false;
    menu.items[2].enabled = false;
    for (vgui2::KeyCode code : {vgui2::KEY_HOME, vgui2::KEY_END, vgui2::KEY_PAGEUP, vgui2::KEY_PAGEDOWN})
    {
        menu.OnKeyCodeTyped(code);
        EXPECT_EQ(menu.highlighted, menu.GetMenuID(1));
    }
}

TEST(PropertyPageOverflowMenu, EqualCaptionsSelectOnlyTheHighlightedPageIdentity)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    menu.OnKeyCodeTyped(vgui2::KEY_END);
    menu.OnKeyCodeTyped(vgui2::KEY_ENTER);
    EXPECT_EQ(menu.picked, (std::vector<int>{3}));
    EXPECT_EQ(menu.items[0].caption, menu.items[2].caption);
}

TEST(PropertyPageOverflowMenu, SpaceLeavesNativeMenuSelectionUnchanged)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    menu.HighlightFirstEnabled();
    menu.OnKeyCodeTyped(vgui2::KEY_SPACE);
    EXPECT_TRUE(menu.picked.empty());
}

TEST(PropertyPageOverflowMenu, EscapeDiscardsTheHighlightAndReturnsFocusToMore)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    menu.visible = true;
    menu.OnKeyCodeTyped(vgui2::KEY_END);
    menu.OnKeyCodeTyped(vgui2::KEY_ESCAPE);
    EXPECT_FALSE(menu.visible);
    EXPECT_TRUE(menu.parent.focus_requested);
    EXPECT_TRUE(menu.picked.empty());
}

TEST(PropertyPageOverflowMenu, DisabledAfterOpeningCannotBeCommittedByEnterOrSpace)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    menu.HighlightFirstEnabled();
    menu.items[0].enabled = false;
    menu.OnKeyCodeTyped(vgui2::KEY_ENTER);
    menu.OnKeyCodeTyped(vgui2::KEY_SPACE);
    EXPECT_TRUE(menu.picked.empty());
    menu.OnKeyCodeTyped(vgui2::KEY_DOWN);
    EXPECT_EQ(menu.highlighted, menu.GetMenuID(1));
}

TEST(PropertyPageOverflowMenu, EmptyAndFullyDisabledMenusHaveNoKeyboardSelection)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    for (MenuItemProbe& item : menu.items)
    {
        item.enabled = false;
    }
    menu.HighlightFirstEnabled();
    EXPECT_EQ(menu.highlighted, -1);
    menu.OnKeyCodeTyped(vgui2::KEY_HOME);
    EXPECT_EQ(menu.highlighted, -1);
    menu.OnKeyCodeTyped(vgui2::KEY_ENTER);
    EXPECT_TRUE(menu.picked.empty());
    menu.items.clear();
    menu.HighlightFirstEnabled();
    menu.OnKeyCodeTyped(vgui2::KEY_DOWN);
    EXPECT_EQ(menu.highlighted, -1);
}

TEST(PropertyPageOverflowMenu, ScrollingPastTwelveItemsPreservesIdentityAndDisabledState)
{
    PropertyPageOverflowMenu<MenuProbe> menu;
    menu.items.clear();
    for (int index = 0; index < 30; ++index)
    {
        menu.items.push_back({index != 29, index, std::wstring(500, L'x')});
    }
    menu.OnKeyCodeTyped(vgui2::KEY_END);
    EXPECT_EQ(menu.highlighted, menu.GetMenuID(28));
    menu.OnKeyCodeTyped(vgui2::KEY_ENTER);
    EXPECT_EQ(menu.picked, (std::vector<int>{28}));
}

TEST(PropertyPageOverflowButton, EnterAndSpaceToggleTheMenuOnlyOncePerKeySequence)
{
    PropertyPageOverflowButton<ButtonProbe> button;
    for (vgui2::KeyCode code : {vgui2::KEY_ENTER, vgui2::KEY_SPACE})
    {
        button.menu.visible = false;
        const int before = button.clicks;
        button.OnKeyCodePressed(code);
        button.OnKeyCodeTyped(code);
        button.OnKeyCodeReleased(code);
        EXPECT_EQ(button.clicks, before + 1);
        EXPECT_TRUE(button.menu.visible);
    }
    EXPECT_EQ(button.forwarded_keys, 0);
}

TEST(PropertyPageOverflowButton, DisabledButtonCannotOpenTheMenuAndOtherKeysAreForwarded)
{
    PropertyPageOverflowButton<ButtonProbe> button;
    button.enabled = false;
    button.OnKeyCodePressed(vgui2::KEY_ENTER);
    EXPECT_EQ(button.clicks, 0);
    button.OnKeyCodePressed(vgui2::KEY_TAB);
    button.OnKeyCodeTyped(vgui2::KEY_TAB);
    button.OnKeyCodeReleased(vgui2::KEY_TAB);
    EXPECT_EQ(button.forwarded_keys, 3);
}

TEST(PropertyPageOverflowButton, ClosingTheFocusedMenuReturnsFocusToMore)
{
    PropertyPageOverflowButton<ButtonProbe> button;
    button.menu.visible = true;
    button.menu.focused = true;
    button.HideMenu();
    EXPECT_FALSE(button.menu.visible);
    EXPECT_TRUE(button.focus_requested);
    EXPECT_TRUE(button.menu.focused);
}

TEST(PropertyPageOverflowButton, PostedMenuCloseCannotStealFocusAfterSelectionOrDialogClose)
{
    PropertyPageOverflowButton<ButtonProbe> selected;
    selected.menu.visible = false;
    selected.menu.focused = true;
    selected.HideMenu();
    EXPECT_FALSE(selected.focus_requested);
    EXPECT_TRUE(selected.menu.focused);
    PropertyPageOverflowButton<ButtonProbe> closed;
    ParentProbe frame;
    frame.visible = false;
    closed.parent.parent = &frame;
    closed.menu.focused = true;
    closed.HideMenu();
    EXPECT_FALSE(closed.focus_requested);
}
