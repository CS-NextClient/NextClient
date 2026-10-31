#include "GameUi/MaterialEditor/MaterialParamRow.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

#include <Color.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Tooltip.h>

#include "GameUi/Controls/ImageFilePanel.h"
#include "GameUi/MaterialEditor/MaterialEditDialog.h"

namespace material_editor
{

using namespace vgui2;

namespace
{
    // Slider ticks control input increments independently of stored float precision.
    constexpr int kSliderTicks = 1000;

    // How far a drag has to travel to cross a whole declared range. Chosen so a parameter
    // that runs 0..1 moves in steps an author can land on without a steady hand.
    constexpr float kPixelsPerRange = 220.0f;

    // What a pixel is worth where the program declared no range. A tiling factor and an
    // offset are the usual cases and both live around one.
    constexpr float kStepWithoutRange = 0.005f;

    constexpr int kRowHeight = 26;
    constexpr int kLabelWidth = 150;
    constexpr int kFieldWidth = 62;
    constexpr int kPreviewSize = 22;
    constexpr int kComponentLabelWidth = 12;
    constexpr int kSwatchWidth = 34;
    constexpr const char* kComponentNames[4] = {"X", "Y", "Z", "W"};
    constexpr const char* kColorNames[4] = {"R", "G", "B", "A"};

    void SetNumber(TextEntry* entry, float value)
    {
        char text[32];
        Q_snprintf(text, sizeof(text), "%.*g", std::numeric_limits<float>::max_digits10, value);
        entry->SetText(text);
    }

    float ReadNumber(TextEntry* entry)
    {
        char text[64] = {};
        entry->GetText(text, sizeof(text));

        return static_cast<float>(atof(text));
    }

}

// The button paints its own swatch because focus repainting would cover a separate panel.
class SwatchButton : public Button
{
    DECLARE_CLASS_SIMPLE(SwatchButton, Button);

private:
    ISurface& m_Surface;
    Color m_Color;

public:
    SwatchButton(Panel* parent, Panel* owner, ISurface& surface, const char* command) :
        BaseClass(parent, "Swatch", "", owner, command),
        m_Surface(surface)
    {
        m_Color = Color(255, 255, 255, 255);

        SetPaintBackgroundEnabled(false);
        SetPaintBorderEnabled(false);
    }

