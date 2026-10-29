#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

#include "hud/radar/map/layers/RadarLayerAnalysisJob.h"

namespace radar::integration
{
    class Performance;

    class LayerAnalysis
    {
    private:
        Performance& performance_;
        std::unordered_map<RadarLayerAnalysisRequest_t, uint32_t> generations_;
        RadarLayerAnalysisRequest_t next_request_{};
        RadarLayerAnalysisJob job_;

    public:
        LayerAnalysis(taskcoro::TaskCoroImplInterface& scheduler, Performance& performance);
        ~LayerAnalysis();

        LayerAnalysis(const LayerAnalysis&) = delete;
        LayerAnalysis& operator=(const LayerAnalysis&) = delete;
        LayerAnalysis(LayerAnalysis&&) = delete;
        LayerAnalysis& operator=(LayerAnalysis&&) = delete;

        void Start(RadarMapGeometry geometry, std::optional<RadarLayerOverrides> overrides = std::nullopt);
        void Reset();
        void Shutdown();
        std::unique_ptr<RadarLayerAnalysisResult> Poll();
        bool is_pending() const;

    private:
        void Report(const RadarLayerAnalysisEvent& event);
    };
} // namespace radar::integration
