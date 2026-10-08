#include "ColorPickerDialog.h"

#include "Controls/PixelPanel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <vector>

#include <vgui/Cursor.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/MouseCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui2;

namespace
{
    constexpr int kShadeWide = 300;
    constexpr int kShadeTall = 160;
    constexpr int kBarTall = 18;
    constexpr int kSwatchWide = 140;

    constexpr int kStripShade = 0;
    constexpr int kStripHue = 1;
    constexpr int kStripAlpha = 2;
    constexpr int kStripPreset = 100;  // plus the index of the preset

    constexpr int kPresetTall = 20;
    constexpr int kPresetGap = 3;

    // half the side of the box that marks a point in the shade square, and half the width
    // of the bar that marks a position on a one-axis strip
    constexpr int kMarkerRadius = 5;
    constexpr int kCaretRadius = 2;

    // Kept as bytes: these are the colors a config carries, and the mark below compares
    // what the picker holds against them without a detour through fractions.
    constexpr ncl_math::Color kPresetColors[] = {
        {50, 250, 50},
        {250, 50, 50},
        {50, 50, 250},
        {250, 250, 50},
        {50, 250, 250},
        {250, 50, 250},
        {255, 140, 0},
        {250, 250, 250},
        {128, 128, 128},
        {10, 10, 10},
    };

    // the hue/value round trip can shift a component by a step, which is not a different color
    constexpr int kPresetMatchTolerance = 2;

    uint8_t ToByte(float value)
    {
        return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    ncl_math::Color ToBytes(const ncl_math::ColorF& color)
    {
        return ncl_math::Color(ToByte(color.r), ToByte(color.g), ToByte(color.b), ToByte(color.a));
    }

    // Four filled rects rather than DrawOutlinedRect: the engine surface puts down only the
    // top edge of that call, so a frame asked for over a gradient comes out as a dash.
    void FrameRect(int x0, int y0, int x1, int y1)
    {
        surface()->DrawFilledRect(x0, y0, x1, y0 + 1);
        surface()->DrawFilledRect(x0, y1 - 1, x1, y1);
        surface()->DrawFilledRect(x0, y0 + 1, x0 + 1, y1 - 1);
        surface()->DrawFilledRect(x1 - 1, y0 + 1, x1, y1 - 1);
    }

    // Two frames, dark around light, so the mark reads over any color a gradient puts
    // under it. A line would not do: the surface draws one additively, and the dark half
    // of the mark then adds nothing at all.
    void Marker(int x0, int y0, int x1, int y1)
    {
        surface()->DrawSetColor(Color(0, 0, 0, 255));
        FrameRect(x0, y0, x1, y1);

        surface()->DrawSetColor(Color(255, 255, 255, 255));
        FrameRect(x0 + 1, y0 + 1, x1 - 1, y1 - 1);
    }
}

// A ready color, and the mark that says the picker currently holds it.
class CColorPickerDialog::Preset : public CPixelPanel
{
    DECLARE_CLASS_SIMPLE(Preset, CPixelPanel);

private:
    CColorPickerDialog* m_pOwner;
    int m_Id;
    bool m_Selected{};

public:
    Preset(CColorPickerDialog* owner, const char* name, int id) :
        BaseClass(owner, name, 2, 2)
    {
        m_pOwner = owner;
        m_Id = id;

        SetMouseInputEnabled(true);
        SetCursor(dc_hand);
    }

    void OnMousePressed(MouseCode code) override
    {
        if (code == MOUSE_LEFT)
        {
            m_pOwner->OnStripPicked(m_Id, 0.0f, 0.0f);
        }
    }

    void SetSelected(bool selected)
    {
        m_Selected = selected;
    }

    void Paint() override
    {
        BaseClass::Paint();

        if (!m_Selected)
        {
            return;
        }

        int wide = 0;
        int tall = 0;
        GetSize(wide, tall);

        Marker(0, 0, wide, tall);
    }
};

// A gradient strip that reports where it was clicked and keeps reporting while the
// button is held, and marks where the current value sits. One class for all three: the
// only thing that differs is what the dialog does with the two fractions.
class CColorPickerDialog::Strip : public CPixelPanel
{
    DECLARE_CLASS_SIMPLE(Strip, CPixelPanel);

private:
    CColorPickerDialog* m_pOwner;
    int m_Id;
    bool m_TwoAxis;
    bool m_Dragging;
    float m_MarkerX;
    float m_MarkerY;

public:
    Strip(CColorPickerDialog* owner, const char* name, int width, int height, int id, bool two_axis) :
        BaseClass(owner, name, width, height)
    {
        m_pOwner = owner;
        m_Id = id;
        m_TwoAxis = two_axis;
        m_Dragging = false;
        m_MarkerX = 0.0f;
        m_MarkerY = 0.5f;

        SetMouseInputEnabled(true);
        SetCursor(dc_crosshair);
    }

