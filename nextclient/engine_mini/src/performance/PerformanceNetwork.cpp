#include "performance/PerformanceNetwork.h"

#include <algorithm>
#include <cmath>

#include <goldsrc/cvar.h>
#include <goldsrc/engine/client_state.h>

#include "performance/PerformanceFrameHistory.h"

namespace performance
{
    namespace
    {
        constexpr ncl_perf::PerfDuration_t kSampleInterval = 100000000;
        constexpr double kStalePacketMilliseconds = 1000;
        constexpr uint32_t kFlowHistoryCount = 32;
    } // namespace

    PerfNetworkSnapshot::PerfNetworkSnapshot()
    {
        for (std::vector<PerfPlotBin>& graph : graphs)
        {
            graph.resize(kPerfGraphCapacity);
        }
    }

    void PerformanceNetworkMonitor::SetEnabled(bool enabled)
    {
        if (enabled_ != enabled)
        {
            Reset();
            enabled_ = enabled;
        }
    }

    bool PerformanceNetworkMonitor::is_enabled() const
    {
        return enabled_;
    }

    void PerformanceNetworkMonitor::Reset()
    {
        ++revision_;
        frame_packets_ = {};
        parsed_sequences_ = {};
        incoming_sequence_ = 0;
        has_sequence_ = false;
        records_ = {};
        traffic_ = {};
        sampled_bytes_ = {};
        sampled_packets_ = {};
        latest_ = {};
        first_ = 0;
        count_ = 0;
        last_sample_ = 0;
        map_generation_ = 0;
        connection_time_ = 0;
        packet_age_ms_ = 0;
        connected_ = false;
        demo_playback_ = false;
        has_sample_ = false;
        sequence_reset_ = false;
    }

    void PerformanceNetworkMonitor::ObserveTraffic(const goldsrc::netchan_t& channel)
    {
        if (!enabled_)
        {
            return;
        }

        for (uint32_t index = 0; index < traffic_.size(); ++index)
        {
            const goldsrc::flow_t& flow = channel.flow[index];
            Traffic& traffic = traffic_[index];
            uint32_t current = uint32_t(flow.current);

            if (!traffic.initialized)
            {
                traffic.current = current;
                traffic.initialized = true;
                continue;
            }

            uint32_t unread = current - traffic.current;
            if (!unread)
            {
                continue;
            }

            ++revision_;

            if (unread > UINT32_MAX / 2)
            {
                traffic.complete = false;
                sequence_reset_ = true;
                traffic.current = current;
                continue;
            }

            if (unread > kFlowHistoryCount)
            {
                traffic.complete = false;
            }

            uint32_t retained = std::min(unread, kPerfNetworkPacketCapacity);

            for (uint32_t offset = unread - retained; offset < unread; ++offset)
            {
                PerfNetworkValue packet;

                if (unread - offset > kFlowHistoryCount)
                {
                    packet.state = ncl_perf::PerfSampleState::Dropped;
                }
                else
                {
                    int bytes = flow.stats[(traffic.current + offset) % kFlowHistoryCount].size;

                    if (bytes < 0)
                    {
                        traffic.complete = false;
                        packet.state = ncl_perf::PerfSampleState::Dropped;
                    }
                    else
                    {
                        traffic.bytes += uint32_t(bytes);
                        packet = {double(bytes), ncl_perf::PerfSampleState::Measured};
                    }
                }

                traffic.sizes.Append(packet);
            }

            traffic.packets += unread;
            traffic.current = current;
        }
    }

