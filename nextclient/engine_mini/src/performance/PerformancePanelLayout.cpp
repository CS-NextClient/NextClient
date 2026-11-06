#include "performance/PerformancePanelLayout.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>

#include <strtools.h>

#include "performance/performance_stages.h"
#include "performance/PerformanceMetrics.h"
#include "performance/PerformanceNetwork.h"
#include "performance/PerformanceMapper.h"

namespace performance
{
    namespace
    {
        constexpr int kLineHeight = 14;
        constexpr float kBaseScale = 1.15f;
        constexpr float kReferenceHeight = 1200.0f;
        constexpr int kPadding = 6;
        constexpr uint32_t kTextColor = 0xe4e4e4ff;
        constexpr uint32_t kBackground = 0x11131d00;
        constexpr uint32_t kPlotBackground = 0x06091120;
        constexpr uint32_t kAxisColor = 0xc0c6d270;
        constexpr uint32_t kGridColor = 0xc0c6d228;
        constexpr int kSectionGap = 6;
        constexpr uint32_t kSectionBackground = 0x9cbfdf24;
        constexpr uint32_t kSectionTitleBackground = 0x9cbfdf40;
        constexpr uint32_t kSecondaryText = 0xb8c4d8ff;
        constexpr uint32_t kSectionAccent = 0x9ccfffe0;
        constexpr uint32_t kSectionText = 0xe6f3ffff;
        constexpr int kGraphAxisWidth = 60;
        constexpr uint32_t kLatencyColor = 0xd8cf7aff;
        constexpr uint32_t kIncomingColor = 0x65cfffff;
        constexpr uint32_t kOutgoingColor = 0x86de86ff;
        constexpr uint32_t kLossColor = 0xff6464ff;
        constexpr uint32_t kChokeColor = 0xffae54ff;
        constexpr int kNetworkPlotOverhead = 88 + 2 * kSectionGap;
        constexpr uint32_t kOverviewSeriesCount = performance::CountOverviewSeries();
        constexpr int kOverviewLegendHeight = 12 * kLineHeight;
        constexpr int kOverviewMinimumGraphHeight = 192;
        constexpr int kOverviewGraphHeight = 490;
        constexpr int kIndividualMinimumGraphHeight = 32;
        constexpr int kStudioGaugeLines = int(performance::kGauges.size());
        constexpr int kStudioSummaryLines = 7 + kStudioGaugeLines;
        constexpr int kStudioCompactLines = 8 + kStudioGaugeLines;
        constexpr int kStudioDetailLines = 17 + kStudioGaugeLines;
        constexpr int kStudioDetailWidth = 440;
        constexpr PerfNetworkSeries kNetworkSummarySeries[] = {PerfNetworkSeries::Latency, PerfNetworkSeries::Loss, PerfNetworkSeries::Choke};
        constexpr const char* kNetworkSummaryNames[] = {"Latency", "loss", "choke"};
        constexpr const char* kNetworkSummaryUnits[] = {"ms", "%", "%"};
        constexpr uint32_t kNetworkSummaryColors[] = {kLatencyColor, kLossColor, kChokeColor};

        performance::SeriesPlacement TimingPlacement(bool cpu)
        {
            return cpu ? performance::SeriesPlacement::Cpu : performance::SeriesPlacement::Gpu;
        }

        std::array<char, ncl_perf::kPerfNameCapacity> TimingLabel(const performance::SeriesDescriptor& series)
        {
            std::array<char, ncl_perf::kPerfNameCapacity> label{};
            std::snprintf(label.data(), label.size(), "%*s%s", int(series.presentation.indentation * 2), "", series.presentation.label);

            return label;
        }

        int StudioLines(int batch_level, bool compact)
        {
            if (batch_level < 2)
            {
                return kStudioSummaryLines;
            }

            return compact ? kStudioCompactLines : kStudioDetailLines;
        }

        int PerformanceBodyHeight(int mode)
        {
            return mode == 1   ? 10 * kLineHeight
                   : mode == 2 ? kOverviewLegendHeight + kOverviewGraphHeight + 4
                   : mode == 3 ? int(kOverviewSeriesCount) * (2 * kLineHeight + 65 + 4) + 2 * kLineHeight
                               : 0;
        }

        int MapperHeight(const PerfMapperSnapshot* mapper)
        {
            if (!mapper)
            {
                return 0;
            }

            int properties = mapper->level >= 2 ? int(std::min(size_t(kPerfMapPropertyRows), mapper->properties.size())) + 1 : 0;

            return kLineHeight + (mapper->map_loaded ? (6 + 2 * int(mapper->level >= 2) + properties) * kLineHeight : kLineHeight);
        }

        int SectionHeight(int mode, bool network, const PerfMapperSnapshot* mapper, int batch_level)
        {
            int body = mode      ? 3 * kLineHeight + PerformanceBodyHeight(mode) + kSectionGap
                       : network ? 5 * kLineHeight + kSectionGap + 2 * 65 + kNetworkPlotOverhead
                       : mapper  ? MapperHeight(mapper)
                                 : StudioLines(batch_level, false) * kLineHeight;

            return 16 + 2 * kPadding + body;
        }

        int MinimumSectionHeight(int mode, bool network, const PerfMapperSnapshot* mapper, int batch_level)
        {
            if (mode)
            {
                return 16 + 2 * kPadding + 3 * kLineHeight + kSectionGap;
            }

            if (network)
            {
                return SectionHeight(0, true, nullptr, 0) - 2 * (65 - 24);
            }

            if (mapper)
            {
                int properties =
                    mapper->map_loaded && mapper->level >= 2 ? int(std::min(size_t(kPerfMapPropertyRows), mapper->properties.size())) : 0;

                return SectionHeight(0, false, mapper, 0) - properties * kLineHeight;
            }

            return 16 + 2 * kPadding + StudioLines(batch_level, true) * kLineHeight;
        }

        unsigned long long Counter(const PerfSnapshot& snapshot, PerfCounterId counter)
        {
            return static_cast<unsigned long long>(snapshot.counters[uint32_t(counter)]);
        }

        const ncl_perf::PerfCpuSample* CpuSample(const PerfSnapshot& snapshot, ncl_perf::PerfScopeId_t id)
        {
            for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
            {
                if (snapshot.cpu_tree[index].scope_id == id)
                {
                    return &snapshot.cpu_tree[index];
                }
            }

            return nullptr;
        }

        float Scale(float panel_scale, int drawable_height)
        {
            float normalized = panel_scale > 0 && std::isfinite(panel_scale)
                                   ? std::clamp(panel_scale, 0.5f, 3.0f)
                                   : std::clamp(float(drawable_height) / kReferenceHeight, 1.0f, 2.0f);

            return normalized * kBaseScale;
        }

        void NetworkValue(const PerfNetworkValue& value, char (&text)[24])
        {
            if (value.state == ncl_perf::PerfSampleState::Measured)
            {
                std::snprintf(text, sizeof(text), value.value >= 1000000 ? "%.3g" : "%.1f", value.value);
            }
            else
            {
                std::snprintf(text, sizeof(text), "-");
            }
        }

        void Value(const PerfSummary& summary, performance::MetricUnit unit, char (&text)[48])
        {
            if (summary.state == ncl_perf::PerfSampleState::Measured)
            {
                std::snprintf(text, sizeof(text), "%.2f %s", double(summary.mean_ns) / 1e6, performance::UnitLabel(unit));
            }
            else
            {
                std::snprintf(text, sizeof(text), "%s", stages::StateName(summary.state));
            }
        }

    } // namespace

    bool PerformancePanelLayout::CanShowNetwork(float panel_scale, int drawable_width, int drawable_height)
    {
        float scale = Scale(panel_scale, drawable_height);
        int minimum_height = 2 * (8 + kPadding) + 5 * kLineHeight + kSectionGap + kNetworkPlotOverhead + 48;

        return drawable_width / scale >= 400 && drawable_height / scale >= minimum_height;
    }

    uint32_t PerformancePanelLayout::NetworkColumnCount(float panel_scale, int drawable_width, int drawable_height)
    {
        return CanShowNetwork(panel_scale, drawable_width, drawable_height) ? GraphColumnCount(panel_scale, drawable_width, drawable_height)
                                                                            : 0;
    }

