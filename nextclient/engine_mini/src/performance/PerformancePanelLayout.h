#pragma once

#include <array>
#include <initializer_list>
#include <span>
#include <vector>

#include "performance/PerformanceFrameHistory.h"

namespace performance
{
    inline constexpr float kPerfDefaultBackgroundAlpha = 0.2f;
    inline constexpr uint32_t kPerfMapPropertyRows = 16;

    struct PerfMapperSnapshot;
    struct PerfNetworkSnapshot;
    struct PerfPlotBin;

    enum class PerfPanelCommandKind
    {
        Rect,
        Text,
        Peak,
        Mean
    };

    struct PerfPanelCommand
    {
        PerfPanelCommandKind kind{};
        int x{};
        int y{};
        int width{};
        int height{};
        uint32_t color{};
        uint32_t text_index{};
        // Clockwise from the top-left vertex.
        std::array<int, 4> vertex_y{};
        bool area{};
        bool pixel_x{};
        uint32_t column_index{};
        uint32_t column_count{};
    };

    struct PerfPanelLabels
    {
        char title[128] = "Performance";
        char history[128] = "History";
        char cpu[128] = "CPU stages";
        char gpu[128] = "GPU passes";
        char worker[128] = "Worker";
        char batch_title[128] = "Studio batching";
    };

    class PerformancePanelLayout
    {
    private:
        struct ColumnGrid
        {
            int left{};
            int right{};
            int step{};
        };

        struct ColumnSample
        {
            double lower{};
            double upper{};
            bool available{};
            bool interpolate_next = true;
        };

        struct NetworkLayout
        {
            uint32_t first_command{};
            uint32_t last_command{};
            uint32_t first_text{};
            uint32_t last_text{};
            int x{};
            int y{};
            int width{};
            int graph_height{};
            int bottom{};
            bool standalone{};
        };

        struct BatchLayout
        {
            uint32_t first_command{};
            uint32_t last_command{};
            uint32_t first_text{};
            uint32_t last_text{};
            int x{};
            int y{};
            int width{};
            int bottom{};
            int level{};
            bool compact{};
        };

        NetworkLayout network_layout_;
        NetworkLayout mapper_layout_;
        BatchLayout batch_layout_;
        std::vector<ColumnSample> plot_samples_;
        std::vector<ColumnSample> group_samples_;
        std::array<std::vector<PerfGroupedGraphBin>, 2> frame_groups_;
        std::vector<PerfPanelCommand> retained_commands_;
        std::vector<std::array<char, 128>> retained_texts_;
        int drawable_width_{};
        int drawable_height_{};
        std::vector<PerfPanelCommand> commands_ = std::vector<PerfPanelCommand>(32768);
        std::array<std::array<char, 128>, 256> texts_{};
        PerfPanelLabels labels_;
        uint32_t command_count_{};
        uint32_t text_count_{};
        int width_{};
        int height_{};
        int content_bottom_{};
        float scale_ = 1.0f;
        float background_alpha_ = kPerfDefaultBackgroundAlpha;
        int font_tall_ = 12;

    public:
        void Build(
            const PerfSnapshot& snapshot,
            int drawable_width,
            int drawable_height,
            int mode,
            const PerfNetworkSnapshot* network = nullptr,
            const PerfMapperSnapshot* mapper = nullptr,
            int batch_level = 0,
            const PerfSnapshot* batch_snapshot = nullptr,
            const PerfGroupedGraphs* groups = nullptr,
            const PerfFrameGraphs* frames = nullptr
        );
        void RefreshNetwork(const PerfNetworkSnapshot& network);
        void RefreshMapper(const PerfMapperSnapshot& mapper);
        void RefreshBatching(const PerfSnapshot& snapshot, const PerfSnapshot* batch_snapshot);
        void SetLabels(const PerfPanelLabels& labels);
        bool SetBackgroundAlpha(float alpha);
        std::span<const PerfPanelCommand> get_commands() const;
        const char* get_text(const PerfPanelCommand& command) const;
        int get_font_tall() const;
        float get_scale() const;