    void SetMarker(float x, float y)
    {
        m_MarkerX = x;
        m_MarkerY = y;
    }

    void Paint() override
    {
        BaseClass::Paint();

        int wide = 0;
        int tall = 0;
        GetSize(wide, tall);

        int x = static_cast<int>(m_MarkerX * (wide - 1));

        if (!m_TwoAxis)
        {
            Marker(x - kCaretRadius, 0, x + kCaretRadius + 1, tall);
            return;
        }

        int y = static_cast<int>(m_MarkerY * (tall - 1));

        Marker(x - kMarkerRadius, y - kMarkerRadius, x + kMarkerRadius + 1, y + kMarkerRadius + 1);
    }

    void OnMousePressed(MouseCode code) override
    {
        if (code != MOUSE_LEFT)
        {
            return;
        }

        m_Dragging = true;
        input()->SetMouseCapture(GetVPanel());
        Report();
    }

    void OnMouseReleased(MouseCode code) override
    {
        m_Dragging = false;
        input()->SetMouseCapture(0);
    }

    void OnCursorMoved(int x, int y) override
    {
        if (m_Dragging)
        {
            Report();
        }
    }

private:
    void Report()
    {
        int x = 0;
        int y = 0;
        input()->GetCursorPos(x, y);
        ScreenToLocal(x, y);

        int wide = 0;
        int tall = 0;
        GetSize(wide, tall);

        // Clamped rather than ignored: a drag that leaves the strip should hold the end
        // it left by, not stop responding halfway through a gesture.
        float fx = wide > 1 ? std::clamp(static_cast<float>(x) / (wide - 1), 0.0f, 1.0f) : 0.0f;
        float fy = tall > 1 ? std::clamp(static_cast<float>(y) / (tall - 1), 0.0f, 1.0f) : 0.0f;

        m_pOwner->OnStripPicked(m_Id, fx, fy);
    }
};

CColorPickerDialog::CColorPickerDialog(
    Panel* parent,
    IListener* listener,
    const char* param,
    const float rgba[4],
    const float defaults[4],
    bool with_alpha
) :
    BaseClass(parent, "CColorPickerDialog")
{
    m_pListener = listener;
    m_Param = param;
    m_Loading = false;
    m_EditingField = false;
    m_PreviousModal = 0;
    m_WithAlpha = with_alpha;

    m_Hue = 0.0f;

    m_Original = {rgba[0], rgba[1], rgba[2], rgba[3]};
    m_Defaults = {defaults[0], defaults[1], defaults[2], defaults[3]};

    SetTitle("#GameUI_ColorPickerTitle", true);
    SetBounds(0, 0, 500, (with_alpha ? 300 : 276) + kPresetTall + 10);
    SetSizeable(false);
    SetDeleteSelfOnClose(true);

    m_pNew = new CPixelPanel(this, "New", 2, 2);
    m_pOld = new CPixelPanel(this, "Old", 2, 2);
    m_pNew->SetMouseInputEnabled(false);
    m_pOld->SetMouseInputEnabled(false);

    m_pField = new TextEntry(this, "Value");
    m_pField->AddActionSignalTarget(this);

    new Button(this, "Defaults", "#GameUI_ViewDefaultsBtn", this, "defaults");

    m_pShade = new Strip(this, "Shade", kShadeWide, kShadeTall, kStripShade, true);
    m_pHue = new Strip(this, "Hue", 256, 1, kStripHue, false);
    m_pAlpha = new Strip(this, "Alpha", 256, 1, kStripAlpha, false);
    m_pAlpha->SetVisible(with_alpha);

    static_assert(std::size(kPresetColors) == kPresetCount);

    for (int i = 0; i < kPresetCount; i++)
    {
        char name[32];
        Q_snprintf(name, sizeof(name), "Preset%d", i);

        Preset* preset = new Preset(this, name, kStripPreset + i);
        preset->SetSolidColor(kPresetColors[i]);
        m_Presets[i] = preset;
    }

    new Button(this, "Ok", "#GameUI_OK", this, "ok");
    new Button(this, "Cancel", "#GameUI_Cancel", this, "cancel");

    // The hue bar never changes, so it is drawn once here rather than on every redraw.
    std::vector<uint8_t> hue(256 * 4);

    for (int x = 0; x < 256; x++)
    {
        ncl_math::ColorF rgb = ncl_math::HsvToRgb(x / 255.0f, 1.0f, 1.0f);

        hue[x * 4 + 0] = ToByte(rgb.r);
        hue[x * 4 + 1] = ToByte(rgb.g);
        hue[x * 4 + 2] = ToByte(rgb.b);
        hue[x * 4 + 3] = 255;
    }

    m_pHue->SetPixels(hue.data());

    m_pOld->SetSolidColor(ToBytes(m_Original.WithAlpha(1.0f)));

    SetColor(rgba);
}

ncl_math::ColorF CColorPickerDialog::CurrentColor() const
{
    return ncl_math::HsvToRgb(m_Hue, m_Saturation, m_Value).WithAlpha(m_Alpha);
}

void CColorPickerDialog::SetColor(const float rgba[4])
{
    ncl_math::RgbToHsv({rgba[0], rgba[1], rgba[2]}, m_Hue, m_Saturation, m_Value);
    m_Alpha = std::clamp(rgba[3], 0.0f, 1.0f);

    RedrawShade();
    RedrawBars();
    UpdateField();
}

void CColorPickerDialog::PerformLayout()
{
    BaseClass::PerformLayout();

    int wide = 0;
    int tall = 0;
    GetSize(wide, tall);

    int left = 12;
    int top = 34;

    // The color being chosen sits directly over the color it replaces, so the comparison
    // is one glance rather than a memory of what it looked like a moment ago.
    m_pNew->SetBounds(left, top, kSwatchWide, 90);
    m_pOld->SetBounds(left, top + 90, kSwatchWide, 60);

    m_pField->SetBounds(left, top + 162, kSwatchWide, 22);

    if (Panel* defaults = FindChildByName("Defaults"))
    {
        defaults->SetBounds(left, top + 192, kSwatchWide, 24);
    }

    int right = left + kSwatchWide + 14;

    m_pShade->SetBounds(right, top, kShadeWide, kShadeTall);
    m_pHue->SetBounds(right, top + kShadeTall + 10, kShadeWide, kBarTall);
    if (m_WithAlpha)
        m_pAlpha->SetBounds(right, top + kShadeTall + 10 + kBarTall + 6, kShadeWide, kBarTall);

    int presets_y = top + kShadeTall + 10 + kBarTall + (m_WithAlpha ? kBarTall + 6 : 0) + 10;
    int preset_wide = (kShadeWide - kPresetGap * (kPresetCount - 1)) / kPresetCount;

    for (int i = 0; i < static_cast<int>(m_Presets.size()); i++)
    {
        m_Presets[i]->SetBounds(right + i * (preset_wide + kPresetGap), presets_y, preset_wide, kPresetTall);
    }

    if (Panel* ok = FindChildByName("Ok"))
    {
        ok->SetBounds(wide - 172, tall - 34, 76, 24);
    }

    if (Panel* cancel = FindChildByName("Cancel"))
    {
        cancel->SetBounds(wide - 90, tall - 34, 76, 24);
    }
}

void CColorPickerDialog::RedrawShade()
{
    std::vector<uint8_t> pixels(static_cast<size_t>(kShadeWide) * kShadeTall * 4);

    for (int y = 0; y < kShadeTall; y++)
    {
        float value = 1.0f - static_cast<float>(y) / (kShadeTall - 1);

        for (int x = 0; x < kShadeWide; x++)
        {
            float saturation = static_cast<float>(x) / (kShadeWide - 1);

            ncl_math::ColorF rgb = ncl_math::HsvToRgb(m_Hue, saturation, value);

            size_t at = (static_cast<size_t>(y) * kShadeWide + x) * 4;
            pixels[at + 0] = ToByte(rgb.r);
            pixels[at + 1] = ToByte(rgb.g);
            pixels[at + 2] = ToByte(rgb.b);
            pixels[at + 3] = 255;
        }
    }

    m_pShade->SetPixels(pixels.data());
    m_pShade->SetMarker(m_Saturation, 1.0f - m_Value);
}

void CColorPickerDialog::RedrawBars()
{
    ncl_math::ColorF color = CurrentColor();

    std::vector<uint8_t> alpha(256 * 4);

    for (int x = 0; x < 256; x++)
    {
        // Composited over a mid grey rather than left transparent: the surface draws this
        // strip with blending, and a strip that fades into the dialog's own background
        // shows the background rather than the alpha.
        float coverage = x / 255.0f;

        alpha[x * 4 + 0] = ToByte(color.r * coverage + 0.35f * (1.0f - coverage));
        alpha[x * 4 + 1] = ToByte(color.g * coverage + 0.35f * (1.0f - coverage));
        alpha[x * 4 + 2] = ToByte(color.b * coverage + 0.35f * (1.0f - coverage));
        alpha[x * 4 + 3] = 255;
    }

    m_pAlpha->SetPixels(alpha.data());

    m_pHue->SetMarker(m_Hue, 0.5f);
    m_pAlpha->SetMarker(m_Alpha, 0.5f);

    m_pNew->SetSolidColor(ToBytes(color.WithAlpha(1.0f)));

    MarkMatchingPreset();
}

void CColorPickerDialog::MarkMatchingPreset()
{
    ncl_math::Color current = ToBytes(CurrentColor());

    for (int i = 0; i < static_cast<int>(m_Presets.size()); i++)
    {
        const ncl_math::Color& preset = kPresetColors[i];

        bool same = std::abs(current.r - preset.r) <= kPresetMatchTolerance
            && std::abs(current.g - preset.g) <= kPresetMatchTolerance
            && std::abs(current.b - preset.b) <= kPresetMatchTolerance;

        m_Presets[i]->SetSelected(same);
    }
}

void CColorPickerDialog::UpdateField()
{
    if (m_EditingField)
    {
        return;
    }

    ncl_math::Color color = ToBytes(CurrentColor());

    m_Loading = true;

    char text[64];

    if (m_WithAlpha)
    {
        Q_snprintf(text, sizeof(text), "%d %d %d %d", color.r, color.g, color.b, color.a);
    }
    else
    {
        Q_snprintf(text, sizeof(text), "%d %d %d", color.r, color.g, color.b);
    }

    m_pField->SetText(text);

    m_Loading = false;
}

void CColorPickerDialog::Publish()
{
    ncl_math::ColorF color = CurrentColor();

    m_pListener->OnColorPicked(m_Param.c_str(), color.data());
}

void CColorPickerDialog::OnStripPicked(int id, float x, float y)
{
    if (id >= kStripPreset)
    {
        int index = std::clamp(id - kStripPreset, 0, kPresetCount - 1);

        ncl_math::ColorF picked = kPresetColors[index].ToFloat();
        float rgba[4] = {picked.r, picked.g, picked.b, m_Alpha};

        SetColor(rgba);
        m_pShade->SetMarker(m_Saturation, 1.0f - m_Value);
        Publish();
        return;
    }

    if (id == kStripShade)
    {
        m_Saturation = x;

        // The square is drawn with full value at the top, which is the way every color
        // dialog draws it, so the fraction has to be turned over here.
        m_Value = 1.0f - y;

        m_pShade->SetMarker(m_Saturation, 1.0f - m_Value);
    }
    else if (id == kStripHue)
    {
        m_Hue = x;
        RedrawShade();
    }
    else
    {
        m_Alpha = x;
    }

    RedrawBars();
    UpdateField();
    Publish();
}

void CColorPickerDialog::OnTextChanged(Panel* panel)
{
    if (m_Loading)
    {
        return;
    }

    char text[64] = {};
    m_pField->GetText(text, sizeof(text));

    // Three numbers or four, the way the game's own color dialog takes them. A missing
    // fourth leaves the alpha alone rather than reading as opaque.
    int component[4] = {0, 0, 0, static_cast<int>(ToByte(m_Alpha))};
    int read = sscanf(text, "%d %d %d %d", &component[0], &component[1], &component[2], &component[3]);

    if (read < 3)
    {
        return;
    }

    float rgba[4];
    for (int i = 0; i < 4; i++)
    {
        rgba[i] = std::clamp(component[i], 0, 255) / 255.0f;
    }

    // a typed fourth number would otherwise set an alpha the caller never asked to edit
    if (!m_WithAlpha)
        rgba[3] = m_Alpha;

    m_EditingField = true;
    SetColor(rgba);
    m_EditingField = false;

    Publish();
}

void CColorPickerDialog::Activate()
{
    MoveToCenterOfScreen();
    BaseClass::Activate();

    m_PreviousModal = input()->GetAppModalSurface();
    input()->SetAppModalSurface(GetVPanel());
}

void CColorPickerDialog::OnClose()
{
    input()->ReleaseAppModalSurface();

    if (m_PreviousModal != 0)
    {
        input()->SetAppModalSurface(m_PreviousModal);
        m_PreviousModal = 0;
    }

    BaseClass::OnClose();
}

void CColorPickerDialog::OnCommand(const char* command)
{
    if (V_stricmp(command, "defaults") == 0)
    {
        SetColor(m_Defaults.data());
        Publish();
        return;
    }

    if (V_stricmp(command, "ok") == 0)
    {
        Publish();
        Close();
        return;
    }

    if (V_stricmp(command, "cancel") == 0)
    {
        // Put back what was there: the model has been following the cursor all along, so
        // cancelling has to undo it rather than merely stop.
        m_pListener->OnColorPicked(m_Param.c_str(), m_Original.data());
        Close();
        return;
    }

    BaseClass::OnCommand(command);
}