    uint32_t PerformancePanelLayout::GraphColumnCount(float panel_scale, int drawable_width, int drawable_height, int mode)
    {
        if (drawable_width <= 0 || drawable_height <= 0)
        {
            return 0;
        }

        float scale = Scale(panel_scale, drawable_height);
        int logical_width = int(drawable_width / scale);
        int panel_width = std::min(mode == 1 ? 400 : 440, logical_width - 16);
        int plot_width = panel_width - 2 * kPadding - kGraphAxisWidth;

        if (plot_width <= 0)
        {
            return 0;
        }

        int pixels = int(std::round(plot_width * scale));

        return uint32_t(std::clamp(pixels, 0, int(kPerfHistoryCapacity)));
    }

    void PerformancePanelLayout::Build(
        const PerfSnapshot& snapshot,
        int drawable_width,
        int drawable_height,
        int mode,
        const PerfNetworkSnapshot* network,
        const PerfMapperSnapshot* mapper,
        int batch_level,
        const PerfSnapshot* batch_snapshot,
        const PerfGroupedGraphs* groups,
        const PerfFrameGraphs* frames
    )
    {
        drawable_width_ = drawable_width;
        drawable_height_ = drawable_height;
        scale_ = Scale(snapshot.panel_scale, drawable_height);
        font_tall_ = std::max(6, int(std::round(12 * scale_)));
        bool show_network = network && network->enabled && CanShowNetwork(snapshot.panel_scale, drawable_width, drawable_height);
        BuildLayout(
            snapshot,
            int(drawable_width / scale_),
            int(drawable_height / scale_),
            mode,
            show_network ? network : nullptr,
            mapper && mapper->enabled ? mapper : nullptr,
            batch_level,
            batch_snapshot,
            groups,
            frames
        );
        ScaleCommands(0, command_count_);
    }

    PerformancePanelLayout::ColumnGrid PerformancePanelLayout::CalculateColumnGrid(int x, int width, uint32_t count) const
    {
        int left = int(std::round(x * scale_));
        int right = std::min(drawable_width_, left + int(std::round(std::min(width, width_ - x) * scale_)));
        int step = std::max(1, (right - left) / int(count));

        return {std::max(left, right - int(count) * step), right, step};
    }

    void PerformancePanelLayout::ScaleCommands(uint32_t first, uint32_t last)
    {
        for (uint32_t index = first; index < last; ++index)
        {
            PerfPanelCommand& command = commands_[index];
            int right = command.pixel_x ? command.x + command.width : int(std::round((command.x + command.width) * scale_));

            if (command.column_count)
            {
                ColumnGrid grid = CalculateColumnGrid(command.x, command.width, command.column_count);
                command.x = grid.right - int(command.column_count - command.column_index) * grid.step;
                command.width = grid.step;
            }
            else if (!command.pixel_x)
            {
                command.x = int(std::round(command.x * scale_));
                command.width = std::min(drawable_width_ - command.x, std::max(1, int(std::round(command.width * scale_))));
            }

            command.y = int(std::round(command.y * scale_));
            command.height = std::min(drawable_height_ - command.y, std::max(1, int(std::round(command.height * scale_))));

            if (command.area)
            {
                if (!command.column_count && !command.pixel_x)
                {
                    command.width = std::min(drawable_width_ - command.x, std::max(1, right - command.x));
                }

                for (int& vertex : command.vertex_y)
                {
                    vertex = std::clamp(int(std::round(vertex * scale_)), 0, drawable_height_);
                }

                command.y = *std::min_element(command.vertex_y.begin(), command.vertex_y.end());
                command.height = *std::max_element(command.vertex_y.begin(), command.vertex_y.end()) - command.y;
            }
            else
            {
                command.vertex_y = {command.y, command.y, command.y + command.height, command.y + command.height};
            }
        }
    }

    void PerformancePanelLayout::BuildLayout(
        const PerfSnapshot& snapshot,
        int drawable_width,
        int drawable_height,
        int mode,
        const PerfNetworkSnapshot* network,
        const PerfMapperSnapshot* mapper,
        int batch_level,
        const PerfSnapshot* batch_snapshot,
        const PerfGroupedGraphs* groups,
        const PerfFrameGraphs* frames
    )
    {
        command_count_ = 0;
        text_count_ = 0;
        batch_layout_ = {};
        network_layout_ = {};
        mapper_layout_ = {};
        width_ = std::max(drawable_width, 0);
        height_ = std::max(drawable_height, 0);

        if ((mode <= 0 && !network && !mapper && !batch_level) || width_ < 96 || height_ < 32)
        {
            return;
        }

        struct Section
        {
            int mode{};
            const PerfNetworkSnapshot* network{};
            const PerfMapperSnapshot* mapper{};
            int batch_level{};
            int height{};
            int column{};
            int top{};
        };

        std::array<Section, 4> sections{};
        int section_count = 0;

        if (mode)
        {
            sections[section_count++] = {mode};
        }

        if (network)
        {
            sections[section_count++] = {0, network};
        }

        if (batch_level)
        {
            sections[section_count++] = {0, nullptr, nullptr, batch_level};
        }

        if (mapper)
        {
            sections[section_count++] = {0, nullptr, mapper};
        }

        for (int index = 0; index < section_count; ++index)
        {
            Section& section = sections[index];
            section.height = std::min(SectionHeight(section.mode, section.network != nullptr, section.mapper, section.batch_level), height_);
        }

        int column_width = std::min(kStudioDetailWidth, width_ - 16);
        int available_columns = std::clamp((width_ - 16 + kSectionGap) / (column_width + kSectionGap), 1, int(sections.size()));
        int column = 0;
        int used_height = 0;

        for (int index = 0; index < section_count; ++index)
        {
            Section& section = sections[index];

            if (used_height && used_height + section.height > height_ && column + 1 < available_columns)
            {
                ++column;
                used_height = 0;
            }

            if (column + 1 == available_columns)
            {
                int remaining_count = section_count - index;
                int budget = std::max(0, height_ - used_height - (remaining_count - 1) * kSectionGap);
                int header_height = std::min(16 + 2 * kPadding + kLineHeight, budget / remaining_count);
                std::array<int, 4> heights{};
                int minimum_height = 0;

                for (int remaining = index; remaining < section_count; ++remaining)
                {
                    const Section& next = sections[remaining];
                    heights[remaining] = MinimumSectionHeight(next.mode, next.network != nullptr, next.mapper, next.batch_level);
                    minimum_height += heights[remaining];
                }

                for (int remaining = index; remaining < section_count; ++remaining)
                {
                    if (minimum_height > budget)
                    {
                        heights[remaining] = header_height;
                    }
                }

                for (int remaining = index; remaining < section_count; ++remaining)
                {
                    budget -= heights[remaining];
                }

                for (bool network_first : {true, false})
                {
                    for (int remaining = index; remaining < section_count; ++remaining)
                    {
                        if ((sections[remaining].network != nullptr) != network_first)
                        {
                            continue;
                        }

                        int extra = std::min(budget, sections[remaining].height - heights[remaining]);
                        heights[remaining] += extra;
                        budget -= extra;
                    }
                }

                for (int remaining = index; remaining < section_count; ++remaining)
                {
                    sections[remaining].height = heights[remaining];
                    sections[remaining].column = column;
                    sections[remaining].top = used_height;
                    used_height += heights[remaining] + kSectionGap;
                }

                break;
            }

            section.column = column;
            section.top = used_height;
            used_height += section.height + kSectionGap;
        }

        for (int index = 0; index < section_count; ++index)
        {
            const Section& section = sections[index];
            AppendLayout(
                snapshot,
                width_ - 16 - column_width - section.column * (column_width + kSectionGap),
                section.top,
                column_width + 16,
                section.height,
                section.mode,
                section.network,
                section.mapper,
                section.batch_level,
                batch_snapshot,
                groups,
                frames
            );
        }
    }