    void SetSwatch(const float rgba[4])
    {
        m_Color = Color(
            static_cast<int>(std::clamp(rgba[0], 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<int>(std::clamp(rgba[1], 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<int>(std::clamp(rgba[2], 0.0f, 1.0f) * 255.0f + 0.5f),
            static_cast<int>(std::clamp(rgba[3], 0.0f, 1.0f) * 255.0f + 0.5f)
        );

        Repaint();
    }

    void Paint() override
    {
        BaseClass::Paint();

        int wide = 0;
        int tall = 0;
        GetSize(wide, tall);

        // Over a mid gray, so the alpha of a tint shows as a wash rather than as whatever
        // the dialog happens to have behind it.
        m_Surface.DrawSetColor(96, 96, 96, 255);
        m_Surface.DrawFilledRect(0, 0, wide, tall);

        m_Surface.DrawSetColor(m_Color);
        m_Surface.DrawFilledRect(0, 0, wide, tall);

        m_Surface.DrawSetColor(0, 0, 0, 255);
        m_Surface.DrawOutlinedRect(0, 0, wide, tall);
    }

    // Panels here are painted when something has marked them dirty rather than once a
    // frame, and a color that only changes when it is set has nothing to mark it.
    void OnThink() override
    {
        BaseClass::OnThink();

        Repaint();
    }
};

int CMaterialParamRow::HeightFor(const render::MaterialEditParam& param)
{
    return kRowHeight;
}

CMaterialParamRow::CMaterialParamRow(
    Panel* parent,
    CMaterialEditDialog* owner,
    render::MaterialEditInterface& editor,
    std::shared_ptr<ImageFileCache> images,
    ISurface& surface,
    const render::MaterialEditParam& param
) :
    BaseClass(parent, "MaterialParamRow"),
    m_Editor(editor)
{
    m_pOwner = owner;
    m_Name = param.name;
    m_Label = param.label;
    m_Hint = param.hint != nullptr ? param.hint : "";
    m_Type = param.type;
    m_Widget = param.widget;
    m_HasRange = param.has_range;
    m_Min = param.min_value;
    m_Max = param.max_value;
    m_Loading = false;

    for (float& component : m_DragValue)
    {
        component = 0.0f;
    }

    m_pSlider = nullptr;
    m_pChoices = nullptr;
    m_pBrowse = nullptr;
    m_pSwatch = nullptr;
    m_pPreview = nullptr;

    for (TextEntry*& field : m_pFields)
    {
        field = nullptr;
    }

    for (CDragValueLabel*& component : m_pComponents)
    {
        component = nullptr;
    }

    // Index -1 is the whole parameter, which for a scalar is its only value.
    if (m_Widget == render::MaterialEditParamWidget::Choice)
    {
        m_pLabel = new Label(this, "Label", param.label);
    }
    else
    {
        m_pLabel = new CDragValueLabel(this, "Label", param.label, this, -1);
    }

    // The explanation is what the label cannot carry: several of these parameters are a
    // width or a threshold and no single word names them.
    if (param.hint != nullptr && *param.hint != '\0')
    {
        m_pLabel->GetTooltip()->SetText(param.hint);
    }

    if (m_Type == render::MaterialEditParamType::Scalar)
    {
        if (m_Widget == render::MaterialEditParamWidget::Choice)
        {
            const char* program = m_Editor.get_edited_program();
            const int count = m_Editor.GetChoiceCount(program, m_Name.c_str());
            m_pChoices = new ComboBox(this, "Choice", std::min(count, 12), false);

            for (int index = 0; index < count; index++)
            {
                m_pChoices->AddItem(m_Editor.GetChoiceLabel(program, m_Name.c_str(), index), nullptr);
            }

            m_pChoices->SetEnabled(count > 0);
            m_pChoices->AddActionSignalTarget(this);

            return;
        }

        if (m_HasRange)
        {
            m_pSlider = new Slider(this, "Slider");
            m_pSlider->SetRange(0, kSliderTicks);
            m_pSlider->AddActionSignalTarget(this);
        }

        m_pFields[0] = new TextEntry(this, "Value");
        m_pFields[0]->AddActionSignalTarget(this);

        return;
    }

    if (m_Type == render::MaterialEditParamType::Vec4)
    {
        bool color = is_color();

        for (int i = 0; i < 4; i++)
        {
            m_pComponents[i] = new CDragValueLabel(this, "Component", color ? kColorNames[i] : kComponentNames[i], this, i);

            m_pFields[i] = new TextEntry(this, "Value");
            m_pFields[i]->AddActionSignalTarget(this);
        }

        if (color)
        {
            // The command goes in the constructor, not into a SetCommand afterwards: the
            // constructor registers the target only when it is given a command too, so a
            // button built the other way fires into nowhere and simply does nothing.
            std::string command = "color " + m_Name;

            m_pSwatch = new SwatchButton(this, m_pOwner, surface, command.c_str());
        }

        return;
    }

    std::string command = "pick " + m_Name;

    m_pFields[0] = new TextEntry(this, "Path");
    m_pFields[0]->AddActionSignalTarget(this);
    m_pBrowse = new Button(this, "Browse", "...", m_pOwner, command.c_str());
    m_pPreview = new CImageFilePanel(this, "Preview", std::move(images), surface);
}

void CMaterialParamRow::PerformLayout()
{
    BaseClass::PerformLayout();

    int wide = 0;
    int tall = 0;
    GetSize(wide, tall);

    m_pLabel->SetBounds(0, 3, kLabelWidth, tall - 6);

    int x = kLabelWidth + 4;
    int right = wide - 4;

    if (m_Type == render::MaterialEditParamType::Scalar)
    {
        if (m_pChoices != nullptr)
        {
            m_pChoices->SetBounds(x, 3, right - x, tall - 6);
            return;
        }

        if (m_pSlider != nullptr)
        {
            m_pSlider->SetBounds(x, 2, right - x - kFieldWidth - 4, tall - 4);
        }

        m_pFields[0]->SetBounds(right - kFieldWidth, 3, kFieldWidth, tall - 6);
        return;
    }

    if (m_Type == render::MaterialEditParamType::Vec4)
    {
        int reserved = is_color() ? kSwatchWidth + 6 : 0;
        int span = (right - reserved - x) / 4;

        for (int i = 0; i < 4; i++)
        {
            int component_x = x + i * span;

            m_pComponents[i]->SetBounds(component_x, 3, kComponentLabelWidth, tall - 6);
            m_pFields[i]->SetBounds(component_x + kComponentLabelWidth, 3, span - kComponentLabelWidth - 4, tall - 6);
        }

        if (is_color())
        {
            m_pSwatch->SetBounds(right - kSwatchWidth, 4, kSwatchWidth, tall - 8);
        }

        return;
    }

    m_pPreview->SetBounds(right - kPreviewSize, 2, kPreviewSize, kPreviewSize);
    m_pBrowse->SetBounds(right - kPreviewSize - 26, 3, 22, tall - 6);
    m_pFields[0]->SetBounds(x, 3, right - kPreviewSize - 30 - x, tall - 6);
}

void CMaterialParamRow::Refresh()
{
    if (m_pOwner->is_closing())
    {
        return;
    }

    const bool mixed = m_Editor.is_param_mixed(m_Name.c_str());
    if (mixed)
    {
        const wchar_t* label = g_pVGuiLocalize->Find(m_Label.c_str());
        wchar_t fallback[256]{};
        if (label == nullptr)
        {
            g_pVGuiLocalize->ConvertANSIToUnicode(m_Label.c_str(), fallback, sizeof(fallback));
            label = fallback;
        }
        wchar_t text[288]{};
        V_snwprintf(text, std::size(text), L"* %s", label);
        m_pLabel->SetText(text);
        m_pLabel->GetTooltip()->SetText("#GameUI_MatEdit_MixedValues");
    }
    else
    {
        m_pLabel->SetText(m_Label.c_str());
        m_pLabel->GetTooltip()->SetText(m_Hint.c_str());
    }
    m_Loading = true;

    if (m_Type == render::MaterialEditParamType::Scalar)
    {
        float value = 0.0f;
        m_Editor.get_scalar(m_Name.c_str(), &value);

        if (m_pChoices != nullptr)
        {
            m_pChoices->SilentActivateItemByRow(static_cast<int>(value));

            if (mixed)
            {
                // SetText is silent; a distinct caption makes the first choice observable too.
                m_pChoices->SetText("#GameUI_MatEdit_MixedChoice");
            }
        }
        else
        {
            SetNumber(m_pFields[0], value);
        }

        if (m_pSlider != nullptr)
        {
            m_pSlider->SetValue(ValueToSlider(value), false);
        }
    }
    else if (m_Type == render::MaterialEditParamType::Vec4)
    {
        float value[4] = {};
        m_Editor.get_vec4(m_Name.c_str(), value);

        for (int i = 0; i < 4; i++)
        {
            SetNumber(m_pFields[i], value[i]);
        }

        UpdateSwatch();
    }
    else
    {
        const char* path = m_Editor.get_texture(m_Name.c_str());
        m_pFields[0]->SetText(path);
        m_pPreview->SetImageFile(path);
    }

    m_Loading = false;
}

void CMaterialParamRow::UpdateSwatch()
{
    if (m_pSwatch == nullptr || m_pOwner->is_closing())
    {
        return;
    }

    float value[4] = {};
    m_Editor.get_vec4(m_Name.c_str(), value);

    m_pSwatch->SetSwatch(value);
}

void CMaterialParamRow::SetInstanceValue(float value)
{
    PushScalar(value);
    Refresh();
}

float CMaterialParamRow::StepPerPixel(bool fine, bool coarse) const
{
    float step = m_HasRange ? (m_Max - m_Min) / kPixelsPerRange : kStepWithoutRange;

    if (fine)
    {
        step *= 0.1f;
    }

    if (coarse)
    {
        step *= 10.0f;
    }

    return step;
}

void CMaterialParamRow::OnDragValueBegin(int index)
{
    if (m_pOwner->is_closing())
    {
        return;
    }

    for (float& component : m_DragValue)
    {
        component = 0.0f;
    }

    if (m_Type == render::MaterialEditParamType::Scalar)
    {
        m_Editor.get_scalar(m_Name.c_str(), &m_DragValue[0]);
    }
    else if (m_Type == render::MaterialEditParamType::Vec4)
    {
        m_Editor.get_vec4(m_Name.c_str(), m_DragValue);
    }
}

void CMaterialParamRow::OnDragValue(int index, int pixels, bool fine, bool coarse)
{
    if (m_pOwner->is_closing())
    {
        return;
    }

    if (m_Widget == render::MaterialEditParamWidget::Choice)
    {
        return;
    }

    float step = StepPerPixel(fine, coarse) * pixels;

    // Held at the ends, so that dragging far past a limit and coming back does not first
    // have to undo the distance traveled outside it.
    if (m_Type == render::MaterialEditParamType::Scalar)
    {
        m_DragValue[0] += step;

        if (m_HasRange)
        {
            m_DragValue[0] = std::clamp(m_DragValue[0], m_Min, m_Max);
        }

        PushScalar(m_DragValue[0]);
    }
    else if (m_Type == render::MaterialEditParamType::Vec4 && index >= 0 && index < 4)
    {
        m_DragValue[index] += step;

        if (m_HasRange)
        {
            m_DragValue[index] = std::clamp(m_DragValue[index], m_Min, m_Max);
        }

        m_Editor.SetVec4(m_Name.c_str(), m_DragValue);
    }
    else
    {
        return;
    }

    Refresh();
    m_pOwner->OnRowChanged();
}

void CMaterialParamRow::OnDragValueEnd(int index)
{
    if (m_pOwner->is_closing())
    {
        return;
    }

    Refresh();
}

void CMaterialParamRow::OnSliderMoved(Panel* panel)
{
    if (m_pOwner->is_closing())
    {
        return;
    }

    if (m_Loading || m_pSlider == nullptr)
    {
        return;
    }

    PushScalar(SliderToValue(m_pSlider->GetValue()));

    // Read the stored value back after the editor applies the declared bounds.
    m_Loading = true;

    float stored = 0.0f;

    if (!m_pOwner->is_closing() && m_Editor.get_scalar(m_Name.c_str(), &stored))
    {
        SetNumber(m_pFields[0], stored);
    }

    m_Loading = false;

    m_pOwner->OnRowChanged();
}

void CMaterialParamRow::OnTextChanged(Panel* panel)
{
    if (m_pOwner->is_closing())
    {
        return;
    }

    if (m_Loading)
    {
        return;
    }

    if (m_Type == render::MaterialEditParamType::Scalar)
    {
        if (m_pChoices != nullptr)
        {
            if (panel != m_pChoices)
            {
                return;
            }

            const int row = m_pChoices->GetRowByItemId(m_pChoices->GetActiveItem());
            float stored = 0.0f;
            const bool mixed = m_Editor.is_param_mixed(m_Name.c_str());

            if (row < 0 || (!mixed && m_Editor.get_scalar(m_Name.c_str(), &stored) && stored == static_cast<float>(row)))
            {
                return;
            }

            PushScalar(static_cast<float>(row));
            Refresh();
            m_pOwner->OnRowChanged();
            return;
        }

        if (!PushScalar(ReadNumber(m_pFields[0])))
        {
            Refresh();
            return;
        }

        if (m_pSlider != nullptr)
        {
            m_Loading = true;

            float stored = 0.0f;

            if (!m_pOwner->is_closing() && m_Editor.get_scalar(m_Name.c_str(), &stored))
            {
                m_pSlider->SetValue(ValueToSlider(stored), false);
            }

            m_Loading = false;
        }
    }
    else if (m_Type == render::MaterialEditParamType::Vec4)
    {
        if (!PushVec4())
        {
            Refresh();
            return;
        }

        UpdateSwatch();
    }
    else
    {
        PushTexture();
    }

    m_pOwner->OnRowChanged();
}

bool CMaterialParamRow::PushScalar(float value)
{
    return !m_pOwner->is_closing() && m_Editor.SetScalar(m_Name.c_str(), value);
}

bool CMaterialParamRow::PushVec4()
{
    if (m_pOwner->is_closing())
    {
        return false;
    }

    float value[4];

    for (int i = 0; i < 4; i++)
    {
        value[i] = ReadNumber(m_pFields[i]);
    }

    return m_Editor.SetVec4(m_Name.c_str(), value);
}

void CMaterialParamRow::PushTexture()
{
    if (m_pOwner->is_closing())
    {
        return;
    }

    char path[256] = {};
    m_pFields[0]->GetText(path, sizeof(path));

    // A refused path leaves the parameter as it was rather than half-applied; the preview
    // showing nothing is what tells the author the path is not one the client will open.
    m_Editor.SetTexture(m_Name.c_str(), path);
    m_pPreview->SetImageFile(m_Editor.get_texture(m_Name.c_str()));
}

float CMaterialParamRow::SliderToValue(int position) const
{
    float t = static_cast<float>(position) / static_cast<float>(kSliderTicks);

    return m_Min + (m_Max - m_Min) * t;
}

int CMaterialParamRow::ValueToSlider(float value) const
{
    if (!(m_Max > m_Min))
    {
        return 0;
    }

    float t = (value - m_Min) / (m_Max - m_Min);

    return std::clamp(static_cast<int>(t * kSliderTicks + 0.5f), 0, kSliderTicks);
}

} // namespace material_editor
