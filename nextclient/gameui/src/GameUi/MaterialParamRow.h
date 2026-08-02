#pragma once

#include <string>

#include <vgui_controls/Panel.h>

#include <next_engine_mini/MaterialEditInterface.h>

#include "Controls/DragValueLabel.h"

class CImageFilePanel;
class CMaterialEditDialog;
class SwatchButton;

namespace vgui2
{
    class Button;
    class Label;
    class Slider;
    class TextEntry;
} // namespace vgui2

// One declared parameter, laid out according to what the program said about it: a slider
// where a range was declared, typed numbers where it was not, four fields for a vec4, a
// swatch and a picker where the program asked for a colour, a path and a browser for an
// image.
//
// Every numeric field also has a caption that changes it when dragged sideways. An author
// is looking at the model, not at the number, and reaching for the keyboard to try 0.4
// instead of 0.35 breaks that.
class CMaterialParamRow : public vgui2::Panel, public CDragValueLabel::IListener
{
    DECLARE_CLASS_SIMPLE(CMaterialParamRow, vgui2::Panel);

public:
    CMaterialParamRow(vgui2::Panel* parent, CMaterialEditDialog* owner, const MaterialEditParam& param);

    static int HeightFor(const MaterialEditParam& param);

    // Pulls the value out of the editor into the controls, without sending it back.
    void Refresh();

    void PerformLayout() override;

    MESSAGE_FUNC_PTR(OnSliderMoved, "SliderMoved", panel);
    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

    void OnDragValue(int index, int pixels, bool fine, bool coarse) override;
    void OnDragValueBegin(int index) override;
    void OnDragValueEnd(int index) override;

    const std::string& name() const { return m_Name; }
    bool is_instance() const { return m_Instance; }
    bool is_color() const { return m_Widget == MaterialEditParamWidget::Color; }

    void SetInstanceValue(float value);

private:
    void PushScalar(float value);
    void PushVec4();
    void PushTexture();

    // How much one pixel of drag is worth. A declared range spreads over a fixed number of
    // pixels so every parameter feels the same however its numbers are scaled; without one
    // there is nothing to scale against and a plain step is used.
    float StepPerPixel(bool fine, bool coarse) const;

    float SliderToValue(int position) const;
    int ValueToSlider(float value) const;

    void UpdateSwatch();

    CMaterialEditDialog* m_pOwner;

    std::string m_Name;
    MaterialEditParamType m_Type;
    MaterialEditParamWidget m_Widget;
    bool m_Instance;
    bool m_HasRange;
    float m_Min;
    float m_Max;
    float m_Quantum;

    CDragValueLabel* m_pLabel;
    CDragValueLabel* m_pComponents[4];
    vgui2::Slider* m_pSlider;
    vgui2::TextEntry* m_pFields[4];
    vgui2::Button* m_pBrowse;
    SwatchButton* m_pSwatch;
    CImageFilePanel* m_pPreview;

    // Set while the controls are being filled from the editor, so the change messages that
    // causes do not travel back and rewrite what was just read.
    bool m_Loading;

    // The value as the drag has moved it, before the editor snaps it to the declared step.
    // Reading the stored value back each time instead would make a parameter on a coarse
    // grid immovable: every step smaller than the grid rounds to where it started.
    float m_DragValue[4];
};