    void PerformancePanelLayout::AppendLayout(
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
    )
    {
        if (width < 96 || height < 32)
        {
            return;
        }

        uint32_t backing = kBackground | uint32_t(std::round(background_alpha_ * 255.0f));

        if (!mode && batch_level && !network && !mapper)
        {
            bool compact = height < StudioLines(batch_level, false) * kLineHeight + 2 * kPadding + 16;
            int panel_width = std::min(kStudioDetailWidth, width - 16);
            int panel_height = std::min(StudioLines(batch_level, compact) * kLineHeight + 2 * kPadding, height - 16);
            int x = left + width - panel_width - 8;
            content_bottom_ = top + 8 + panel_height - kPadding;
            Add(PerfPanelCommandKind::Rect, x, top + 8, panel_width, panel_height, backing);
            StudioBatching(snapshot, x + kPadding, top + 8 + kPadding, panel_width - 2 * kPadding, compact, batch_level, batch_snapshot);
            return;
        }

        int desired_height = SectionHeight(mode, network != nullptr, mapper, batch_level) - 16;
        int panel_width = std::min(mode == 1 ? 400 : 440, width - 16);
        int panel_height = std::min(desired_height, height - 16);
        int x = left + width - panel_width - 8 + kPadding;
        int y = top + 8 + kPadding;
        int content_width = panel_width - 2 * kPadding;
        content_bottom_ = top + 8 + panel_height - kPadding;
        Add(PerfPanelCommandKind::Rect, x - kPadding, top + 8, panel_width, panel_height, backing);

        int network_height = 0;

        if (network)
        {
            int graph_budget = panel_height - 2 * kPadding - 5 * kLineHeight - kSectionGap - kNetworkPlotOverhead;

            if (graph_budget >= 48)
            {
                network_height = std::min(65, graph_budget / 2);
            }
        }

        if (mode)
        {
            const PerfSummary& intervals = snapshot.summaries[0];
            char title[160];
            std::snprintf(title, sizeof(title), "%s | FPS %.0f", labels_.title, intervals.fps);
            SectionHeader(title, x, y, content_width);

            if (frames)
            {
                Label(x, y, content_width, kSecondaryText, "%s | last %zu frames", labels_.history, frames->frames.size());
            }
            else
            {
                Label(x, y, content_width, kSecondaryText, "%s %.2f s", labels_.history, double(snapshot.coverage_ns) / 1e9);
            }

            y += kLineHeight;

            if (mode == 1 || mode == 2)
            {
                PerfGroupedGraphs fallback;

                if (frames)
                {
                    BuildFrameGroups(frames->frames);
                }

                if (mode == 2 && !groups && !frames)
                {
                    for (const performance::SeriesDescriptor& series : performance::kSeries)
                    {
                        if (!series.presentation.fallback)
                        {
                            continue;
                        }

                        for (uint32_t index = 0; index < kPerfGraphCapacity; ++index)
                        {
                            const PerfGraphBin& value = snapshot.graphs[uint32_t(series.id)][index];
                            uint32_t target = index * kPerfGroupedGraphCapacity / kPerfGraphCapacity;
                            PerfGroupedGraphBin& bin = series.presentation.placement == performance::SeriesPlacement::Gpu
                                                           ? fallback.gpu[target]
                                                           : fallback.cpu[target];
                            PerfGraphBin& total = series.presentation.role == performance::GroupRole::Interval ? bin.interval : bin.total;
                            total.maximum_ns = std::max(total.maximum_ns, value.maximum_ns);
                            total.mean_ns += value.mean_ns * value.count;
                            total.count += value.count;
                        }
                    }

                    for (auto* bins : {&fallback.cpu, &fallback.gpu})
                    {
                        for (PerfGroupedGraphBin& bin : *bins)
                        {
                            for (PerfGraphBin* value : {&bin.interval, &bin.total})
                            {
                                if (value->count)
                                {
                                    value->mean_ns /= value->count;
                                }
                            }
                        }
                    }

                    groups = &fallback;
                }

                std::span<const PerfGroupedGraphBin> cpu_bins;
                std::span<const PerfGroupedGraphBin> gpu_bins;

                if (frames)
                {
                    cpu_bins = frame_groups_[0];
                    gpu_bins = frame_groups_[1];
                }
                else if (groups)
                {
                    cpu_bins = groups->cpu;
                    gpu_bins = groups->gpu;
                }

                ncl_perf::PerfDuration_t maximum = 1000000;

                for (std::span<const PerfGroupedGraphBin> bins : {cpu_bins, gpu_bins})
                {
                    for (const PerfGroupedGraphBin& bin : bins)
                    {
                        maximum = std::max({maximum, bin.interval.maximum_ns, bin.total.maximum_ns});
                    }
                }

                int graph_budget = content_bottom_ - y - kOverviewLegendHeight - kLineHeight - 4 - kSectionGap;
                int graph_height = mode == 2 ? std::max(0, std::min(kOverviewGraphHeight, graph_budget) / 2) : 0;

                for (bool cpu : {true, false})
                {
                    TimingTable(snapshot, cpu, x, y, content_width);

                    if (mode == 2 && graph_height >= kIndividualMinimumGraphHeight)
                    {
                        Label(
                            x,
                            y,
                            content_width,
                            kSecondaryText,
                            cpu ? "Overlaid totals; stacked measured parts" : "Overlaid render and parts (async)"
                        );
                        y += kLineHeight;
                        GroupGraph(cpu ? cpu_bins : gpu_bins, cpu, x, y, content_width, graph_height, maximum, frames == nullptr);
                        y += graph_height + 2;
                    }
                }
            }
            else if (mode == 3)
            {
                int graph_budget = content_bottom_ - y - int(kOverviewSeriesCount) * (2 * kLineHeight + 4) - 3 * kLineHeight - kSectionGap;
                int graph_height = std::clamp(graph_budget / int(kOverviewSeriesCount), kIndividualMinimumGraphHeight, 65);

                for (bool cpu : {true, false})
                {
                    SectionHeader(cpu ? "CPU" : "GPU", x, y, content_width);
                    performance::SeriesPlacement placement = TimingPlacement(cpu);
                    uint32_t count = performance::CountSeries(placement);

                    for (uint32_t index = 0; index < count; ++index)
                    {
                        if (y + 2 * kLineHeight + graph_height + 4 + kLineHeight + kSectionGap + (cpu ? kLineHeight : 0) > content_bottom_)
                        {
                            break;
                        }

                        const performance::SeriesDescriptor& descriptor = *performance::OrderedSeries(placement, index);
                        PerfSeries series = descriptor.id;
                        std::array<char, ncl_perf::kPerfNameCapacity> name = TimingLabel(descriptor);
                        StatisticHeader(name.data(), performance::UnitLabel(descriptor.unit), x, y, content_width, true);
                        y += kLineHeight;
                        Statistics("", snapshot.summaries[uint32_t(series)], x, y, content_width, descriptor.presentation.color);
                        y += kLineHeight;

                        if (frames)
                        {
                            FrameGraph(frames->frames, series, x, y, content_width, graph_height, descriptor.presentation.color);
                        }
                        else
                        {
                            Graph(snapshot, series, x, y, content_width, graph_height, descriptor.presentation.color);
                        }

                        y += graph_height + 4;
                    }
                }
            }

            Separator(x, y, content_width);

            for (uint32_t order = 0; order < performance::CountSeries(performance::SeriesPlacement::Footer); ++order)
            {
                const performance::SeriesDescriptor& series = *performance::OrderedSeries(performance::SeriesPlacement::Footer, order);
                char value[48];
                Value(snapshot.summaries[uint32_t(series.id)], series.unit, value);
                Label(
                    x,
                    y,
                    content_width,
                    series.presentation.color,
                    "%s %s | GPU age %u%s",
                    series.presentation.label,
                    value,
                    snapshot.gpu_age,
                    snapshot.capacity_limited ? " | limited" : ""
                );
                y += kLineHeight;
            }
        }

        if (network)
        {
            network_layout_ = {command_count_, 0, text_count_, 0, x, y, content_width, network_height, content_bottom_, !mode};
            NetworkContents(*network, x, y, content_width, network_height, !mode);
            network_layout_.last_command = command_count_;
            network_layout_.last_text = text_count_;
        }

        if (mapper)
        {
            SectionHeader("Map inspection", x, y, content_width);
            mapper_layout_ = {command_count_, 0, text_count_, 0, x, y, content_width, 0, content_bottom_};
            MapperSummary(*mapper, x, y, content_width);
            mapper_layout_.last_command = command_count_;
            mapper_layout_.last_text = text_count_;
        }
    }