    private:
        void BuildLayout(
            const PerfSnapshot& snapshot,
            int width,
            int height,
            int mode,
            const PerfNetworkSnapshot* network,
            const PerfMapperSnapshot* mapper,
            int batch_level,
            const PerfSnapshot* batch_snapshot,
            const PerfGroupedGraphs* groups,
            const PerfFrameGraphs* frames
        );
        void BuildFrameGroups(std::span<const PerfFrameGraph> frames);
        void AppendLayout(
            const PerfSnapshot& snapshot,
            int left,
            int top,
            int width,
            int height,
            int mode,
            const PerfNetworkSnapshot* network,
            const PerfMapperSnapshot* mapper,
            int batch_level,
            const PerfSnapshot* batch_snapshot,
            const PerfGroupedGraphs* groups,
            const PerfFrameGraphs* frames
        );
        void FrameGraph(std::span<const PerfFrameGraph> frames, PerfSeries series, int x, int y, int width, int height, uint32_t color);
        void GroupGraph(
            std::span<const PerfGroupedGraphBin> bins,
            bool cpu,
            int x,
            int y,
            int width,
            int height,
            ncl_perf::PerfDuration_t maximum,
            bool interpolate = true
        );
        void Columns(std::span<const ColumnSample> samples, int x, int y, int width, int height, uint32_t color, double maximum);
        void TimingTable(const PerfSnapshot& snapshot, bool cpu, int x, int& y, int width);
        ColumnGrid CalculateColumnGrid(int x, int width, uint32_t count) const;
        void ScaleCommands(uint32_t first, uint32_t last);
        void SectionHeader(const char* title, int x, int& y, int width);
        void MapperSummary(const PerfMapperSnapshot& mapper, int x, int& y, int width);
        void StudioBatching(
            const PerfSnapshot& snapshot,
            int x,
            int y,
            int width,
            bool compact,
            int batch_level,
            const PerfSnapshot* batch_snapshot
        );
        void StudioBatchingContents(
            const PerfSnapshot& snapshot,
            int x,
            int y,
            int width,
            bool compact,
            int batch_level,
            const PerfSnapshot* batch_snapshot
        );
        void TableHeader(std::initializer_list<const char*> cells, int x, int y, int width);
        void TableCells(std::initializer_list<const char*> cells, int x, int y, int width, uint32_t color);
        void Separator(int x, int& y, int width);
        void StudioBatchingRow(const char* name, uint64_t grouped, uint64_t single, uint64_t total, int x, int y, int width);
        void NetworkContents(const PerfNetworkSnapshot& network, int x, int& y, int width, int graph_height, bool standalone);
        void NetworkSummary(const PerfNetworkSnapshot& network, int x, int y, int width);
        void NetworkGraphs(const PerfNetworkSnapshot& network, int x, int& y, int width, int graph_height);
        void Plot(
            std::span<const PerfPlotBin> bins,
            int x,
            int y,
            int width,
            int height,
            uint32_t color,
            bool background = true,
            double maximum = 1,
            uint32_t column_count = kPerfGraphCapacity
        );
        void StatisticHeader(const char* name, const char* unit, int x, int y, int width, bool percentile = false);
        void StatisticRow(const char* name, std::array<double, 3> values, const char* unavailable, int x, int y, int width, uint32_t color);
        void Statistics(const char* name, const PerfSummary& summary, int x, int y, int width, uint32_t color);
        void Add(PerfPanelCommandKind kind, int x, int y, int width, int height, uint32_t color);
        void AddColumn(PerfPanelCommandKind kind, uint32_t index, uint32_t count, int x, int y, int width, int height, uint32_t color);
        void Label(int x, int y, int width, uint32_t color, const char* format, ...);
        void Graph(
            const PerfSnapshot& snapshot,
            PerfSeries series,
            int x,
            int y,
            int width,
            int height,
            uint32_t color,
            bool background = true,
            ncl_perf::PerfDuration_t scale = 0
        );

    public:
        static bool CanShowNetwork(float panel_scale, int drawable_width, int drawable_height);
        static uint32_t GraphColumnCount(float panel_scale, int drawable_width, int drawable_height, int mode = 0);
        static uint32_t NetworkColumnCount(float panel_scale, int drawable_width, int drawable_height);
    };
} // namespace performance
