#include "performance/performance_scopes.h"

namespace performance
{
    namespace
    {
        template <class TMonitor>
        void StudioFlushImpl(TMonitor* monitor, PerfCounterId reason, size_t packets)
        {
            if (monitor == nullptr)
            {
                return;
            }

            monitor->AddCounter(PerfCounterId::StudioPackets, packets);
            monitor->AddCounter(reason, 1);

            if (packets > 1)
            {
                monitor->AddCounter(PerfCounterId::StudioSharedRuns, 1);
                monitor->AddCounter(PerfCounterId::StudioSharedPackets, packets);
            }
            else
            {
                monitor->AddCounter(PerfCounterId::StudioSinglePackets, packets);
            }
        }

        template <class TMonitor>
        void StudioImmediateImpl(TMonitor* monitor, PerfCounterId reason)
        {
            if (monitor != nullptr)
            {
                monitor->AddCounter(reason, 1);
            }
        }

        template <class TMonitor>
        void ReportDrawImpl(TMonitor* monitor, uint64_t index_count, size_t range_count)
        {
            if (monitor == nullptr)
            {
                return;
            }

            monitor->AddCounter(PerfCounterId::StudioDrawCalls, 1);
            monitor->AddCounter(PerfCounterId::StudioConstituentDraws, static_cast<uint64_t>(range_count));
            monitor->AddCounter(PerfCounterId::StudioTriangles, index_count / 3);

            if (range_count > 1)
            {
                monitor->AddCounter(PerfCounterId::StudioMultiDrawBatches, 1);
                monitor->AddCounter(PerfCounterId::StudioBatchedMeshes, static_cast<uint64_t>(range_count));
            }
            else if (range_count == 1)
            {
                monitor->AddCounter(PerfCounterId::StudioSingleMeshes, 1);
            }
        }

        template <class TMonitor>
        void StudioQueuePeakImpl(TMonitor* monitor, size_t packets)
        {
            if (monitor != nullptr)
            {
                monitor->SetGauge(PerfGaugeId::StudioQueuePeakPackets, double(packets));
            }
        }
    } // namespace

    void reporting::ReportDraw(PerformanceMonitorInterface* monitor, uint64_t index_count, size_t range_count)
    {
        ReportDrawImpl(monitor, index_count, range_count);
    }

    void reporting::ReportDraw(PerformanceMonitor& monitor, uint64_t index_count, size_t range_count)
    {
        ReportDrawImpl(monitor.get_counter_monitor(), index_count, range_count);
    }

    void reporting::StudioFlush(PerformanceMonitorInterface* monitor, PerfCounterId reason, size_t packets)
    {
        StudioFlushImpl(monitor, reason, packets);
    }

    void reporting::StudioFlush(PerformanceMonitor& monitor, PerfCounterId reason, size_t packets)
    {
        StudioFlushImpl(monitor.get_counter_monitor(), reason, packets);
    }

    void reporting::StudioImmediate(PerformanceMonitorInterface* monitor, PerfCounterId reason)
    {
        StudioImmediateImpl(monitor, reason);
    }

    void reporting::StudioImmediate(PerformanceMonitor& monitor, PerfCounterId reason)
    {
        StudioImmediateImpl(monitor.get_counter_monitor(), reason);
    }

    void reporting::StudioQueuePeak(PerformanceMonitorInterface* monitor, size_t packets)
    {
        StudioQueuePeakImpl(monitor, packets);
    }

    void reporting::StudioQueuePeak(PerformanceMonitor& monitor, size_t packets)
    {
        StudioQueuePeakImpl(monitor.get_counter_monitor(), packets);
    }
} // namespace performance
