#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <concurrencpp/results/result.h>
#include <taskcoro/CancellationToken.h>
#include <taskcoro/TaskCoroImplInterface.h>

#include "hud/radar/map/layers/RadarMapLayers.h"

enum class RadarLayerAnalysisStatus
{
    Pending,
    Completed,
    Failed,
    Canceled,
};

using RadarLayerAnalysisRequest_t = uint64_t;

struct RadarLayerAnalysisEvent
{
    RadarLayerAnalysisRequest_t request{};
    uint64_t duration_ns{};
    RadarLayerAnalysisStatus status{};
};

using RadarLayerAnalysisSink_t = std::function<void(const RadarLayerAnalysisEvent&)>;

struct RadarLayerAnalysisResult
{
    RadarLayerAnalysisRequest_t request{};
    uint64_t duration_ns{};
    RadarMapLayers map;
    RadarMapStatus status = RadarMapStatus::Failed;
    std::string warning;
    std::string failure_reason;
};

class RadarLayerAnalysisJob
{
private:
    struct Input
    {
        RadarMapGeometry geometry;
        std::optional<RadarLayerOverrides> overrides;
        RadarLayerAnalysisRequest_t request{};
    };

    taskcoro::TaskCoroImplInterface& scheduler_;
    RadarLayerAnalysisSink_t sink_;
    concurrencpp::result<std::unique_ptr<RadarLayerAnalysisResult>> running_;
    std::shared_ptr<taskcoro::CancellationToken> cancellation_;
    std::optional<Input> pending_;
    std::unique_ptr<RadarLayerAnalysisResult> completed_;
    RadarLayerAnalysisRequest_t running_request_{};

public:
    explicit RadarLayerAnalysisJob(taskcoro::TaskCoroImplInterface& scheduler, RadarLayerAnalysisSink_t sink = {});

    RadarLayerAnalysisJob(const RadarLayerAnalysisJob&) = delete;
    RadarLayerAnalysisJob& operator=(const RadarLayerAnalysisJob&) = delete;
    RadarLayerAnalysisJob(RadarLayerAnalysisJob&&) = delete;
    RadarLayerAnalysisJob& operator=(RadarLayerAnalysisJob&&) = delete;
    ~RadarLayerAnalysisJob();

    void Start(
        RadarLayerAnalysisRequest_t request,
        RadarMapGeometry geometry,
        std::optional<RadarLayerOverrides> overrides = std::nullopt
    );
    void Reset();
    void Shutdown();

    std::unique_ptr<RadarLayerAnalysisResult> Poll();
    bool is_pending() const;

private:
    void Dispatch();
    void Report(const RadarLayerAnalysisEvent& event);
};
