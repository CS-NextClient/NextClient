#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "GameUi/PluginControlAdapters.h"

namespace
{
    class SliderProbe
    {
    public:
        SliderProbe(vgui2::Panel*, const char*) {}

        void SetRange(int minimum, int maximum)
        {
            minimum_ = minimum;
            maximum_ = maximum;
        }

        void SetValue(int value, bool notify = true)
        {
            const int previous = value_;
            value_ = std::clamp(value, minimum_, maximum_);
            if (notify && value_ != previous)
            {
                ++notifications;
            }
        }

        int GetValue()
        {
            return value_;
        }

        void Click(float fraction)
        {
            // Slider::OnMousePressed rounds the value in the widget's range.
            const float value = static_cast<float>(minimum_) + fraction * static_cast<float>(maximum_ - minimum_);
            SetValue(static_cast<int>(value + 0.5f));
        }

        void Step(int direction)
        {
            SetValue(GetValue() + direction);
        }

        int notifications{};

    private:
        int minimum_{};
        int maximum_{};
        int value_{};
    };

    class ComboProbe
    {
    public:
        ComboProbe(vgui2::Panel*, const char*, int, bool) {}
        virtual ~ComboProbe() = default;

        void AddCaption(const wchar_t* caption)
        {
            captions_.emplace_back(caption);
        }

        void SilentActivateItemByRow(int row)
        {
            if (GetRowByItemId(row) >= 0)
            {
                active_ = row;
                SetText(captions_[row].c_str());
            }
        }

        void SetText(const wchar_t* text)
        {
            text_ = text;
        }

        int GetActiveItem()
        {
            return active_;
        }

        int GetItemIDFromRow(int row)
        {
            return GetRowByItemId(row);
        }

        int GetRowByItemId(int id)
        {
            return id >= 0 && id < static_cast<int>(captions_.size()) ? id : -1;
        }

        ComboProbe* GetMenu()
        {
            return this;
        }

        int GetCurrentlyHighlightedItem()
        {
            return highlighted_;
        }

        void SetCurrentlyHighlightedItem(int item)
        {
            highlighted_ = item;
            active_ = item;
        }

        void ClearCurrentlyHighlightedItem()
        {
            highlighted_ = -1;
        }

        bool IsDropdownVisible()
        {
            return false;
        }

        void PostActionSignal(KeyValues* message)
        {
            ++notifications;
            message->deleteThis();
        }

        void Hover(int row)
        {
            highlighted_ = row;
        }

        void Pick(int row)
        {
            active_ = row;
            highlighted_ = row;
            OnMenuItemSelected();
        }

        void Key(vgui2::KeyCode code)
        {
            OnKeyCodeTyped(code);
        }

        void Character(wchar_t character)
        {
            OnKeyTyped(character);
        }

        const std::wstring& text() const
        {
            return text_;
        }

        int notifications{};

    protected:
        virtual void OnMenuItemSelected()
        {
            ShowCaption(active_);
        }

        virtual void OnKeyCodeTyped(vgui2::KeyCode code)
        {
            if (captions_.empty())
            {
                return;
            }
            switch (code)
            {
                case vgui2::KEY_HOME:
                    highlighted_ = 0;
                    break;
                case vgui2::KEY_END:
                    highlighted_ = static_cast<int>(captions_.size()) - 1;
                    break;
                case vgui2::KEY_DOWN:
                    highlighted_ = (highlighted_ + 1) % static_cast<int>(captions_.size());
                    break;
                default:
                    return;
            }
            ShowCaption(highlighted_);
        }

        virtual void OnKeyTyped(wchar_t character)
        {
            for (int row = 0; row < static_cast<int>(captions_.size()); ++row)
            {
                if (!captions_[row].empty() && captions_[row][0] == character)
                {
                    highlighted_ = row;
                    ShowCaption(row);
                    return;
                }
            }
        }

    private:
        void ShowCaption(int row)
        {
            if (GetRowByItemId(row) >= 0 && text_ != captions_[row])
            {
                text_ = captions_[row];
                ++notifications;
            }
        }

        // Match VGUI: invalid activation retains zero (or the previous item),
        // while keyboard navigation can update the caption before activation.
        int active_{};
        int highlighted_ = -1;
        std::vector<std::wstring> captions_;
        std::wstring text_;
    };

    class ListProbe : public PluginListControl<ComboProbe>
    {
    public:
        using PluginListControl<ComboProbe>::PluginListControl;
        int selection_notifications{};

    protected:
        void SendSelectionChanged() override
        {
            // KeyValues allocation requires the engine's shared VGUI allocator.
            ++selection_notifications;
            ++notifications;
        }
    };
} // namespace

TEST(PluginSliderControl, MouseClicksPreserveNegativeValuesAndSignedEndpoints)
{
    PluginSliderControl<SliderProbe> slider(nullptr, "slider", -100, -1);
    slider.SetControlValue(-50);
    EXPECT_EQ(slider.ReadControlValue(), -50);
    EXPECT_EQ(slider.notifications, 0);
    slider.Click(0.0f);
    EXPECT_EQ(slider.ReadControlValue(), -100);
    slider.Click(1.0f);
    EXPECT_EQ(slider.ReadControlValue(), -1);
    slider.Step(1);
    EXPECT_EQ(slider.ReadControlValue(), -1);
    slider.Step(-1);
    EXPECT_EQ(slider.ReadControlValue(), -2);
}

