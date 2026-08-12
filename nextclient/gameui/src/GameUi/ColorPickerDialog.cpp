#include "ColorPickerDialog.h"

#include "Controls/PixelPanel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

    void HsvToRgb(float h, float s, float v, float out[3])
    {
        float sector = std::fmod(h, 1.0f) * 6.0f;
        int index = static_cast<int>(sector);
        float fraction = sector - index;

        float p = v * (1.0f - s);
        float q = v * (1.0f - s * fraction);
        float t = v * (1.0f - s * (1.0f - fraction));

        switch (index % 6)
        {
        case 0: out[0] = v; out[1] = t; out[2] = p; break;
        case 1: out[0] = q; out[1] = v; out[2] = p; break;
        case 2: out[0] = p; out[1] = v; out[2] = t; break;
        case 3: out[0] = p; out[1] = q; out[2] = v; break;
        case 4: out[0] = t; out[1] = p; out[2] = v; break;
        default: out[0] = v; out[1] = p; out[2] = q; break;
        }
    }

    void RgbToHsv(const float rgb[3], float& h, float& s, float& v)
    {
        float high = std::max({rgb[0], rgb[1], rgb[2]});
        float low = std::min({rgb[0], rgb[1], rgb[2]});
        float span = high - low;

        v = high;
        s = high > 0.0f ? span / high : 0.0f;

        if (span <= 0.0f)
        {
            // Grey has no hue of its own, so the one already in force is kept: recomputing
            // it as zero would snap the square back to red every time a colour is dragged
            // down to black.
            return;
        }

        if (high == rgb[0])
        {
            h = (rgb[1] - rgb[2]) / span;
        }
        else if (high == rgb[1])
        {
            h = 2.0f + (rgb[2] - rgb[0]) / span;
        }
        else
        {
            h = 4.0f + (rgb[0] - rgb[1]) / span;
        }

        h /= 6.0f;

        if (h < 0.0f)
        {
            h += 1.0f;
        }
    }

    uint8_t ToByte(float value)
    {
        return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    void Ring(int x, int y, int radius, int r, int g, int b)
    {
        constexpr int kSegments = 16;

        surface()->DrawSetColor(r, g, b, 255);

        int previous_x = x + radius;
        int previous_y = y;

        for (int i = 1; i <= kSegments; i++)
        {
            float angle = 6.2831853f * i / kSegments;
            int next_x = x + static_cast<int>(radius * std::cos(angle));
            int next_y = y + static_cast<int>(radius * std::sin(angle));

            surface()->DrawLine(previous_x, previous_y, next_x, next_y);

            previous_x = next_x;
            previous_y = next_y;
        }
    }

    // A gradient strip that reports where it was clicked and keeps reporting while the
    // button is held, and draws a ring where the current value sits. One class for all
    // three: the only thing that differs is what the caller does with the two fractions.
    class Strip : public CPixelPanel
    {
        DECLARE_CLASS_SIMPLE(Strip, CPixelPanel);

    public:
        Strip(Panel* parent, const char* name, int width, int height, IColorStripListener* listener, int id) :
            BaseClass(parent, name, width, height)
        {
            m_pListener = listener;
            m_Id = id;
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
            int y = static_cast<int>(m_MarkerY * (tall - 1));

            // Two rings, dark inside light: one alone disappears wherever the gradient
            // happens to match it, and the gradient covers every colour by construction.
            Ring(x, y, 6, 0, 0, 0);
            Ring(x, y, 5, 255, 255, 255);
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

            m_pListener->OnStripPicked(m_Id, fx, fy);
        }

        IColorStripListener* m_pListener;
        int m_Id;
        bool m_Dragging;
        float m_MarkerX;
        float m_MarkerY;
    };

    void FillSwatch(CPixelPanel* panel, const float rgb[3], float alpha)
    {
        uint8_t pixels[2 * 2 * 4];

        for (int i = 0; i < 4; i++)
        {
            pixels[i * 4 + 0] = ToByte(rgb[0]);
            pixels[i * 4 + 1] = ToByte(rgb[1]);
            pixels[i * 4 + 2] = ToByte(rgb[2]);
            pixels[i * 4 + 3] = ToByte(alpha);
        }

        panel->SetPixels(pixels);
    }
}