    void PerformancePanelLayout::RefreshNetwork(const PerfNetworkSnapshot& network)
    {
        if (!network_layout_.width)
        {
            return;
        }

        uint32_t old_last_command = network_layout_.last_command;
        uint32_t old_last_text = network_layout_.last_text;
        retained_commands_.assign(commands_.begin() + old_last_command, commands_.begin() + command_count_);
        retained_texts_.assign(texts_.begin() + old_last_text, texts_.begin() + text_count_);

        command_count_ = network_layout_.first_command;
        text_count_ = network_layout_.first_text;
        int bottom = content_bottom_;
        content_bottom_ = network_layout_.bottom;
        int y = network_layout_.y;

        NetworkContents(network, network_layout_.x, y, network_layout_.width, network_layout_.graph_height, network_layout_.standalone);
        network_layout_.last_command = command_count_;
        network_layout_.last_text = text_count_;
        ScaleCommands(network_layout_.first_command, network_layout_.last_command);
        content_bottom_ = bottom;

        int command_delta = int(command_count_) - int(old_last_command);
        int text_delta = int(text_count_) - int(old_last_text);
        assert(command_count_ + retained_commands_.size() <= commands_.size());
        assert(text_count_ + retained_texts_.size() <= texts_.size());

        for (PerfPanelCommand command : retained_commands_)
        {
            if (command.kind == PerfPanelCommandKind::Text)
            {
                command.text_index = uint32_t(int(command.text_index) + text_delta);
            }

            commands_[command_count_++] = command;
        }

        for (const std::array<char, 128>& text : retained_texts_)
        {
            texts_[text_count_++] = text;
        }

        if (mapper_layout_.width && mapper_layout_.first_command >= old_last_command)
        {
            mapper_layout_.first_command = uint32_t(int(mapper_layout_.first_command) + command_delta);
            mapper_layout_.last_command = uint32_t(int(mapper_layout_.last_command) + command_delta);
            mapper_layout_.first_text = uint32_t(int(mapper_layout_.first_text) + text_delta);
            mapper_layout_.last_text = uint32_t(int(mapper_layout_.last_text) + text_delta);
        }

        if (batch_layout_.level && batch_layout_.first_command >= old_last_command)
        {
            batch_layout_.first_command = uint32_t(int(batch_layout_.first_command) + command_delta);
            batch_layout_.last_command = uint32_t(int(batch_layout_.last_command) + command_delta);
            batch_layout_.first_text = uint32_t(int(batch_layout_.first_text) + text_delta);
            batch_layout_.last_text = uint32_t(int(batch_layout_.last_text) + text_delta);
        }
    }

    void PerformancePanelLayout::RefreshMapper(const PerfMapperSnapshot& mapper)
    {
        if (!mapper_layout_.width)
        {
            return;
        }

        uint32_t command_count = command_count_;
        uint32_t text_count = text_count_;
        int bottom = content_bottom_;
        command_count_ = mapper_layout_.first_command;
        text_count_ = mapper_layout_.first_text;
        content_bottom_ = mapper_layout_.bottom;
        int y = mapper_layout_.y;
        MapperSummary(mapper, mapper_layout_.x, y, mapper_layout_.width);
        assert(command_count_ == mapper_layout_.last_command && text_count_ == mapper_layout_.last_text);
        ScaleCommands(mapper_layout_.first_command, mapper_layout_.last_command);
        command_count_ = command_count;
        text_count_ = text_count;
        content_bottom_ = bottom;
    }

    void PerformancePanelLayout::RefreshBatching(const PerfSnapshot& snapshot, const PerfSnapshot* batch_snapshot)
    {
        if (!batch_layout_.level)
        {
            return;
        }

        uint32_t command_count = command_count_;
        uint32_t text_count = text_count_;
        int bottom = content_bottom_;
        command_count_ = batch_layout_.first_command;
        text_count_ = batch_layout_.first_text;
        content_bottom_ = batch_layout_.bottom;
        StudioBatchingContents(
            snapshot, batch_layout_.x, batch_layout_.y, batch_layout_.width, batch_layout_.compact, batch_layout_.level, batch_snapshot
        );
        assert(command_count_ == batch_layout_.last_command && text_count_ == batch_layout_.last_text);
        ScaleCommands(batch_layout_.first_command, batch_layout_.last_command);
        command_count_ = command_count;
        text_count_ = text_count;
        content_bottom_ = bottom;
    }

    void PerformancePanelLayout::StudioBatching(
        const PerfSnapshot& snapshot,
        int x,
        int y,
        int width,
        bool compact,
        int batch_level,
        const PerfSnapshot* batch_snapshot
    )
    {
        batch_layout_ = {command_count_, 0, text_count_, 0, x, y, width, content_bottom_, batch_level, compact};
        StudioBatchingContents(snapshot, x, y, width, compact, batch_level, batch_snapshot);
        batch_layout_.last_command = command_count_;
        batch_layout_.last_text = text_count_;
    }