    void PerformanceNetworkMonitor::ObserveFrames(
        const goldsrc::client_static_t& client_static,
        const goldsrc::client_state_t& client_state,
        bool normal_message
    )
    {
        if (!normal_message || !enabled_ || !connected_ || client_static.demoplayback || client_static.state != goldsrc::ca_active ||
            client_static.netchan.incoming_sequence <= 0)
        {
            return;
        }

        uint32_t sequence = uint32_t(client_static.netchan.incoming_sequence);
        uint32_t unread = has_sequence_ ? sequence - incoming_sequence_ : 1;

        if (!unread)
        {
            return;
        }

        if (unread > UINT32_MAX / 2)
        {
            frame_packets_ = {};
            parsed_sequences_ = {};
            unread = 1;
        }

        uint32_t retained = std::min(unread, kPerfNetworkPacketCapacity);
        parsed_sequences_[sequence % kPerfNetworkFrameCapacity] = sequence;

        for (uint32_t offset = unread - retained; offset < unread; ++offset)
        {
            FramePacket packet;

            if (unread - offset <= kPerfNetworkFrameCapacity)
            {
                uint32_t frame_sequence = sequence - unread + offset + 1;
                const goldsrc::frame_t& frame = client_state.frames[frame_sequence % kPerfNetworkFrameCapacity];
                packet.loss = {0, ncl_perf::PerfSampleState::Measured};
                packet.choke = {0, ncl_perf::PerfSampleState::Measured};
                if (IsFrameChoked(frame, frame_sequence))
                {
                    packet.choke.value = 100;
                }
                else if (frame.receivedtime == -1)
                {
                    packet.loss.value = 100;
                }
                else if (frame.receivedtime > 0 && !frame.invalid && std::isfinite(frame.latency) && frame.latency >= 0)
                {
                    packet.latency = {frame.latency * 1000, ncl_perf::PerfSampleState::Measured};
                }
            }

            frame_packets_.Append(packet);
        }

        incoming_sequence_ = sequence;
        has_sequence_ = true;
        ++revision_;
    }

    bool PerformanceNetworkMonitor::IsFrameChoked(const goldsrc::frame_t& frame, uint32_t sequence) const
    {
        // Missing slots retain flags from older sequences; normal messages confirm fresh choke markers.
        return frame.choked && (frame.receivedtime > 0 || parsed_sequences_[sequence % kPerfNetworkFrameCapacity] == sequence);
    }

    void PerformanceNetworkMonitor::Update(
        ncl_perf::PerfTimestamp_t now,
        const goldsrc::client_static_t* client_static,
        const goldsrc::client_state_t* client_state,
        double realtime,
        uint32_t map_generation,
        bool normal_message
    )
    {
        if (!enabled_)
        {
            return;
        }

        bool demo = client_static && client_static->demoplayback;
        bool connected = client_static && client_state && std::isfinite(realtime) &&
                         (client_static->state == goldsrc::ca_active || client_static->state == goldsrc::ca_connected);

        if (!connected || demo)
        {
            if (connected_ || count_ || demo_playback_ != demo)
            {
                Reset();
            }

            demo_playback_ = demo;
            return;
        }

        if (!connected_ || map_generation_ != map_generation || connection_time_ != client_static->netchan.connect_time || sequence_reset_ ||
            (has_sample_ && now < last_sample_))
        {
            Reset();
            connected_ = true;
            map_generation_ = map_generation;
            connection_time_ = client_static->netchan.connect_time;
        }

        packet_age_ms_ = std::max(0.0, (realtime - client_static->netchan.last_received) * 1000);
        ObserveTraffic(client_static->netchan);
        ObserveFrames(*client_static, *client_state, normal_message);

        if (has_sample_ && now - last_sample_ < kSampleInterval)
        {
            return;
        }

        latest_ = {};
        if (client_static->state == goldsrc::ca_active && client_static->netchan.incoming_sequence > 0)
        {
            uint32_t sequence = uint32_t(client_static->netchan.incoming_sequence);
            const goldsrc::frame_t& frame = client_state->frames[sequence % kPerfNetworkFrameCapacity];
            if (frame.receivedtime > 0 && !frame.invalid && !frame.choked && std::isfinite(frame.latency) && frame.latency >= 0 &&
                packet_age_ms_ <= kStalePacketMilliseconds)
            {
                latest_[uint32_t(PerfNetworkSeries::Latency)] = {frame.latency * 1000, ncl_perf::PerfSampleState::Measured};
            }
            if (std::isfinite(client_static->packet_loss))
            {
                latest_[uint32_t(PerfNetworkSeries::Loss)] = {
                    std::clamp(double(client_static->packet_loss), 0.0, 100.0), ncl_perf::PerfSampleState::Measured
                };
            }

            uint32_t frame_count = std::min(sequence, kPerfNetworkFrameCapacity);
            uint32_t choked = 0;
            for (uint32_t offset = 0; offset < frame_count; ++offset)
            {
                uint32_t frame_sequence = sequence - offset;
                choked += IsFrameChoked(client_state->frames[frame_sequence % kPerfNetworkFrameCapacity], frame_sequence);
            }
            latest_[uint32_t(PerfNetworkSeries::Choke)] = {100.0 * choked / frame_count, ncl_perf::PerfSampleState::Measured};
        }
        for (uint32_t index = 0; index < traffic_.size(); ++index)
        {
            Traffic& traffic = traffic_[index];
            uint32_t bytes_series = uint32_t(index ? PerfNetworkSeries::IncomingTraffic : PerfNetworkSeries::OutgoingTraffic);
            uint32_t packet_series = uint32_t(index ? PerfNetworkSeries::IncomingPackets : PerfNetworkSeries::OutgoingPackets);
            ncl_perf::PerfSampleState state = !has_sample_       ? ncl_perf::PerfSampleState::Pending
                                    : traffic.complete ? ncl_perf::PerfSampleState::Measured
                                                       : ncl_perf::PerfSampleState::Dropped;
            if (state == ncl_perf::PerfSampleState::Measured)
            {
                double seconds = double(now - last_sample_) / 1e9;
                latest_[bytes_series] = {double(traffic.bytes - sampled_bytes_[index]) / seconds / 1024, state};
                latest_[packet_series] = {double(traffic.packets - sampled_packets_[index]) / seconds, state};
            }
            else
            {
                latest_[bytes_series].state = state;
                latest_[packet_series].state = state;
            }
            sampled_bytes_[index] = traffic.bytes;
            sampled_packets_[index] = traffic.packets;
            traffic.complete = true;
        }

        if (count_ == records_.size())
        {
            first_ = (first_ + 1) % uint32_t(records_.size());
            --count_;
        }
        records_[(first_ + count_) % records_.size()] = {now, latest_};
        ++count_;
        last_sample_ = now;
        has_sample_ = true;
        ++revision_;
    }

