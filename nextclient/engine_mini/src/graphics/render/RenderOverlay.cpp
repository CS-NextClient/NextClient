#include "engine.h"

#include <algorithm>
#include <cstdio>

#include "common/sys_dll.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/RenderOverlay.h"
#include "graphics/render/StudioIntercept.h"

namespace
{
    constexpr int kHistory = 128;

    // Panel layout, HUD pixels.
    constexpr int kPanelX = 16;
    constexpr int kPanelY = 16;
    constexpr int kBarWidth = 2;
    constexpr int kPad = 4;
    constexpr int kStripHeight = 56;
    constexpr int kStripGap = 4;
    constexpr int kTextHeight = 14;

    // Autoscale floors, so an idle scene does not zoom the graphs into noise.
    constexpr float kFrameScaleFloorMs = 8.0f;
    constexpr float kStageScaleFloorMs = 0.5f;

    // The readout is smoothed because per-frame numbers are unreadable; the graphs stay
    // raw so a single spike is still visible.
    constexpr float kEmaWeight = 0.05f;

    struct Rgba
    {
        int r, g, b, a;
    };

    constexpr Rgba kPanelColor {12, 12, 12, 160};
    constexpr Rgba kStripColor {0, 0, 0, 120};
    constexpr Rgba kFrameColor {140, 235, 140, 255};
    constexpr Rgba kStageColor {255, 170, 60, 255};

    float g_Frame[kHistory];
    float g_Stage[kHistory];
    int g_Head;
    int g_Count;

    float g_EmaFrame;
    float g_EmaStage;
    double g_PrevTime;

    void Fill(int x, int y, int w, int h, const Rgba& color)
    {
        gEngfuncs.pfnFillRGBABlend(x, y, w, h, color.r, color.g, color.b, color.a);
    }

    void DrawLabel(int x, int y, const char* text, const Rgba& color)
    {
        gEngfuncs.pfnDrawSetTextColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
        gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(text));
    }

    float HistoryMax(const float* history)
    {
        float result = 0.0f;

        for (int i = 0; i < g_Count; i++)
        {
            result = std::max(result, history[i]);
        }

        return result;
    }

    // One series as a column chart: the newest sample sits at the right edge, the
    // autoscaled maximum fills the strip.
    void DrawSeries(int x, int y, int height, const float* history, float scale, const Rgba& color)
    {
        for (int i = 0; i < g_Count; i++)
        {
            float value = history[(g_Head - g_Count + i + kHistory) % kHistory];
            int bar = static_cast<int>(std::min(value / scale, 1.0f) * height);

            if (bar < 1)
            {
                bar = 1;
            }

            int column = x + (kHistory - g_Count + i) * kBarWidth;
            Fill(column, y + height - bar, kBarWidth, bar, color);
        }
    }
}

void render::Overlay_Draw()
{
    if (g_RenderCfg.studio_graph == nullptr || g_RenderCfg.studio_graph->value == 0.0f)
    {
        g_Count = 0;
        g_PrevTime = 0.0;
        return;
    }

    StudioFrameStats stats = Studio_TakeFrameStats();

    double now = Sys_FloatTime();
    double previous = g_PrevTime;
    g_PrevTime = now;

    if (previous == 0.0)
    {
        return;
    }

    float frame_ms = static_cast<float>((now - previous) * 1000.0);

    g_Frame[g_Head] = frame_ms;
    g_Stage[g_Head] = stats.submission_ms;
    g_Head = (g_Head + 1) % kHistory;
    g_Count = std::min(g_Count + 1, kHistory);

    g_EmaFrame += (frame_ms - g_EmaFrame) * kEmaWeight;
    g_EmaStage += (stats.submission_ms - g_EmaStage) * kEmaWeight;

    int strip_width = kHistory * kBarWidth;
    int panel_width = strip_width + kPad * 2;
    int panel_height = kPad * 2 + kTextHeight * 2 + kStripHeight * 2 + kStripGap;

    Fill(kPanelX, kPanelY, panel_width, panel_height, kPanelColor);

    int x = kPanelX + kPad;
    int y = kPanelY + kPad;

    char line[128];
    bool shader_on = g_RenderCfg.studio_shader != nullptr && g_RenderCfg.studio_shader->value != 0.0f;

    V_snprintf(
        line,
        sizeof(line),
        "frame %.2f ms (%.0f fps)   %s",
        g_EmaFrame,
        g_EmaFrame > 0.0f ? 1000.0f / g_EmaFrame : 0.0f,
        shader_on ? "shader path" : "original path"
    );
    DrawLabel(x, y, line, kFrameColor);
    y += kTextHeight;

    float frame_scale = std::max(HistoryMax(g_Frame), kFrameScaleFloorMs);
    Fill(x, y, strip_width, kStripHeight, kStripColor);
    DrawSeries(x, y, kStripHeight, g_Frame, frame_scale, kFrameColor);
    y += kStripHeight + kStripGap;

    V_snprintf(
        line,
        sizeof(line),
        "studio %.3f ms   %u calls, %u by shader",
        g_EmaStage,
        stats.calls,
        stats.shader_calls
    );
    DrawLabel(x, y, line, kStageColor);
    y += kTextHeight;

    float stage_scale = std::max(HistoryMax(g_Stage), kStageScaleFloorMs);
    Fill(x, y, strip_width, kStripHeight, kStripColor);
    DrawSeries(x, y, kStripHeight, g_Stage, stage_scale, kStageColor);
}