    void PerformancePanelLayout::StudioBatchingContents(
        const PerfSnapshot& snapshot,
        int x,
        int y,
        int width,
        bool compact,
        int batch_level,
        const PerfSnapshot* batch_snapshot
    )
    {
        const PerfSnapshot& counts = batch_snapshot ? *batch_snapshot : snapshot;
        auto count = [&](PerfCounterId counter) { return Counter(counts, counter); };
        unsigned long long packets = count(PerfCounterId::StudioPackets);
        unsigned long long grouped_meshes = count(PerfCounterId::StudioBatchedMeshes);
        unsigned long long single_meshes = count(PerfCounterId::StudioSingleMeshes);
        unsigned long long ranges = count(PerfCounterId::StudioConstituentDraws);
        unsigned long long saved_calls = ranges - count(PerfCounterId::StudioDrawCalls);

        char title[160];
        std::snprintf(title, sizeof(title), "%s | totals, last %u frames", labels_.batch_title, counts.cpu_frame_count);
        SectionHeader(title, x, y, width);
        TableHeader({"Item", "Grouped", "Single", "Total", "Grouped %"}, x, y, width);
        y += kLineHeight;
        StudioBatchingRow(
            "Model draws", count(PerfCounterId::StudioSharedPackets), count(PerfCounterId::StudioSinglePackets), packets, x, y, width
        );
        y += kLineHeight;
        StudioBatchingRow("MultiDraw meshes", grouped_meshes, single_meshes, grouped_meshes + single_meshes, x, y, width);
        y += kLineHeight;

        for (const performance::GaugeDescriptor& gauge : performance::kGauges)
        {
            if (gauge.placement != performance::GaugePlacement::Studio)
            {
                continue;
            }

            uint32_t index = uint32_t(gauge.id);
            const PerfScalarValue& value = snapshot.gauges[index];
            const PerfScalarSummary& summary = counts.gauge_summaries[index];
            char last[24];
            char maximum[24];

            if (value.state == ncl_perf::PerfSampleState::Measured)
            {
                std::snprintf(last, sizeof(last), "%g", value.value);
            }
            else
            {
                std::snprintf(last, sizeof(last), "%s", stages::StateName(value.state));
            }

            if (summary.state == ncl_perf::PerfSampleState::Measured)
            {
                std::snprintf(maximum, sizeof(maximum), "%g", summary.maximum);
            }
            else
            {
                std::snprintf(maximum, sizeof(maximum), "%s", stages::StateName(summary.state));
            }

            Label(
                x,
                y,
                width,
                kTextColor,
                "%s | last=%s max=%s %s",
                gauge.label,
                last,
                maximum,
                performance::UnitLabel(gauge.unit)
            );
            y += kLineHeight;
        }

        Separator(x, y, width);

        if (ranges)
        {
            Label(
                x,
                y,
                width,
                kTextColor,
                "Draw calls saved: %llu / %llu (%.1f%%)",
                saved_calls,
                ranges,
                100.0 * double(saved_calls) / double(ranges)
            );
        }
        else
        {
            Label(x, y, width, kTextColor, "Draw calls saved: -");
        }

        y += kLineHeight;

        Separator(x, y, width);

        if (batch_level < 2)
        {
            Label(x, y, width, kSecondaryText, "Detailed counters: perf_dump");
            return;
        }

        const PerfSnapshot* cpu_snapshot = batch_snapshot;
        auto cpu_sample = [&](ncl_perf::PerfScopeId_t id) {
            return cpu_snapshot ? CpuSample(*cpu_snapshot, id) : nullptr;
        };
        struct Cost
        {
            const char* name;
            const ncl_perf::PerfCpuSample* sample;
        };
        std::array<Cost, performance::kStudioCostCapacity> costs{};

        for (uint32_t index = 0; index < performance::CountStudioCosts(); ++index)
        {
            const performance::StageDescriptor& stage = *performance::StudioCost(index);
            costs[index] = {stage.studio_label, cpu_sample(stage.scope.scope_id)};
        }

        auto is_measured = [&](const ncl_perf::PerfCpuSample* sample) {
            return cpu_snapshot && cpu_snapshot->cpu_frame_count && sample && sample->state == ncl_perf::PerfSampleState::Measured &&
                   sample->calls;
        };
        const ncl_perf::PerfCpuSample* top_detail = nullptr;
        ncl_perf::PerfDuration_t other_details = 0;

        for (uint32_t index = 0; cpu_snapshot && index < cpu_snapshot->cpu_count; ++index)
        {
            const ncl_perf::PerfCpuSample& sample = cpu_snapshot->cpu_tree[index];

            if (!is_measured(&sample))
            {
                continue;
            }

            const performance::StageDescriptor* stage = performance::Stage(sample.scope_id);

            if (stage && stage->studio_detail && (!top_detail || sample.inclusive_ns > top_detail->inclusive_ns))
            {
                top_detail = &sample;
            }

            if (stage && stage->studio_other_cost)
            {
                other_details += sample.inclusive_ns;
            }
        }

        const Cost* top_cost = nullptr;

        for (const Cost& cost : costs)
        {
            if (is_measured(cost.sample) && (!top_cost || cost.sample->inclusive_ns > top_cost->sample->inclusive_ns))
            {
                top_cost = &cost;
            }
        }

        if (compact)
        {
            if (top_detail)
            {
                Label(
                    x,
                    y,
                    width,
                    kSectionText,
                    "Top detail: %s %.3f ms/frame",
                    stages::ScopeName(top_detail->scope_id),
                    double(top_detail->inclusive_ns) / 1e6 / cpu_snapshot->cpu_frame_count
                );
            }
            else if (top_cost)
            {
                Label(
                    x,
                    y,
                    width,
                    kSectionText,
                    "Top GL cost: %s %.3f ms/frame",
                    top_cost->name,
                    double(top_cost->sample->inclusive_ns) / 1e6 / cpu_snapshot->cpu_frame_count
                );
            }
            else
            {
                Label(x, y, width, kTextColor, cpu_snapshot ? "CPU live: no Studio samples" : "CPU live: waiting for frame");
            }
        }
        else
        {
            if (cpu_snapshot && cpu_snapshot->cpu_frame_count)
            {
                Label(x, y, width, kSecondaryText, "CPU live: averages, last %u frames", cpu_snapshot->cpu_frame_count);
            }
            else
            {
                Label(x, y, width, kSecondaryText, "CPU live: waiting for frame");
            }

            y += kLineHeight;
            TableHeader({"CPU operation", "ms/frame", "us/call", "Calls total"}, x, y, width);

            for (const Cost& cost : costs)
            {
                y += kLineHeight;
                char per_frame[24] = "-";
                char per_call[24] = "-";
                char calls[24] = "-";

                if (is_measured(cost.sample))
                {
                    std::snprintf(
                        per_frame, sizeof(per_frame), "%.3f", double(cost.sample->inclusive_ns) / 1e6 / cpu_snapshot->cpu_frame_count
                    );
                    std::snprintf(per_call, sizeof(per_call), "%.3f", double(cost.sample->inclusive_ns) / 1e3 / double(cost.sample->calls));
                    std::snprintf(calls, sizeof(calls), "%llu", static_cast<unsigned long long>(cost.sample->calls));
                }

                TableCells({cost.name, per_frame, per_call, calls}, x, y, width, &cost == top_cost ? kSectionText : kTextColor);
            }

            y += kLineHeight;
            Separator(x, y, width);
            const ncl_perf::PerfCpuSample* studio = cpu_sample(PerfStage::Studio);

            if (is_measured(studio))
            {
                Label(
                    x,
                    y,
                    width,
                    kTextColor,
                    "Other Studio: %.3f ms/frame",
                    double(studio->self_ns + other_details) / 1e6 / cpu_snapshot->cpu_frame_count
                );
            }
            else
            {
                Label(x, y, width, kTextColor, "Other Studio: -");
            }

            y += kLineHeight;

            if (top_detail)
            {
                Label(
                    x,
                    y,
                    width,
                    kSectionText,
                    "Top detail: %s %.3f ms/frame",
                    stages::ScopeName(top_detail->scope_id),
                    double(top_detail->inclusive_ns) / 1e6 / cpu_snapshot->cpu_frame_count
                );
            }
            else
            {
                Label(x, y, width, kTextColor, "Top detail: no samples");
            }
        }

        y += kLineHeight;
        Label(x, y, width, kSecondaryText, "Detailed counters: perf_dump");
    }

    void PerformancePanelLayout::TableCells(std::initializer_list<const char*> cells, int x, int y, int width, uint32_t color)
    {
        int shares = int(cells.size()) + 1;
        int column = 0;

        for (const char* cell : cells)
        {
            if (*cell)
            {
                int column_x = x + (column ? width * (column + 1) / shares : 0);
                int column_width = column ? width / shares : width * 2 / shares;
                Label(column_x, y, column_width - 2, color, "%s", cell);
            }
            ++column;
        }
    }

    void PerformancePanelLayout::Separator(int x, int& y, int width)
    {
        int line_y = y + kSectionGap / 2;

        if (line_y < content_bottom_)
        {
            Add(PerfPanelCommandKind::Rect, x, line_y, width, 1, kSectionAccent);
        }

        y += kSectionGap;
    }

    void PerformancePanelLayout::StudioBatchingRow(
        const char* name, uint64_t grouped, uint64_t single, uint64_t total, int x, int y, int width
    )
    {
        char grouped_text[24];
        char single_text[24];
        char total_text[24];
        char share[24] = "-";
        std::snprintf(grouped_text, sizeof(grouped_text), "%llu", static_cast<unsigned long long>(grouped));
        std::snprintf(single_text, sizeof(single_text), "%llu", static_cast<unsigned long long>(single));
        std::snprintf(total_text, sizeof(total_text), "%llu", static_cast<unsigned long long>(total));

        if (total)
        {
            std::snprintf(share, sizeof(share), "%.1f%%", 100.0 * double(grouped) / double(total));
        }

        TableCells({name, grouped_text, single_text, total_text, share}, x, y, width, kTextColor);
    }

    void PerformancePanelLayout::SectionHeader(const char* title, int x, int& y, int width)
    {
        int header_height = std::min(kLineHeight, content_bottom_ - y);
        Add(PerfPanelCommandKind::Rect, x, y, width, header_height, kSectionTitleBackground);
        Add(PerfPanelCommandKind::Rect, x, y, 2, header_height, kSectionAccent);
        Label(x + 8, y, width - 8, kSectionText, "%s", title);
        y += kLineHeight;
    }

    void PerformancePanelLayout::TableHeader(std::initializer_list<const char*> cells, int x, int y, int width)
    {
        Add(PerfPanelCommandKind::Rect, x, y, width, std::min(kLineHeight, content_bottom_ - y), kSectionBackground);
        TableCells(cells, x, y, width, kSectionText);
    }

    void PerformancePanelLayout::StatisticHeader(const char* name, const char* unit, int x, int y, int width, bool percentile)
    {
        char title[128];
        std::snprintf(title, sizeof(title), "%s (%s)", name, unit);
        TableHeader({title, "avg", percentile ? "p99" : "median", "max"}, x, y, width);
    }

