#include "RadarDebug.h"

#include <algorithm>
#include <span>
#include <cstdint>
#include <cstdio>

#include "main.h"

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>

#include "RadarDraw.h"
#include "RadarView.h"

namespace
{
    constexpr int kHistory = 128;

    // Panel layout, HUD pixels.
    constexpr float kPanelGap = 4.0f;
    constexpr float kPad = 4.0f;
    constexpr float kPanelWidthScale = 1.4f;
    constexpr float kTimeStripH = 84.0f;
    constexpr float kBatchStripH = 28.0f;
    constexpr float kStripGap = 4.0f;

    // Autoscale floors so a quiet radar does not zoom the graphs into noise.
    constexpr float kTimeScaleFloorMs = 0.5f;
    constexpr float kBatchScaleFloor = 8.0f;

    constexpr float kEmaWeight = 0.05f;

    constexpr ncl_math::Color kPanelColor{12, 12, 12, 160};
    constexpr ncl_math::Color kStripColor{0, 0, 0, 120};
    constexpr ncl_math::Color kTotalColor{140, 235, 140};
    constexpr ncl_math::Color kBlurColor{255, 170, 60};
    constexpr ncl_math::Color kTracesColor{205, 145, 255};
    constexpr ncl_math::Color kTextColor{255, 235, 120};
    constexpr ncl_math::Color kBatchColor{130, 190, 255};

    goldsrc::cvar_t* g_Enabled;

    float g_Total[kHistory];
    float g_Blur[kHistory];
    float g_Traces[kHistory];
    float g_Text[kHistory];
    float g_Batches[kHistory];
    int g_Head;
    int g_Count;

    float g_EmaTotal;
    float g_EmaBlur;
    float g_EmaTraces;
    float g_EmaText;
    float g_EmaBatches;

    float ComputeHistoryMax(std::span<const float, kHistory> hist)
    {
        float max_v = 0.0f;

        for (int i = 0; i < g_Count; i++)
        {
            max_v = std::max(max_v, hist[i]);
        }

        return max_v;
    }

    void Fill(float x, float y, float w, float h, ncl_math::Color c)
    {
        gEngfuncs.pfnFillRGBABlend((int)x, (int)y, (int)w, (int)h, c.r, c.g, c.b, c.a);
    }

    int DrawLabel(int x, int y, const char* text, ncl_math::Color color)
    {
        ncl_math::ColorF c = color.ToFloat();
        gEngfuncs.pfnDrawSetTextColor(c.r, c.g, c.b);
        return gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(text));
    }

    void DrawSeries(float x, float y, float w, float h, std::span<const float, kHistory> hist, float vmax, ncl_math::Color color)
    {
        if (g_Count < 2)
        {
            return;
        }

        ncl_math::Vector2 pts[kHistory];
        uint8_t alpha[kHistory];
        float step = w / (float)(kHistory - 1);

        for (int i = 0; i < g_Count; i++)
        {
            float v = hist[(g_Head - g_Count + i + kHistory) % kHistory];
            float t = std::min(v / vmax, 1.0f);
            pts[i] = {x + (float)(kHistory - g_Count + i) * step, y + h - t * h};
            alpha[i] = 255;
        }

        RadarDraw_DrawPolyline({pts, (size_t)g_Count}, {alpha, (size_t)g_Count}, color, 1.5f);
    }
} // namespace

void RadarDebug_Init()
{
    g_Enabled = gEngfuncs.pfnRegisterVariable("hud_radar_debug", "0", 0);
}

bool RadarDebug_IsEnabled()
{
    return g_Enabled != nullptr && g_Enabled->value != 0.0f;
}

