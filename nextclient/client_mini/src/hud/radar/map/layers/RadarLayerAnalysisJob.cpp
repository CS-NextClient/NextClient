#include "RadarLayerAnalysisJob.h"

#include <chrono>
#include <exception>
#include <utility>

#include <concurrencpp/results/result.h>
#include <ncl_perf/profiler.h>

namespace
{
    std::unique_ptr<RadarLayerAnalysisResult> Analyze(
        RadarMapGeometry geometry,
        const std::optional<RadarLayerOverrides>& overrides,
        const taskcoro::CancellationToken& cancellation,
        RadarLayerAnalysisRequest_t request
    )
    {
        NCL_PROFILE_NAMED_SCOPE("Radar layer analysis");
        std::unique_ptr<RadarLayerAnalysisResult> result = std::make_unique<RadarLayerAnalysisResult>();
        result->request = request;
        const auto started = std::chrono::steady_clock::now();

        if (overrides)
        {
            result->status = result->map.Build(std::move(geometry), *overrides, result->warning, &cancellation);
        }
        else
        {
            result->status = result->map.Build(std::move(geometry), &cancellation);
        }

        result->duration_ns =
            uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());

        return result;
    }
} // namespace

RadarLayerAnalysisJob::RadarLayerAnalysisJob(taskcoro::TaskCoroImplInterface& scheduler, RadarLayerAnalysisSink_t sink) :
    scheduler_(scheduler),
    sink_(std::move(sink))
{}

RadarLayerAnalysisJob::~RadarLayerAnalysisJob()
{
    Shutdown();
}

void RadarLayerAnalysisJob::Start(
    RadarLayerAnalysisRequest_t request,
    RadarMapGeometry geometry,
    std::optional<RadarLayerOverrides> overrides
)
{
    Reset();

    pending_.emplace(Input{std::move(geometry), std::move(overrides), request});

    Dispatch();
}

void RadarLayerAnalysisJob::Reset()
{
    if (running_ && cancellation_ && !cancellation_->IsCanceled())
    {
        Report({running_request_, 0, RadarLayerAnalysisStatus::Canceled});
        cancellation_->SetCanceled();
    }

    if (pending_)
    {
        Report({pending_->request, 0, RadarLayerAnalysisStatus::Canceled});
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

    running_request_ = pending_->request;

    try
    {
        cancellation_ = taskcoro::CancellationToken::Create();

        Input input = std::move(*pending_);
        pending_.reset();

        Report({running_request_, 0, RadarLayerAnalysisStatus::Pending});

        auto promise = std::make_shared<concurrencpp::result_promise<std::unique_ptr<RadarLayerAnalysisResult>>>();
        concurrencpp::result<std::unique_ptr<RadarLayerAnalysisResult>> result = promise->get_result();
        scheduler_.RunTask(taskcoro::TaskType::ThreadPool, [promise, input = std::move(input), cancellation = cancellation_]() mutable {
            try
            {
                promise->set_result(Analyze(std::move(input.geometry), input.overrides, *cancellation, input.request));
            }
            catch (...)
            {
                promise->set_exception(std::current_exception());
            }
        });
        running_ = std::move(result);
    }
    catch (const std::exception& exception)
    {
        pending_.reset();

        completed_ = std::make_unique<RadarLayerAnalysisResult>();
        completed_->request = running_request_;
        completed_->failure_reason = exception.what();
        Report({running_request_, 0, RadarLayerAnalysisStatus::Failed});
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
                Report({
                    result->request,
                    result->duration_ns,
                    result->status == RadarMapStatus::Failed ? RadarLayerAnalysisStatus::Failed : RadarLayerAnalysisStatus::Completed
                });
                completed_ = std::move(result);
            }
        }
        catch (const std::exception& exception)
        {
            if (!cancellation_->IsCanceled())
            {
                completed_ = std::make_unique<RadarLayerAnalysisResult>();
                completed_->request = running_request_;
                completed_->failure_reason = exception.what();
                Report({running_request_, 0, RadarLayerAnalysisStatus::Failed});
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

void RadarLayerAnalysisJob::Report(const RadarLayerAnalysisEvent& event)
{
    if (sink_)
    {
        sink_(event);
    }
}