    void PerformanceNetworkMonitor::BuildSnapshot(
        ncl_perf::PerfTimestamp_t end,
        ncl_perf::PerfDuration_t coverage,
        PerfNetworkSnapshot& output,
        uint32_t packet_count
    ) const
    {
        packet_count = std::min(packet_count, kPerfNetworkPacketCapacity);
        output.coverage_ns = 0;
        output.count = 0;
        output.summaries = {};
        output.packet_summaries = {};
        for (std::vector<PerfPlotBin>& graph : output.graphs)
        {
            graph.resize(packet_count);
            std::fill(graph.begin(), graph.end(), PerfPlotBin{});
        }

        output.captured_at = end;
        output.enabled = enabled_;
        output.connected = connected_;
        output.demo_playback = demo_playback_;
        output.packet_age_ms = packet_age_ms_;
        output.latest = latest_;
        output.revision = revision_;
        output.incoming_packet_count = std::min(traffic_[1].sizes.count, packet_count);
        output.outgoing_packet_count = std::min(traffic_[0].sizes.count, packet_count);
        BuildPacketGraphs(output);
        if (!count_)
        {
            return;
        }

        ncl_perf::PerfDuration_t duration = coverage ? std::min(coverage, kPerfHistoryDuration) : kPerfHistoryDuration;
        ncl_perf::PerfTimestamp_t start = end > duration ? end - duration : 0;
        if (!coverage)
        {
            start = std::max(start, std::min(end, records_[first_].timestamp));
        }
        output.coverage_ns = end - start;
        for (uint32_t index = 0; index < count_; ++index)
        {
            const Record& record = records_[(first_ + index) % records_.size()];
            output.count += record.timestamp >= start && record.timestamp <= end;
        }
        for (uint32_t series = 0; series < uint32_t(PerfNetworkSeries::Count); ++series)
        {
            std::array<double, kPerfNetworkHistoryCapacity> values{};
            PerfNetworkSummary& summary = output.summaries[series];
            double total = 0;
            for (uint32_t index = 0; index < count_; ++index)
            {
                const Record& record = records_[(first_ + index) % records_.size()];
                const PerfNetworkValue& value = record.values[series];
                if (record.timestamp < start || record.timestamp > end || value.state != ncl_perf::PerfSampleState::Measured)
                {
                    continue;
                }
                total += value.value;
                summary.maximum = std::max(summary.maximum, value.value);
                values[summary.count++] = value.value;
            }
            if (summary.count)
            {
                summary.mean = total / summary.count;
                auto middle = values.begin() + summary.count / 2;
                std::nth_element(values.begin(), middle, values.begin() + summary.count);
                summary.median = *middle;
                if (summary.count % 2 == 0)
                {
                    summary.median = (*std::max_element(values.begin(), middle) + *middle) / 2;
                }
            }
        }
    }