    void PerformancePanelLayout::StatisticRow(
        const char* name,
        std::array<double, 3> values,
        const char* unavailable,
        int x,
        int y,
        int width,
        uint32_t color
    )
    {
        if (*name)
        {
            Label(x, y, width * 2 / 5, color, "%s", name);
        }

        for (int column = 0; column < 3; ++column)
        {
            int column_x = x + width * (column + 2) / 5;

            if (!std::isfinite(values[column]))
            {
                Label(column_x, y, width / 5, color, "-");
            }
            else if (unavailable)
            {
                Label(column_x, y, width / 5, color, "%s", column ? "-" : unavailable);
            }
            else
            {
                Label(column_x, y, width / 5, color, values[column] >= 1000000 ? "%.2g" : "%.2f", values[column]);
            }
        }
    }

    void PerformancePanelLayout::Statistics(const char* name, const PerfSummary& summary, int x, int y, int width, uint32_t color)
    {
        StatisticRow(
            name,
            {double(summary.mean_ns) / 1e6,
             summary.percentiles_available ? double(summary.p99_ns) / 1e6 : std::numeric_limits<double>::quiet_NaN(),
             double(summary.maximum_ns) / 1e6},
            summary.count ? nullptr : stages::StateName(summary.state),
            x,
            y,
            width,
            color
        );
    }

    void PerformancePanelLayout::TimingTable(const PerfSnapshot& snapshot, bool cpu, int x, int& y, int width)
    {
        StatisticHeader(cpu ? "CPU" : "GPU", performance::UnitLabel(performance::MetricUnit::Nanoseconds), x, y, width, true);
        y += kLineHeight;
        performance::SeriesPlacement placement = TimingPlacement(cpu);
        uint32_t count = performance::CountSeries(placement);

        for (uint32_t index = 0; index < count; ++index)
        {
            const performance::SeriesDescriptor& series = *performance::OrderedSeries(placement, index);
            std::array<char, ncl_perf::kPerfNameCapacity> label = TimingLabel(series);
            Statistics(label.data(), snapshot.summaries[uint32_t(series.id)], x, y, width, series.presentation.color);
            y += kLineHeight;
        }
    }

    void PerformancePanelLayout::BuildFrameGroups(std::span<const PerfFrameGraph> frames)
    {
        for (uint32_t group = 0; group < 2; ++group)
        {
            performance::SeriesPlacement placement = TimingPlacement(group == 0);
            std::vector<PerfGroupedGraphBin>& bins = frame_groups_[group];
            bins.resize(frames.size());

            for (uint32_t index = 0; index < frames.size(); ++index)
            {
                PerfGroupedGraphBin& bin = bins[index];
                bin = {};
                const PerfFrameGraph& frame = frames[index];
                const performance::SeriesDescriptor* interval_series = performance::GroupSeries(placement, performance::GroupRole::Interval);

                if (interval_series)
                {
                    const PerfValue& interval = frame.values[uint32_t(interval_series->id)];

                    if (performance::IsSummarySample(*interval_series, interval))
                    {
                        bin.interval = {interval.duration_ns, interval.duration_ns, 1};
                    }
                }

                const PerfValue& total = frame.values[uint32_t(performance::GroupSeries(placement, performance::GroupRole::Total)->id)];

                if (total.state != ncl_perf::PerfSampleState::Measured)
                {
                    continue;
                }

                bin.total = {total.duration_ns, total.duration_ns, 1};
                bin.parts_available = true;
                ncl_perf::PerfDuration_t sum = 0;

                for (uint32_t part = 0; part < performance::CountParts(placement); ++part)
                {
                    PerfSeries series = performance::GroupSeries(placement, performance::GroupRole::Part, part)->id;
                    const PerfValue& value = frame.values[uint32_t(series)];

                    if (value.state == ncl_perf::PerfSampleState::Measured)
                    {
                        bin.parts[part] = value.duration_ns;
                        sum += value.duration_ns;
                    }
                    else if (value.state != ncl_perf::PerfSampleState::Inactive)
                    {
                        bin.parts_available = false;
                    }
                }

                bin.parts_available = bin.parts_available && sum <= total.duration_ns;
            }
        }
    }

    void PerformancePanelLayout::FrameGraph(
        std::span<const PerfFrameGraph> frames,
        PerfSeries series,
        int x,
        int y,
        int width,
        int height,
        uint32_t color
    )
    {
        ncl_perf::PerfDuration_t maximum = 1000000;
        group_samples_.resize(frames.size());

        for (uint32_t index = 0; index < frames.size(); ++index)
        {
            const PerfValue& value = frames[index].values[uint32_t(series)];
            bool available = performance::IsSummarySample(*performance::Series(series), value);

            if (available)
            {
                maximum = std::max(maximum, value.duration_ns);
            }

            group_samples_[index] = {0, double(value.duration_ns), available, false};
        }

        Plot({}, x, y, width, height, color, true, double(maximum) / 1e6, uint32_t(frames.size()));
        x += kGraphAxisWidth;
        width -= kGraphAxisWidth;
        Columns(group_samples_, x, y, width, height, (color & 0xffffff00) | 96, double(maximum));
    }

    void PerformancePanelLayout::GroupGraph(
        std::span<const PerfGroupedGraphBin> bins,
        bool cpu,
        int x,
        int y,
        int width,
        int height,
        ncl_perf::PerfDuration_t maximum,
        bool interpolate
    )
    {
        Plot({}, x, y, width, height, kTextColor, true, double(maximum) / 1e6, uint32_t(bins.size()));
        x += kGraphAxisWidth;
        width -= kGraphAxisWidth;
        group_samples_.resize(bins.size());
        std::span<ColumnSample> samples = group_samples_;
        performance::SeriesPlacement placement = TimingPlacement(cpu);

        for (performance::GroupRole role : {performance::GroupRole::Interval, performance::GroupRole::Total})
        {
            const performance::SeriesDescriptor* series = performance::GroupSeries(placement, role);

            if (!series)
            {
                continue;
            }

            bool interval = role == performance::GroupRole::Interval;

            for (uint32_t index = 0; index < bins.size(); ++index)
            {
                const PerfGraphBin& value = interval ? bins[index].interval : bins[index].total;
                // A held stack edge needs the same parent edge when the composition becomes unavailable.
                bool interpolate_next =
                    interpolate && (interval || index + 1 == bins.size() || bins[index].parts_available == bins[index + 1].parts_available);
                samples[index] = {0, double(value.maximum_ns), value.count != 0, interpolate_next};
            }

            Columns(samples, x, y, width, height, (series->presentation.color & 0xffffff00) | 72, double(maximum));
        }

        for (uint32_t part = 0; part < performance::CountParts(placement); ++part)
        {
            const performance::SeriesDescriptor& series = *performance::GroupSeries(placement, performance::GroupRole::Part, part);

            for (uint32_t index = 0; index < bins.size(); ++index)
            {
                const PerfGroupedGraphBin& bin = bins[index];
                ncl_perf::PerfDuration_t lower = 0;

                for (uint32_t previous = 0; previous < part; ++previous)
                {
                    lower += bin.parts[previous];
                }

                samples[index] = {double(lower), double(lower + bin.parts[part]), bin.parts_available && bin.total.count != 0, interpolate};
            }

            Columns(samples, x, y, width, height, (series.presentation.color & 0xffffff00) | 192, double(maximum));
        }
    }

    void PerformancePanelLayout::Columns(
        std::span<const ColumnSample> samples,
        int x,
        int y,
        int width,
        int height,
        uint32_t color,
        double maximum
    )
    {
        if (samples.empty())
        {
            return;
        }

        auto ordinate = [&](double value) { return y + height - int(value / maximum * height); };

        for (uint32_t index = 0; index < samples.size(); ++index)
        {
            const ColumnSample& sample = samples[index];

            if (!sample.available)
            {
                continue;
            }

            const ColumnSample& right =
                sample.interpolate_next && index + 1 < samples.size() && samples[index + 1].available ? samples[index + 1] : sample;
            int left_top = ordinate(sample.upper);
            int right_top = ordinate(right.upper);
            int left_bottom = ordinate(sample.lower);
            int right_bottom = ordinate(right.lower);

            int top = std::min(left_top, right_top);
            int bottom = std::max(left_bottom, right_bottom);
            uint32_t before = command_count_;
            AddColumn(PerfPanelCommandKind::Peak, index, uint32_t(samples.size()), x, top, width, bottom - top, color);

            if (command_count_ > before)
            {
                PerfPanelCommand& command = commands_[before];
                command.area = true;
                command.vertex_y = {left_top, right_top, right_bottom, left_bottom};
            }
        }
    }