CColorPickerDialog::CColorPickerDialog(
    Panel* parent,
    IListener* listener,
    const char* param,
    const float rgba[4],
    const float defaults[4]
) :
    BaseClass(parent, "CColorPickerDialog")
{
    m_pListener = listener;
    m_Param = param;
    m_Loading = false;
    m_PreviousModal = 0;

    m_Hue = 0.0f;

    for (int i = 0; i < 4; i++)
    {
        m_Original[i] = rgba[i];
        m_Defaults[i] = defaults[i];
    }

    SetTitle("#GameUI_MatEdit_ColorTitle", true);
    SetBounds(0, 0, 500, 300);
    SetSizeable(false);
    SetDeleteSelfOnClose(true);

    m_pNew = new CPixelPanel(this, "New", 2, 2);
    m_pOld = new CPixelPanel(this, "Old", 2, 2);
    m_pNew->SetMouseInputEnabled(false);
    m_pOld->SetMouseInputEnabled(false);

    m_pField = new TextEntry(this, "Value");
    m_pField->AddActionSignalTarget(this);

    new Button(this, "Defaults", "#GameUI_MatEdit_UseDefaults", this, "defaults");

    m_pShade = new Strip(this, "Shade", kShadeWide, kShadeTall, this, kStripShade);
    m_pHue = new Strip(this, "Hue", 256, 1, this, kStripHue);
    m_pAlpha = new Strip(this, "Alpha", 256, 1, this, kStripAlpha);

    new Button(this, "Ok", "#GameUI_MatEdit_Ok", this, "ok");
    new Button(this, "Cancel", "#GameUI_MatEdit_Cancel", this, "cancel");

    // The hue bar never changes, so it is drawn once here rather than on every redraw.
    std::vector<uint8_t> hue(256 * 4);

    for (int x = 0; x < 256; x++)
    {
        float rgb[3];
        HsvToRgb(x / 255.0f, 1.0f, 1.0f, rgb);

        hue[x * 4 + 0] = ToByte(rgb[0]);
        hue[x * 4 + 1] = ToByte(rgb[1]);
        hue[x * 4 + 2] = ToByte(rgb[2]);
        hue[x * 4 + 3] = 255;
    }

    m_pHue->SetPixels(hue.data());

    FillSwatch(m_pOld, m_Original, 1.0f);

    SetColor(rgba);
}

void CColorPickerDialog::SetColor(const float rgba[4])
{
    RgbToHsv(rgba, m_Hue, m_Saturation, m_Value);
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

    // The colour being chosen sits directly over the colour it replaces, so the comparison
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
    m_pAlpha->SetBounds(right, top + kShadeTall + 10 + kBarTall + 6, kShadeWide, kBarTall);

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

            float rgb[3];
            HsvToRgb(m_Hue, saturation, value, rgb);

            size_t at = (static_cast<size_t>(y) * kShadeWide + x) * 4;
            pixels[at + 0] = ToByte(rgb[0]);
            pixels[at + 1] = ToByte(rgb[1]);
            pixels[at + 2] = ToByte(rgb[2]);
            pixels[at + 3] = 255;
        }
    }

    m_pShade->SetPixels(pixels.data());

    static_cast<Strip*>(m_pShade)->SetMarker(m_Saturation, 1.0f - m_Value);
}

void CColorPickerDialog::RedrawBars()
{
    float rgb[3];
    HsvToRgb(m_Hue, m_Saturation, m_Value, rgb);

    std::vector<uint8_t> alpha(256 * 4);

    for (int x = 0; x < 256; x++)
    {
        // Composited over a mid grey rather than left transparent: the surface draws this
        // strip with blending, and a strip that fades into the dialog's own background
        // shows the background rather than the alpha.
        float coverage = x / 255.0f;

        alpha[x * 4 + 0] = ToByte(rgb[0] * coverage + 0.35f * (1.0f - coverage));
        alpha[x * 4 + 1] = ToByte(rgb[1] * coverage + 0.35f * (1.0f - coverage));
        alpha[x * 4 + 2] = ToByte(rgb[2] * coverage + 0.35f * (1.0f - coverage));
        alpha[x * 4 + 3] = 255;
    }

    m_pAlpha->SetPixels(alpha.data());

    static_cast<Strip*>(m_pHue)->SetMarker(m_Hue, 0.5f);
    static_cast<Strip*>(m_pAlpha)->SetMarker(m_Alpha, 0.5f);

    FillSwatch(m_pNew, rgb, 1.0f);
}

void CColorPickerDialog::UpdateField()
{
    float rgb[3];
    HsvToRgb(m_Hue, m_Saturation, m_Value, rgb);

    m_Loading = true;

    char text[64];
    Q_snprintf(text, sizeof(text), "%d %d %d %d",
        ToByte(rgb[0]), ToByte(rgb[1]), ToByte(rgb[2]), ToByte(m_Alpha));
    m_pField->SetText(text);

    m_Loading = false;
}

void CColorPickerDialog::Publish()
{
    float rgb[3];
    HsvToRgb(m_Hue, m_Saturation, m_Value, rgb);

    float rgba[4] = {rgb[0], rgb[1], rgb[2], m_Alpha};

    m_pListener->OnColorPicked(m_Param.c_str(), rgba);
}

void CColorPickerDialog::OnStripPicked(int id, float x, float y)
{
    if (id == kStripShade)
    {
        m_Saturation = x;

        // The square is drawn with full value at the top, which is the way every colour
        // dialog draws it, so the fraction has to be turned over here.
        m_Value = 1.0f - y;
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

    if (id == kStripShade)
    {
        static_cast<Strip*>(m_pShade)->SetMarker(m_Saturation, 1.0f - m_Value);
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

    // Three numbers or four, the way the game's own colour dialog takes them. A missing
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

    SetColor(rgba);
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
        SetColor(m_Defaults);
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
        m_pListener->OnColorPicked(m_Param.c_str(), m_Original);
        Close();
        return;
    }

    BaseClass::OnCommand(command);
}