    void PerformanceNetworkMonitor::BuildPacketGraphs(PerfNetworkSnapshot& output) const
    {
        uint32_t column_count = uint32_t(output.graphs[uint32_t(PerfNetworkSeries::Latency)].size());
        auto append = [&](PerfNetworkSeries series, uint32_t index, const PerfNetworkValue& value) {
            if (value.state == ncl_perf::PerfSampleState::Measured)
            {
                output.graphs[uint32_t(series)][index] = {value.value, value.value, 1};
            }
        };
        for (uint32_t direction = 0; direction < traffic_.size(); ++direction)
        {
            const PacketHistory<PerfNetworkValue>& packets = traffic_[direction].sizes;
            PerfNetworkSeries series = direction ? PerfNetworkSeries::IncomingTraffic : PerfNetworkSeries::OutgoingTraffic;
            uint32_t count = std::min(packets.count, column_count);
            uint32_t first_column = column_count - count;
            for (uint32_t index = 0; index < count; ++index)
            {
                append(series, first_column + index, packets.get_value(packets.count - count + index));
            }
        }

        uint32_t frame_count = std::min(frame_packets_.count, column_count);
        uint32_t first_frame_column = column_count - frame_count;
        for (uint32_t index = 0; index < frame_count; ++index)
        {
            const FramePacket& packet = frame_packets_.get_value(frame_packets_.count - frame_count + index);
            uint32_t column = first_frame_column + index;
            append(PerfNetworkSeries::Latency, column, packet.latency);
            append(PerfNetworkSeries::Loss, column, packet.loss);
            append(PerfNetworkSeries::Choke, column, packet.choke);
        }

        std::array<double, kPerfNetworkPacketCapacity> values{};
        for (uint32_t series = 0; series < uint32_t(PerfNetworkSeries::Count); ++series)
        {
            PerfNetworkSummary& summary = output.packet_summaries[series];
            double total = 0;
            for (const PerfPlotBin& bin : output.graphs[series])
            {
                if (bin.count)
                {
                    total += bin.maximum;
                    summary.maximum = std::max(summary.maximum, bin.maximum);
                    values[summary.count++] = bin.maximum;
                }
            }
            if (summary.count)
            {
                summary.mean = total / summary.count;
                auto middle = values.begin() + summary.count / 2;
                std::nth_element(values.begin(), middle, values.begin() + summary.count);
                summary.median = *middle;
                if (summary.count % 2 == 0)
                {
                    summary.median = (*std::max_element(values.begin(), middle) + *middle) / 2;
                }
            }
        }
    }

    PerformanceNetGraphMask::PerformanceNetGraphMask(goldsrc::cvar_t* cvar, bool hide) :
        cvar_(hide ? cvar : nullptr)
    {
        if (cvar_)
        {
            setting_ = cvar_->string ? cvar_->string : "";
            value_ = cvar_->value;
            cvar_->value = 0;
        }
    }

    PerformanceNetGraphMask::~PerformanceNetGraphMask()
    {
        if (cvar_ && cvar_->value == 0 && cvar_->string && setting_ == cvar_->string)
        {
            cvar_->value = value_;
        }
    }
} // namespace performance