    bool PerformancePanelLayout::SetBackgroundAlpha(float alpha)
    {
        alpha = std::isfinite(alpha) ? std::clamp(alpha, 0.0f, 1.0f) : kPerfDefaultBackgroundAlpha;
        bool changed = background_alpha_ != alpha;
        background_alpha_ = alpha;

        return changed;
    }

    void PerformancePanelLayout::SetLabels(const PerfPanelLabels& labels)
    {
        labels_ = labels;
    }

    std::span<const PerfPanelCommand> PerformancePanelLayout::get_commands() const
    {
        return {commands_.data(), command_count_};
    }

    const char* PerformancePanelLayout::get_text(const PerfPanelCommand& command) const
    {
        return texts_[command.text_index].data();
    }

    int PerformancePanelLayout::get_font_tall() const
    {
        return font_tall_;
    }

    float PerformancePanelLayout::get_scale() const
    {
        return scale_;
    }

    void PerformancePanelLayout::Add(PerfPanelCommandKind kind, int x, int y, int width, int height, uint32_t color)
    {
        if (command_count_ == commands_.size() || x < 0 || y < 0 || width <= 0 || height <= 0 || x >= width_ || y >= height_)
        {
            return;
        }

        int clipped_width = std::min(width, width_ - x);
        int clipped_height = std::min(height, height_ - y);
        commands_[command_count_++] = {kind, x, y, clipped_width, clipped_height, color, 0, {y, y, y + clipped_height, y + clipped_height}};
    }

    void PerformancePanelLayout::AddColumn(
        PerfPanelCommandKind kind,
        uint32_t index,
        uint32_t count,
        int x,
        int y,
        int width,
        int height,
        uint32_t color
    )
    {
        ColumnGrid grid = CalculateColumnGrid(x, width, count);

        if (grid.right - int(count - index) * grid.step < grid.left)
        {
            return;
        }

        uint32_t before = command_count_;
        Add(kind, x, y, width, height, color);

        if (command_count_ > before)
        {
            commands_[before].column_index = index;
            commands_[before].column_count = count;
        }
    }

    void PerformancePanelLayout::Label(int x, int y, int width, uint32_t color, const char* format, ...)
    {
        if (text_count_ == texts_.size() || command_count_ == commands_.size() || y + kLineHeight > content_bottom_ || x >= width_ ||
            width <= 0)
        {
            return;
        }

        std::array<char, 128>& text = texts_[text_count_];
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(text.data(), text.size(), format, arguments);
        va_end(arguments);

        int character_limit = width / 7;
        int cursor = 0;
        int characters = 0;

        while (text[cursor] && characters < character_limit)
        {
            uchar32 codepoint = 0;
            bool error = false;
            int consumed = Q_UTF8ToUChar32(text.data() + cursor, codepoint, error);

            if (error || consumed <= 0 || cursor + consumed >= int(text.size()))
            {
                break;
            }

            cursor += consumed;
            ++characters;
        }

        text[cursor] = 0;

        uint32_t before = command_count_;
        Add(PerfPanelCommandKind::Text, x, y, width, kLineHeight, color);

        if (command_count_ > before)
        {
            commands_[before].text_index = text_count_++;
        }
    }

    void PerformancePanelLayout::Graph(
        const PerfSnapshot& snapshot,
        PerfSeries series,
        int x,
        int y,
        int width,
        int height,
        uint32_t color,
        bool background,
        ncl_perf::PerfDuration_t scale
    )
    {
        for (const PerfGraphBin& bin : snapshot.graphs[uint32_t(series)])
        {
            scale = std::max(scale, bin.maximum_ns);
        }

        scale = std::max<ncl_perf::PerfDuration_t>(scale, 1000000);
        Plot({}, x, y, width, height, color, background, double(scale) / 1e6);
        x += kGraphAxisWidth;
        width -= kGraphAxisWidth;
        std::array<ColumnSample, kPerfGraphCapacity> samples;

        for (uint32_t index = 0; index < samples.size(); ++index)
        {
            const PerfGraphBin& source = snapshot.graphs[uint32_t(series)][index];
            samples[index] = {0, double(source.maximum_ns), source.count != 0};
        }

        Columns(samples, x, y, width, height, (color & 0xffffff00) | 96, double(scale));
    }

    void PerformancePanelLayout::Plot(
        std::span<const PerfPlotBin> bins,
        int x,
        int y,
        int width,
        int height,
        uint32_t color,
        bool background,
        double maximum,
        uint32_t column_count
    )
    {
        maximum = std::max(1.0, maximum);

        for (const PerfPlotBin& bin : bins)
        {
            maximum = std::max(maximum, bin.maximum);
        }

        int plot_x = x + kGraphAxisWidth;
        width -= kGraphAxisWidth;

        if (width <= 0)
        {
            return;
        }

        if (background)
        {
            ColumnGrid grid = CalculateColumnGrid(plot_x, width, bins.empty() ? std::max(1u, column_count) : uint32_t(bins.size()));
            auto rectangle = [&](int pixel_x, int pixel_width, int rect_y, int rect_height, uint32_t rect_color, bool area = false) {
                uint32_t before = command_count_;
                Add(PerfPanelCommandKind::Rect, plot_x, rect_y, width, rect_height, rect_color);

                if (command_count_ > before)
                {
                    PerfPanelCommand& command = commands_[before];
                    command.x = pixel_x;
                    command.width = pixel_width;
                    command.pixel_x = true;
                    command.area = area;
                }
            };
            uint32_t plot_alpha = uint32_t(std::round((kPlotBackground & 255) * background_alpha_ / kPerfDefaultBackgroundAlpha));
            rectangle(grid.left, grid.right - grid.left, y, height, (kPlotBackground & 0xffffff00) | plot_alpha, true);
            int axis_width = std::max(1, int(std::round(scale_)));
            int tick_width = std::max(1, int(std::round(4 * scale_)));
            rectangle(grid.left - axis_width, axis_width, y, height, kAxisColor, true);

            for (int tick = 0; tick < 3; ++tick)
            {
                int tick_y = y + std::min(tick * height / 2, height - 1);
                int label_y = std::clamp(tick_y - kLineHeight / 2, y, y + height - kLineHeight);
                double value = maximum * (2 - tick) / 2;
                const char* format = "%.2f";

                if (value >= 1000000)
                {
                    format = "%.1e";
                }
                else if (value >= 1000)
                {
                    format = "%.0f";
                }
                else if (value >= 100)
                {
                    format = "%.1f";
                }

                if (tick == 0 || (tick == 1 && height >= 3 * kLineHeight) || (tick == 2 && height >= 2 * kLineHeight))
                {
                    uint32_t before = command_count_;
                    Label(x, label_y, kGraphAxisWidth - 8, kTextColor, format, value);

                    if (command_count_ > before)
                    {
                        PerfPanelCommand& command = commands_[before];
                        command.x = grid.left - int(std::round(kGraphAxisWidth * scale_));
                        command.width = std::max(1, int(std::round((kGraphAxisWidth - 8) * scale_)));
                        command.pixel_x = true;
                    }
                }

                rectangle(grid.left - tick_width, tick_width, tick_y, 1, kAxisColor);
                rectangle(grid.left, grid.right - grid.left, tick_y, 1, kGridColor);
            }
        }

        x = plot_x;

        plot_samples_.resize(bins.size());

        for (uint32_t index = 0; index < bins.size(); ++index)
        {
            const PerfPlotBin& bin = bins[index];
            plot_samples_[index] = {0, bin.maximum, bin.count != 0};
        }

        Columns(plot_samples_, x, y, width, height, (color & 0xffffff00) | 96, maximum);
    }

