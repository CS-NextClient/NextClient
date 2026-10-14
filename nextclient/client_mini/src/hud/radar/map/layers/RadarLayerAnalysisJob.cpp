#include "RadarLayerAnalysisJob.h"

#include <chrono>
#include <exception>
#include <utility>

#include <taskcoro/TaskCoro.h>

namespace
{
    std::unique_ptr<RadarLayerAnalysisResult> Analyze(
        RadarMapGeometry geometry,
        const std::optional<RadarLayerOverrides>& overrides,
        const taskcoro::CancellationToken& cancellation
    )
    {
        std::unique_ptr<RadarLayerAnalysisResult> result = std::make_unique<RadarLayerAnalysisResult>();
        const auto started = std::chrono::steady_clock::now();

        if (overrides)
        {
            result->status = result->map.Build(std::move(geometry), *overrides, result->warning, &cancellation);
        }
        else
        {
            result->status = result->map.Build(std::move(geometry), &cancellation);
        }

        result->analysis_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count();

        return result;
    }
} // namespace

RadarLayerAnalysisJob::~RadarLayerAnalysisJob()
{
    Shutdown();
}

void RadarLayerAnalysisJob::Start(RadarMapGeometry geometry, std::optional<RadarLayerOverrides> overrides)
{
    Reset();

    pending_.emplace(Input{std::move(geometry), std::move(overrides)});

    Dispatch();
}

void RadarLayerAnalysisJob::Reset()
{
    if (cancellation_)
    {
        cancellation_->SetCanceled();
    }

    pending_.reset();
    completed_.reset();
}

void RadarLayerAnalysisJob::Shutdown()
{
    Reset();

    if (running_)
    {
        try
        {
            running_.get();
        }
        catch (...)
        {}
    }
}

void RadarLayerAnalysisJob::Dispatch()
{
    if (running_ || !pending_)
    {
        return;
    }

    try
    {
        cancellation_ = taskcoro::CancellationToken::Create();

        Input input = std::move(*pending_);
        pending_.reset();

        running_ = taskcoro::TaskCoro::RunTask(
            taskcoro::TaskType::ThreadPool,
            ContinuationContextType::Callee,
            [input = std::move(input), cancellation = cancellation_]() mutable {
                return Analyze(std::move(input.geometry), input.overrides, *cancellation);
            }
        );
    }
    catch (const std::exception& exception)
    {
        pending_.reset();

        completed_ = std::make_unique<RadarLayerAnalysisResult>();
        completed_->failure_reason = exception.what();
    }
}

std::unique_ptr<RadarLayerAnalysisResult> RadarLayerAnalysisJob::Poll()
{
    if (running_ && running_.status() != concurrencpp::result_status::idle)
    {
        try
        {
            std::unique_ptr<RadarLayerAnalysisResult> result = running_.get();

            if (!cancellation_->IsCanceled())
            {
                completed_ = std::move(result);
            }
        }
        catch (const std::exception& exception)
        {
            if (!cancellation_->IsCanceled())
            {
                completed_ = std::make_unique<RadarLayerAnalysisResult>();
                completed_->failure_reason = exception.what();
            }
        }

        Dispatch();
    }

    return std::move(completed_);
}

bool RadarLayerAnalysisJob::is_pending() const
{
    return pending_.has_value() || completed_ || (running_ && !cancellation_->IsCanceled());
}
