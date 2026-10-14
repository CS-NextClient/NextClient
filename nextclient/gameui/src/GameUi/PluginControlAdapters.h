#pragma once

#include <cstdint>

#include <KeyValues.h>
#include <vgui/KeyCode.h>

namespace vgui2
{
    class Panel;
}

template <class Slider>
class PluginSliderControl : public Slider
{
public:
    PluginSliderControl(vgui2::Panel* parent, const char* name, int minimum, int maximum) :
        Slider(parent, name),
        minimum_(minimum)
    {
        // VGUI rounds mouse positions in float coordinates. A small nonnegative
        // range preserves signed values and precision near either integer limit.
        Slider::SetRange(0, static_cast<int>(static_cast<int64_t>(maximum) - minimum));
    }

    void SetControlValue(int value)
    {
        Slider::SetValue(static_cast<int>(static_cast<int64_t>(value) - minimum_), false);
    }

    int ReadControlValue()
    {
        return minimum_ + Slider::GetValue();
    }

private:
    int minimum_{};
};

template <class ComboBox>
class PluginListControl : public ComboBox
{
public:
    PluginListControl(vgui2::Panel* parent, const char* name) :
        ComboBox(parent, name, 8, false)
    {}

    void SetSelection(int selection)
    {
        selection_ = selection;
        if (selection >= 0)
        {
            ComboBox::SilentActivateItemByRow(selection);
            this->GetMenu()->SetCurrentlyHighlightedItem(this->GetItemIDFromRow(selection));
        }
        else
        {
            ComboBox::SetText(L"");
            this->GetMenu()->ClearCurrentlyHighlightedItem();
        }
    }

    int get_selection() const
    {
        return selection_;
    }

protected:
    virtual void SendSelectionChanged()
    {
        this->PostActionSignal(new KeyValues("TextChanged"));
    }

    void OnMenuItemSelected() override
    {
        const int previous = selection_;
        selection_ = this->GetRowByItemId(this->GetActiveItem());
        ComboBox::OnMenuItemSelected();
        if (selection_ != previous)
        {
            SendSelectionChanged();
        }
    }

    void OnKeyCodeTyped(vgui2::KeyCode code) override
    {
        ComboBox::OnKeyCodeTyped(code);
        switch (code)
        {
            case vgui2::KEY_HOME:
            case vgui2::KEY_END:
            case vgui2::KEY_PAGEUP:
            case vgui2::KEY_PAGEDOWN:
            case vgui2::KEY_UP:
            case vgui2::KEY_DOWN:
            case vgui2::KEY_ENTER:
                if (!this->IsDropdownVisible())
                {
                    SelectHighlightedItem();
                }
                break;
            default:
                break;
        }
    }

    void OnKeyTyped(wchar_t character) override
    {
        ComboBox::OnKeyTyped(character);
        if (character != L'\t')
        {
            SelectHighlightedItem();
        }
    }

private:
    void SelectHighlightedItem()
    {
        const int row = this->GetRowByItemId(this->GetMenu()->GetCurrentlyHighlightedItem());
        if (row >= 0 && row != selection_)
        {
            SetSelection(row);
            SendSelectionChanged();
        }
    }

    // VGUI's active item starts at zero and cannot be cleared. In particular,
    // blank captions cannot distinguish no selection from a selected item.
    int selection_ = -1;
};
