#pragma once

#include <memory>
#include <optional>
#include <string>

#include <concurrencpp/results/result.h>
#include <taskcoro/CancellationToken.h>

#include "hud/radar/map/layers/RadarMapLayers.h"

struct RadarLayerAnalysisResult
{
    RadarMapLayers map;
    RadarMapStatus status = RadarMapStatus::Failed;
    std::string warning;
    std::string failure_reason;
    float analysis_ms{};
};

class RadarLayerAnalysisJob
{
private:
    struct Input
    {
        RadarMapGeometry geometry;
        std::optional<RadarLayerOverrides> overrides;
    };

    concurrencpp::result<std::unique_ptr<RadarLayerAnalysisResult>> running_;
    std::shared_ptr<taskcoro::CancellationToken> cancellation_;
    std::optional<Input> pending_;
    std::unique_ptr<RadarLayerAnalysisResult> completed_;

public:
    ~RadarLayerAnalysisJob();

    void Start(RadarMapGeometry geometry, std::optional<RadarLayerOverrides> overrides = std::nullopt);
    void Reset();
    void Shutdown();

    std::unique_ptr<RadarLayerAnalysisResult> Poll();
    bool is_pending() const;

private:
    void Dispatch();
};
