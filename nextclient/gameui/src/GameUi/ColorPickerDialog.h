#pragma once

#include <array>
#include <string>

#include <vgui_controls/Frame.h>

#include <ncl_math/color.h>

class CPixelPanel;

namespace vgui2
{
    class TextEntry;
} // namespace vgui2

// Picks a color by eye, laid out the way the game's own color dialog is: the color being
// chosen over the color it replaces, the value as three numbers, the shade square with the
// hue below it. The value is four floats over 0..1, which is what the square covers.
class CColorPickerDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CColorPickerDialog, vgui2::Frame);

public:
    class IListener
    {
    public:
        virtual ~IListener() = default;

        // Sent while the color is being chosen as well as when it is accepted, so the
        // model in front of the author changes under the cursor rather than after an OK.
        virtual void OnColorPicked(const char* param, const float rgba[4]) = 0;
    };

private:
    class Strip;
    class Preset;

    IListener* m_pListener;
    std::string m_Param;

    float m_Hue;
    float m_Saturation;
    float m_Value;
    float m_Alpha;

    ncl_math::ColorF m_Original;
    ncl_math::ColorF m_Defaults;

    Strip* m_pShade;
    Strip* m_pHue;
    Strip* m_pAlpha;

    // A color that carries no transparency hides the bar instead of showing one that
    // does nothing to what the caller reads back.
    bool m_WithAlpha;
    CPixelPanel* m_pNew;
    CPixelPanel* m_pOld;

    // Ready colors, offered as plain squares: naming them buys nothing when the color
    // itself is the label.
    static constexpr int kPresetCount = 10;
    std::array<Preset*, kPresetCount> m_Presets{};

    vgui2::TextEntry* m_pField;

    // Set while the field is being filled from the color, so the change message that
    // causes does not travel back and re-read it.
    bool m_Loading;

    // Set while the color is being read out of the field: writing the field back would
    // put the caret at its start after every keystroke.
    bool m_EditingField;

    // What was modal before this window took over. Restored on every close, including the
    // title bar's button and Escape, neither of which passes through the buttons below.
    vgui2::VPANEL m_PreviousModal;

public:
    CColorPickerDialog(
        vgui2::Panel* parent,
        IListener* listener,
        const char* param,
        const float rgba[4],
        const float defaults[4],
        bool with_alpha = true
    );

    void Activate() override;
    void OnClose() override;
    void OnCommand(const char* command) override;
    void PerformLayout() override;

    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

private:
    // Where a strip reports the fraction it was clicked at; presets report their own id.
    void OnStripPicked(int id, float x, float y);

    ncl_math::ColorF CurrentColor() const;
    void SetColor(const float rgba[4]);
    void RedrawShade();
    void MarkMatchingPreset();
    void RedrawBars();
    void UpdateField();
    void Publish();
};