    void PerformancePanelLayout::NetworkContents(
        const PerfNetworkSnapshot& network,
        int x,
        int& y,
        int width,
        int graph_height,
        bool standalone
    )
    {
        char title[128];
        std::snprintf(title, sizeof(title), "Network | last %zu packets", network.graphs[0].size());
        SectionHeader(title, x, y, width);
        NetworkSummary(network, x, y, width);
        y += 3 * kLineHeight;

        if (graph_height)
        {
            NetworkGraphs(network, x, y, width, graph_height);
        }

        if (standalone)
        {
            Separator(x, y, width);
            Label(x, y, width, kSecondaryText, "Packets: In %u | Out %u", network.incoming_packet_count, network.outgoing_packet_count);
            y += kLineHeight;
        }
    }

    void PerformancePanelLayout::NetworkSummary(const PerfNetworkSnapshot& network, int x, int y, int width)
    {
        if (!network.connected)
        {
            Label(x, y, width, kSecondaryText, "%s", network.demo_playback ? "demo playback" : "disconnected / unavailable");
        }
        else
        {
            Label(x, y, width, kSecondaryText, "Last packet %.3f s ago", network.packet_age_ms / 1000);
        }

        y += kLineHeight;

        for (uint32_t index = 0; index < 3; ++index)
        {
            char value[24];
            NetworkValue(network.latest[uint32_t(kNetworkSummarySeries[index])], value);
            int column_x = x + int(index ? 1 + index : 0) * width / 4;
            int column_width = width * (index ? 1 : 2) / 4;
            Label(
                column_x,
                y,
                column_width,
                kNetworkSummaryColors[index],
                "%s %s%s",
                kNetworkSummaryNames[index],
                value,
                kNetworkSummaryUnits[index]
            );
        }

        y += kLineHeight;

        for (uint32_t index = 0; index < 2; ++index)
        {
            char bytes[24];
            char packets[24];
            NetworkValue(network.latest[uint32_t(index ? PerfNetworkSeries::OutgoingTraffic : PerfNetworkSeries::IncomingTraffic)], bytes);
            NetworkValue(network.latest[uint32_t(index ? PerfNetworkSeries::OutgoingPackets : PerfNetworkSeries::IncomingPackets)], packets);
            Label(
                x + int(index) * width / 2,
                y,
                width / 2,
                index ? kOutgoingColor : kIncomingColor,
                "%s %s KiB/s %s pps",
                index ? "Out" : "In",
                bytes,
                packets
            );
        }
    }

    void PerformancePanelLayout::NetworkGraphs(const PerfNetworkSnapshot& network, int x, int& y, int width, int graph_height)
    {
        Separator(x, y, width);
        StatisticHeader("Latency", "ms", x, y, width);
        y += kLineHeight;
        const PerfNetworkSummary& latency = network.packet_summaries[uint32_t(PerfNetworkSeries::Latency)];
        StatisticRow("", {latency.mean, latency.median, latency.maximum}, latency.count ? nullptr : "-", x, y, width, kLatencyColor);
        y += kLineHeight;
        Plot(network.graphs[uint32_t(PerfNetworkSeries::Latency)], x, y, width, graph_height, kLatencyColor);
        y += graph_height;
        int plot_width = width - kGraphAxisWidth;

        for (uint32_t index = 0; index < 2; ++index)
        {
            const auto& bins = network.graphs[uint32_t(index ? PerfNetworkSeries::Choke : PerfNetworkSeries::Loss)];

            for (uint32_t bin = 0; bin < bins.size(); ++bin)
            {
                if (bins[bin].count && bins[bin].maximum > 0)
                {
                    AddColumn(
                        PerfPanelCommandKind::Rect,
                        bin,
                        uint32_t(bins.size()),
                        x + kGraphAxisWidth,
                        y + 1 + int(index) * 4,
                        plot_width,
                        3,
                        index ? kChokeColor : kLossColor
                    );
                }
            }
        }

        y += 14;
        Separator(x, y, width);
        StatisticHeader("Packet size", "B", x, y, width);
        y += kLineHeight;
        double maximum = 1;

        for (uint32_t index = 0; index < 2; ++index)
        {
            uint32_t series = uint32_t(index ? PerfNetworkSeries::OutgoingTraffic : PerfNetworkSeries::IncomingTraffic);
            uint32_t color = index ? kOutgoingColor : kIncomingColor;
            const PerfNetworkSummary& summary = network.packet_summaries[series];
            StatisticRow(
                index ? "Out" : "In", {summary.mean, summary.median, summary.maximum}, summary.count ? nullptr : "-", x, y, width, color
            );
            y += kLineHeight;

            for (const PerfPlotBin& bin : network.graphs[series])
            {
                maximum = std::max(maximum, bin.maximum);
            }
        }

        Plot(network.graphs[uint32_t(PerfNetworkSeries::IncomingTraffic)], x, y, width, graph_height, kIncomingColor, true, maximum);
        Plot(network.graphs[uint32_t(PerfNetworkSeries::OutgoingTraffic)], x, y, width, graph_height, kOutgoingColor, false, maximum);
        y += graph_height + 4;
    }

    void PerformancePanelLayout::MapperSummary(const PerfMapperSnapshot& mapper, int x, int& y, int width)
    {
        if (!mapper.map_loaded)
        {
            Label(x, y, width, kTextColor, "Map unavailable");
            y += kLineHeight;
            return;
        }

        Label(x, y, width, kSecondaryText, "E: dump to console");
        y += kLineHeight;

        if (mapper.view_measured)
        {
            Label(x, y, width, kTextColor, "wpoly %llu | epoly %llu", mapper.wpoly, mapper.epoly);
        }
        else
        {
            Label(x, y, width, kTextColor, "wpoly - | epoly -");
        }

        y += kLineHeight;

        if (mapper.map_entities_available && mapper.view_measured)
        {
            Label(x, y, width, kTextColor, "Brush entities: map %u | PVS %u", mapper.brush_entities, mapper.pvs_brush_entities);
        }
        else if (mapper.view_measured)
        {
            Label(x, y, width, kTextColor, "Brush entities: PVS %u", mapper.pvs_brush_entities);
        }
        else
        {
            Label(x, y, width, kTextColor, "Brush entities: PVS -");
        }

        y += kLineHeight;
        Label(x, y, width, kTextColor, "Position: %.1f %.1f %.1f", mapper.position.x, mapper.position.y, mapper.position.z);
        y += kLineHeight;

        if (mapper.view_measured)
        {
            Label(x, y, width, kTextColor, "Angles: %.1f %.1f %.1f", mapper.angles.x, mapper.angles.y, mapper.angles.z);
        }
        else
        {
            Label(x, y, width, kTextColor, "Angles: -");
        }

        y += kLineHeight;

        if (mapper.target_hit)
        {
            Label(
                x, y, width, kTextColor, "Look: %.1f %.1f %.1f", mapper.target_position.x, mapper.target_position.y, mapper.target_position.z
            );
        }
        else
        {
            Label(x, y, width, kTextColor, "Look: -");
        }

        y += kLineHeight;

        if (mapper.level < 2)
        {
            return;
        }

        if (mapper.target_hit)
        {
            Label(x, y, width, kTextColor, "Object%s: %s", mapper.target_bounds ? " [bounds]" : "", mapper.target_name.c_str());
            y += kLineHeight;
            Label(
                x, y, width, kTextColor, "Object origin: %.1f %.1f %.1f", mapper.target_origin.x, mapper.target_origin.y, mapper.target_origin.z
            );
        }
        else
        {
            Label(x, y, width, kTextColor, "Object: -");
            y += kLineHeight;
            Label(x, y, width, kTextColor, "Object origin: -");
        }

        y += kLineHeight;

        uint32_t rows = std::min(kPerfMapPropertyRows, uint32_t(mapper.properties.size()));
        uint32_t displayed = 0;

        for (uint32_t index = 0; index < rows; ++index)
        {
            const PerfMapProperty& property = mapper.properties[index];

            if (y + 2 * kLineHeight > content_bottom_)
            {
                break;
            }

            Label(x, y, width, kTextColor, "%s: %s", property.name.c_str(), property.value.c_str());
            ++displayed;
            y += kLineHeight;
        }

        Label(x, y, width, kSecondaryText, "Properties %u/%zu | full: perf_dump", displayed, mapper.properties.size());
        y += kLineHeight;
    }
} // namespace performance