void RadarDebug_DrawFrame(const RadarBox& box, float total_ms, float blur_ms, float traces_ms, float text_ms, int batches)
{
    g_Total[g_Head] = total_ms;
    g_Blur[g_Head] = blur_ms;
    g_Traces[g_Head] = traces_ms;
    g_Text[g_Head] = text_ms;
    g_Batches[g_Head] = (float)batches;
    g_Head = (g_Head + 1) % kHistory;

    if (g_Count == 0)
    {
        g_EmaTotal = total_ms;
        g_EmaBlur = blur_ms;
        g_EmaTraces = traces_ms;
        g_EmaText = text_ms;
        g_EmaBatches = (float)batches;
    }
    else
    {
        g_EmaTotal = ncl_math::Lerp(g_EmaTotal, total_ms, kEmaWeight);
        g_EmaBlur = ncl_math::Lerp(g_EmaBlur, blur_ms, kEmaWeight);
        g_EmaTraces = ncl_math::Lerp(g_EmaTraces, traces_ms, kEmaWeight);
        g_EmaText = ncl_math::Lerp(g_EmaText, text_ms, kEmaWeight);
        g_EmaBatches = ncl_math::Lerp(g_EmaBatches, (float)batches, kEmaWeight);
    }

    g_Count = std::min(g_Count + 1, kHistory);

    int text_w = 0, text_h = 0;
    gEngfuncs.pfnDrawConsoleStringLen("x", &text_w, &text_h);

    float x = box.x;
    float y = box.y + box.size + kPanelGap;
    float w = box.size * kPanelWidthScale;
    // Measured and wrapped before the panel is filled: its height follows the row count.
    struct Readout
    {
        char text[32];
        ncl_math::Color color;
        int wide;
    };

    Readout readouts[] = {
        {"", kTotalColor, 0},
        {"", kBlurColor, 0},
        {"", kTracesColor, 0},
        {"", kTextColor, 0},
        {"", kBatchColor, 0},
    };

    std::snprintf(readouts[0].text, sizeof(readouts[0].text), "radar %.2f ms", g_EmaTotal);
    std::snprintf(readouts[1].text, sizeof(readouts[1].text), "blur %.2f ms", g_EmaBlur);
    std::snprintf(readouts[2].text, sizeof(readouts[2].text), "traces %.2f ms", g_EmaTraces);
    std::snprintf(readouts[3].text, sizeof(readouts[3].text), "text %.2f ms", g_EmaText);
    std::snprintf(readouts[4].text, sizeof(readouts[4].text), "batches %d", (int)(g_EmaBatches + 0.5f));

    int gap_w = 0, ignored_h = 0;
    gEngfuncs.pfnDrawConsoleStringLen("  ", &gap_w, &ignored_h);

    float inner_w = w - kPad * 2.0f;
    int rows = 1;
    float row_x = 0.0f;

    for (Readout& readout : readouts)
    {
        int h = 0;
        gEngfuncs.pfnDrawConsoleStringLen(readout.text, &readout.wide, &h);

        if (row_x > 0.0f && row_x + (float)readout.wide > inner_w)
        {
            rows++;
            row_x = 0.0f;
        }

        row_x += (float)(readout.wide + gap_w);
    }

    float labels_h = (float)(text_h * rows);
    float panel_h = kPad + labels_h + kStripGap + kTimeStripH + kStripGap + kBatchStripH + kPad;

    Fill(x, y, w, panel_h, kPanelColor);

    int tx = (int)(x + kPad);
    int ty = (int)(y + kPad);

    for (const Readout& readout : readouts)
    {
        if (tx > (int)(x + kPad) && (float)tx + (float)readout.wide > x + kPad + inner_w)
        {
            tx = (int)(x + kPad);
            ty += text_h;
        }

        DrawLabel(tx, ty, readout.text, readout.color);
        tx += readout.wide + gap_w;
    }

    float gx = x + kPad;
    float gw = w - kPad * 2.0f;
    float gy = y + kPad + labels_h + kStripGap;

    Fill(gx, gy, gw, kTimeStripH, kStripColor);
    float time_max = std::max(ComputeHistoryMax(g_Total), kTimeScaleFloorMs);
    DrawSeries(gx, gy, gw, kTimeStripH, g_Total, time_max, kTotalColor);
    DrawSeries(gx, gy, gw, kTimeStripH, g_Blur, time_max, kBlurColor);
    DrawSeries(gx, gy, gw, kTimeStripH, g_Traces, time_max, kTracesColor);
    DrawSeries(gx, gy, gw, kTimeStripH, g_Text, time_max, kTextColor);

    float by = gy + kTimeStripH + kStripGap;
    Fill(gx, by, gw, kBatchStripH, kStripColor);
    float batch_max = std::max(ComputeHistoryMax(g_Batches), kBatchScaleFloor);
    DrawSeries(gx, by, gw, kBatchStripH, g_Batches, batch_max, kBatchColor);

    RadarDraw_FlushPrims();
}