TEST(PluginSliderControl, SmallRangesRetainPrecisionNearBothIntegerLimits)
{
    for (const int minimum : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max() - 100})
    {
        PluginSliderControl<SliderProbe> slider(nullptr, "slider", minimum, minimum + 100);
        slider.SetControlValue(minimum + 37);
        EXPECT_EQ(slider.ReadControlValue(), minimum + 37);
        EXPECT_EQ(slider.notifications, 0);
        slider.Click(0.5f);
        EXPECT_EQ(slider.ReadControlValue(), minimum + 50);
        slider.Click(1.0f);
        EXPECT_EQ(slider.ReadControlValue(), minimum + 100);
        slider.Step(1);
        EXPECT_EQ(slider.ReadControlValue(), minimum + 100);
        slider.Click(0.0f);
        slider.Step(-1);
        EXPECT_EQ(slider.ReadControlValue(), minimum);
    }
}

TEST(PluginSliderControl, CrossZeroAndSingleValueRangesRemainStable)
{
    PluginSliderControl<SliderProbe> signed_slider(nullptr, "signed", -100, 100);
    signed_slider.Click(0.5f);
    EXPECT_EQ(signed_slider.ReadControlValue(), 0);
    PluginSliderControl<SliderProbe> fixed_slider(nullptr, "fixed", -7, -7);
    fixed_slider.SetControlValue(-7);
    fixed_slider.Click(0.0f);
    fixed_slider.Click(1.0f);
    fixed_slider.Step(1);
    EXPECT_EQ(fixed_slider.ReadControlValue(), -7);
    EXPECT_EQ(fixed_slider.notifications, 0);
}

TEST(PluginListControl, SilentEmptySelectionDoesNotReportTheStockDefaultItem)
{
    ListProbe combo(nullptr, "list");
    combo.AddCaption(L"First");
    combo.AddCaption(L"Second");
    combo.SetSelection(-1);
    EXPECT_EQ(combo.GetActiveItem(), 0);
    EXPECT_EQ(combo.get_selection(), -1);
    EXPECT_TRUE(combo.text().empty());
    combo.SetSelection(1);
    EXPECT_EQ(combo.get_selection(), 1);
    combo.SetSelection(-1);
    combo.SetSelection(-1);
    EXPECT_EQ(combo.GetActiveItem(), 1);
    EXPECT_EQ(combo.get_selection(), -1);
    EXPECT_TRUE(combo.text().empty());
    EXPECT_EQ(combo.notifications, 0);
}

TEST(PluginListControl, EmptyOptionsStayUnselectedThroughSyncAndKeyboardInput)
{
    ListProbe combo(nullptr, "empty");
    combo.SetSelection(-1);
    combo.SetSelection(-1);
    combo.Key(vgui2::KEY_DOWN);
    combo.Key(vgui2::KEY_END);
    combo.Character(L'a');
    EXPECT_EQ(combo.get_selection(), -1);
    EXPECT_EQ(combo.notifications, 0);
}

TEST(PluginListControl, SelectingBlankOrEqualCaptionsStillReportsTheirRow)
{
    ListProbe combo(nullptr, "list");
    combo.AddCaption(L"");
    combo.AddCaption(L"");
    combo.SetSelection(-1);
    combo.Pick(0);
    EXPECT_EQ(combo.get_selection(), 0);
    EXPECT_EQ(combo.selection_notifications, 1);
    combo.notifications = combo.selection_notifications = 0;
    combo.Pick(1);
    EXPECT_EQ(combo.get_selection(), 1);
    EXPECT_EQ(combo.selection_notifications, 1);
    combo.notifications = combo.selection_notifications = 0;
    combo.Pick(1);
    EXPECT_EQ(combo.selection_notifications, 0);
    combo.SetSelection(-1);
    EXPECT_EQ(combo.get_selection(), -1);
    EXPECT_EQ(combo.notifications, 0);
    EXPECT_EQ(combo.selection_notifications, 0);
}

TEST(PluginListControl, KeyboardSelectionUsesTheHighlightedRow)
{
    ListProbe combo(nullptr, "list");
    combo.AddCaption(L"Alpha");
    combo.AddCaption(L"Beta");
    combo.SetSelection(-1);
    combo.Key(vgui2::KEY_END);
    EXPECT_EQ(combo.get_selection(), 1);
    EXPECT_EQ(combo.GetActiveItem(), 1);
    EXPECT_EQ(combo.selection_notifications, 1);
    combo.SetSelection(-1);
    combo.selection_notifications = 0;
    combo.Character(L'A');
    EXPECT_EQ(combo.get_selection(), 0);
    EXPECT_EQ(combo.selection_notifications, 1);
    combo.SetSelection(-1);
    combo.selection_notifications = 0;
    combo.Key(vgui2::KEY_DOWN);
    EXPECT_EQ(combo.get_selection(), 0);
    EXPECT_EQ(combo.selection_notifications, 1);
}

TEST(PluginListControl, ProgrammaticSelectionResetsTheKeyboardStartingRow)
{
    ListProbe combo(nullptr, "list");
    combo.AddCaption(L"Alpha");
    combo.AddCaption(L"Beta");
    combo.AddCaption(L"Gamma");
    combo.SetSelection(2);
    combo.Key(vgui2::KEY_HOME);
    combo.notifications = combo.selection_notifications = 0;
    combo.SetSelection(1);
    EXPECT_EQ(combo.notifications, 0);
    EXPECT_EQ(combo.selection_notifications, 0);
    combo.Key(vgui2::KEY_DOWN);
    EXPECT_EQ(combo.get_selection(), 2);
    EXPECT_EQ(combo.selection_notifications, 1);
}

TEST(PluginListControl, TabDoesNotCommitAPreviouslyHoveredItem)
{
    ListProbe combo(nullptr, "list");
    combo.AddCaption(L"Alpha");
    combo.AddCaption(L"Beta");
    combo.SetSelection(-1);
    combo.Hover(1);
    combo.Character(L'\t');
    EXPECT_EQ(combo.get_selection(), -1);
    EXPECT_EQ(combo.notifications, 0);
}
