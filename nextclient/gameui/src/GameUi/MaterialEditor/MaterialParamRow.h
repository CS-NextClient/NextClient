#pragma once

#include <string>
#include <memory>

#include <vgui_controls/Panel.h>

#include <next_engine_mini/MaterialEditInterface.h>

#include "GameUi/Controls/DragValueLabel.h"

class ImageFileCache;
class CImageFilePanel;

namespace vgui2
{
    class ISurface;
    class Button;
    class ComboBox;
    class Label;
    class Slider;
    class TextEntry;
} // namespace vgui2

namespace material_editor
{
class CMaterialEditDialog;
class SwatchButton;

// One declared parameter, laid out according to what the program said about it: a slider
// where a range was declared, typed numbers where it was not, four fields for a vec4, a
// swatch and a picker where the program asked for a color, a path and a browser for an
// image.
//
// Every numeric field also has a caption that changes it when dragged sideways. An author
// is looking at the model, not at the number, and reaching for the keyboard to try 0.4
// instead of 0.35 breaks that.
class CMaterialParamRow : public vgui2::Panel, public CDragValueLabel::IListener
{
    DECLARE_CLASS_SIMPLE(CMaterialParamRow, vgui2::Panel);

private:
    CMaterialEditDialog* m_pOwner;
    render::MaterialEditInterface& m_Editor;

    std::string m_Name;
    std::string m_Label;
    std::string m_Hint;
    render::MaterialEditParamType m_Type;
    render::MaterialEditParamWidget m_Widget;
    bool m_HasRange;
    float m_Min;
    float m_Max;

    vgui2::Label* m_pLabel;
    CDragValueLabel* m_pComponents[4];
    vgui2::Slider* m_pSlider;
    vgui2::ComboBox* m_pChoices;
    vgui2::TextEntry* m_pFields[4];
    vgui2::Button* m_pBrowse;
    SwatchButton* m_pSwatch;
    CImageFilePanel* m_pPreview;

    // Set while the controls are being filled from the editor, so the change messages that
    // causes do not travel back and rewrite what was just read.
    bool m_Loading;

    // Accumulated value of the current drag gesture.
    float m_DragValue[4];

public:
    CMaterialParamRow(
        vgui2::Panel* parent,
        CMaterialEditDialog* owner,
        render::MaterialEditInterface& editor,
        std::shared_ptr<ImageFileCache> images,
        vgui2::ISurface& surface,
        const render::MaterialEditParam& param
    );

    // Pulls the value out of the editor into the controls, without sending it back.
    void Refresh();

    void PerformLayout() override;

    MESSAGE_FUNC_PTR(OnSliderMoved, "SliderMoved", panel);
    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

    void OnDragValue(int index, int pixels, bool fine, bool coarse) override;
    void OnDragValueBegin(int index) override;
    void OnDragValueEnd(int index) override;

    const std::string& get_name() const
    {
        return m_Name;
    }

    bool is_color() const
    {
        return m_Widget == render::MaterialEditParamWidget::Color;
    }

    void SetInstanceValue(float value);

private:
    bool PushScalar(float value);
    bool PushVec4();
    void PushTexture();

    // How much one pixel of drag is worth. A declared range spreads over a fixed number of
    // pixels so every parameter feels the same however its numbers are scaled; without one
    // there is nothing to scale against and a plain step is used.
    float StepPerPixel(bool fine, bool coarse) const;

    float SliderToValue(int position) const;
    int ValueToSlider(float value) const;

    void UpdateSwatch();

public:
    static int HeightFor(const render::MaterialEditParam& param);
};

} // namespace material_editor
