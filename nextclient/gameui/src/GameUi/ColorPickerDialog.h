#pragma once

#include <string>

#include <vgui_controls/Frame.h>

class CPixelPanel;

namespace vgui2
{
    class Label;
    class TextEntry;
} // namespace vgui2

// Where a gradient strip reports the fraction it was clicked at. Declared here rather than
// beside the strip itself so the dialog can be its own listener and no object has to exist
// between the two.
class IColorStripListener
{
public:
    virtual ~IColorStripListener() = default;

    virtual void OnStripPicked(int id, float x, float y) = 0;
};

// Picks a colour for a vec4 the program asked to be edited as one, laid out the way the
// game's own colour dialog is: the colour being chosen over the colour it replaces, the
// value as three numbers, the shade square with the hue below it.
//
// The value stays four floats: the material format carries nothing else, and a tint is a
// multiplier, so 0..1 is what the square covers. A number above one is still reachable by
// typing it into the row's own fields; this is for choosing a colour by eye.
class CColorPickerDialog : public vgui2::Frame, public IColorStripListener
{
    DECLARE_CLASS_SIMPLE(CColorPickerDialog, vgui2::Frame);

public:
    class IListener
    {
    public:
        virtual ~IListener() = default;

        // Sent while the colour is being chosen as well as when it is accepted, so the
        // model in front of the author changes under the cursor rather than after an OK.
        virtual void OnColorPicked(const char* param, const float rgba[4]) = 0;
    };

    CColorPickerDialog(
        vgui2::Panel* parent,
        IListener* listener,
        const char* param,
        const float rgba[4],
        const float defaults[4]
    );

    void Activate() override;
    void OnClose() override;
    void OnCommand(const char* command) override;
    void PerformLayout() override;

    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

    void OnStripPicked(int id, float x, float y) override;

private:
    void SetColor(const float rgba[4]);
    void RedrawShade();
    void RedrawBars();
    void UpdateField();
    void Publish();

    IListener* m_pListener;
    std::string m_Param;

    float m_Hue;
    float m_Saturation;
    float m_Value;
    float m_Alpha;

    float m_Original[4];
    float m_Defaults[4];

    CPixelPanel* m_pShade;
    CPixelPanel* m_pHue;
    CPixelPanel* m_pAlpha;
    CPixelPanel* m_pNew;
    CPixelPanel* m_pOld;

    vgui2::TextEntry* m_pField;

    // Set while the field is being filled from the colour, so the change message that
    // causes does not travel back and re-read it.
    bool m_Loading;

    // What was modal before this window took over. Restored on every close, including the
    // title bar's button and Escape, neither of which passes through the buttons below.
    vgui2::VPANEL m_PreviousModal;
};
