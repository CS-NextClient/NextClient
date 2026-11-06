#pragma once

#include <array>
#include <string>
#include <vector>

#include <next_engine_mini/TextRenderInterface.h>

#include "performance/PerformancePanelLayout.h"

namespace vgui2
{
    class ILocalize;
}

namespace performance
{
    using ConsoleText_t = int (*)(int, int, char*);
    struct PerfMapRenderView
    {
        std::array<float, 16> modelview{};
        std::array<float, 16> projection{};
        std::array<int, 4> viewport{};
        std::array<double, 2> depth_range{};
        int depth_function{};
        int draw_framebuffer{};
    };

    namespace panel
    {
        void DrawMapOutline(const PerfMapperSnapshot& mapper, const PerfMapRenderView& view);
    } // namespace panel

    class PerformancePanel
    {
    private:
        struct LayoutKey
        {
            int width{};
            int height{};
            int mode{};
            int batch_level{};
            bool network{};
            bool mapper{};
            int mapper_level{};
            bool mapper_loaded{};
            uint32_t mapper_properties{};
            float scale{};
            uint32_t generation{};
            uint32_t map{};
            uint32_t context{};
            uint32_t frame_columns{};
            ncl_perf::PerfTimestamp_t frame_time{};
            bool frame_graphs{};

            bool operator==(const LayoutKey&) const = default;
        };

        struct GlyphQuad
        {
            TextGlyph glyph;
            int x{};
            int y{};
        };

        struct TextRun
        {
            std::vector<GlyphQuad> glyphs;
            std::string text;
            int x{};
            int y{};
            bool ready{};
        };

        PerformancePanelLayout layout_;
        ncl_perf::PerfClockInterface& clock_;
        int* current_texture_{};
        vgui2::ILocalize* localization_{};
        ConsoleText_t console_text_{};
#ifdef _WIN32
        TextRenderInterface& text_;
#endif
        LayoutKey layout_key_;
        ncl_perf::PerfTimestamp_t layout_time_{};
        uint64_t network_revision_{};
        uint64_t mapper_revision_{};
        uint32_t network_columns_{};
        PerfFrameId_t batch_frame_{};
        uint32_t batch_generation_{};
        bool layout_valid_{};
        int font_{};
        int font_tall_{};
        std::vector<TextRun> text_runs_;
        uint32_t text_generation_{};
        bool text_ready_{};

    public:
        explicit PerformancePanel(
#ifdef _WIN32
            TextRenderInterface& text,
#endif
            ncl_perf::PerfClockInterface& clock,
            int* current_texture,
            vgui2::ILocalize* localization = nullptr,
            ConsoleText_t console_text = nullptr
        );
        void BindPresentation(vgui2::ILocalize* localization, ConsoleText_t console_text);
        void Invalidate();
        void SetBackgroundAlpha(float alpha);
        void Draw(
            const PerfSnapshot& snapshot,
            int width,
            int height,
            int mode,
            const PerfNetworkSnapshot* network = nullptr,
            const PerfMapperSnapshot* mapper = nullptr,
            int batch_level = 0,
            const PerfSnapshot* batch_snapshot = nullptr,
            const PerfGroupedGraphs* groups = nullptr,
            const PerfFrameGraphs* frames = nullptr
        );

    private:
#ifdef _WIN32
        void PrepareText();
        void DrawCachedText(const PerfPanelCommand& command, int drawable_height);
#endif
    };
} // namespace performance
