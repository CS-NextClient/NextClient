#pragma once

#include <array>
#include <string>
#include <vector>

#include <goldsrc/fwd.h>
#include <next_engine_mini/PerformanceData.h>

namespace performance
{
    inline constexpr uint32_t kPerfNetworkHistoryCapacity = 128;
    inline constexpr uint32_t kPerfNetworkPacketCapacity = 4096;
    inline constexpr uint32_t kPerfNetworkFrameCapacity = 64;

    enum class PerfNetworkSeries : uint32_t
    {
        Latency,
        IncomingTraffic,
        OutgoingTraffic,
        Loss,
        Choke,
        IncomingPackets,
        OutgoingPackets,
        Count
    };

    struct PerfNetworkValue
    {
        double value{};
        ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Unavailable;
    };

    struct PerfNetworkSummary
    {
        double mean{};
        double median{};
        double maximum{};
        uint32_t count{};
    };

    struct PerfPlotBin
    {
        double maximum{};
        double mean{};
        uint32_t count{};
    };

    struct PerfNetworkSnapshot
    {
        ncl_perf::PerfTimestamp_t captured_at{};
        ncl_perf::PerfDuration_t coverage_ns{};
        uint32_t count{};
        bool enabled{};
        bool connected{};
        bool demo_playback{};
        double packet_age_ms{};
        std::array<PerfNetworkValue, uint32_t(PerfNetworkSeries::Count)> latest{};
        std::array<PerfNetworkSummary, uint32_t(PerfNetworkSeries::Count)> summaries{};
        // Packet-indexed graphs use bytes for traffic; latest and summaries retain KiB/s.
        std::array<std::vector<PerfPlotBin>, uint32_t(PerfNetworkSeries::Count)> graphs{};
        std::array<PerfNetworkSummary, uint32_t(PerfNetworkSeries::Count)> packet_summaries{};
        uint64_t revision{};
        uint32_t incoming_packet_count{};
        uint32_t outgoing_packet_count{};

        PerfNetworkSnapshot();
    };

    class PerformanceNetworkMonitor
    {
    private:
        struct Record
        {
            ncl_perf::PerfTimestamp_t timestamp{};
            std::array<PerfNetworkValue, uint32_t(PerfNetworkSeries::Count)> values{};
        };

        template <class TValue>
        struct PacketHistory
        {
            std::array<TValue, kPerfNetworkPacketCapacity> values{};
            uint32_t next{};
            uint32_t count{};

            void Append(const TValue& value)
            {
                values[next] = value;
                next = (next + 1) % uint32_t(values.size());
                if (count < values.size())
                {
                    ++count;
                }
            }

            const TValue& get_value(uint32_t index) const
            {
                return values[(next + uint32_t(values.size()) - count + index) % values.size()];
            }
        };

        struct FramePacket
        {
            PerfNetworkValue latency;
            PerfNetworkValue loss;
            PerfNetworkValue choke;
        };

        struct Traffic
        {
            uint32_t current{};
            uint64_t bytes{};
            uint64_t packets{};
            bool initialized{};
            bool complete = true;
            PacketHistory<PerfNetworkValue> sizes;
        };

        std::array<Record, kPerfNetworkHistoryCapacity> records_{};
        std::array<Traffic, 2> traffic_{};
        PacketHistory<FramePacket> frame_packets_;
        std::array<uint32_t, kPerfNetworkFrameCapacity> parsed_sequences_{};
        uint32_t incoming_sequence_{};
        bool has_sequence_{};
        uint64_t revision_{};
        std::array<uint64_t, 2> sampled_bytes_{};
        std::array<uint64_t, 2> sampled_packets_{};
        std::array<PerfNetworkValue, uint32_t(PerfNetworkSeries::Count)> latest_{};
        ncl_perf::PerfTimestamp_t last_sample_{};
        uint32_t first_{};
        uint32_t count_{};
        uint32_t map_generation_{};
        float connection_time_{};
        double packet_age_ms_{};
        bool enabled_{};
        bool connected_{};
        bool demo_playback_{};
        bool has_sample_{};
        bool sequence_reset_{};

    public:
        void SetEnabled(bool enabled);
        bool is_enabled() const;
        void Reset();
        void ObserveTraffic(const goldsrc::netchan_t& channel);
        // Only normal messages initialize the current frame; assembled reliable fragments do not.
        void ObserveFrames(const goldsrc::client_static_t& client_static, const goldsrc::client_state_t& client_state, bool normal_message);
        void Update(
            ncl_perf::PerfTimestamp_t now,
            const goldsrc::client_static_t* client_static,
            const goldsrc::client_state_t* client_state,
            double realtime,
            uint32_t map_generation,
            bool normal_message = false
        );
        void BuildSnapshot(
            ncl_perf::PerfTimestamp_t end,
            ncl_perf::PerfDuration_t coverage,
            PerfNetworkSnapshot& output,
            uint32_t packet_count = kPerfGraphCapacity
        ) const;

    private:
        bool IsFrameChoked(const goldsrc::frame_t& frame, uint32_t sequence) const;
        void BuildPacketGraphs(PerfNetworkSnapshot& output) const;
    };

    class PerformanceNetGraphMask
    {
    private:
        goldsrc::cvar_t* cvar_{};
        std::string setting_;
        float value_{};

    public:
        PerformanceNetGraphMask(goldsrc::cvar_t* cvar, bool hide);
        ~PerformanceNetGraphMask();
        PerformanceNetGraphMask(const PerformanceNetGraphMask&) = delete;
        PerformanceNetGraphMask& operator=(const PerformanceNetGraphMask&) = delete;
    };
} // namespace performance
