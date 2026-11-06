#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include <goldsrc/cvar.h>
#include <goldsrc/engine/client_state.h>

#include "performance/PerformanceNetwork.h"
#include "performance/performance_dump.h"

namespace
{
    constexpr ncl_perf::PerfTimestamp_t kStart = 1000000000;
    constexpr ncl_perf::PerfDuration_t kInterval = 100000000;

    class PerformanceNetworkTest : public testing::Test
    {
    protected:
        performance::PerformanceNetworkMonitor monitor;
        std::unique_ptr<goldsrc::client_static_t> cls = std::make_unique<goldsrc::client_static_t>();
        std::unique_ptr<goldsrc::client_state_t> client = std::make_unique<goldsrc::client_state_t>();
        std::unique_ptr<performance::PerfNetworkSnapshot> snapshot = std::make_unique<performance::PerfNetworkSnapshot>();

        void SetUp() override
        {
            cls->state = goldsrc::ca_active;
            cls->netchan.connect_time = 1;
            cls->netchan.last_received = 10;
            cls->netchan.incoming_sequence = 64;
            client->frames[0].receivedtime = 10;
            client->frames[0].latency = .035;
            monitor.SetEnabled(true);
        }

        void Update(ncl_perf::PerfTimestamp_t now)
        {
            monitor.Update(now, cls.get(), client.get(), 10, 1, true);
            monitor.BuildSnapshot(now, 0, *snapshot);
        }

        void Packet(uint32_t flow, int bytes)
        {
            goldsrc::flow_t& traffic = cls->netchan.flow[flow];
            traffic.stats[uint32_t(traffic.current) % 32] = {bytes, 10};
            traffic.current = int(uint32_t(traffic.current) + 1);
            monitor.ObserveTraffic(cls->netchan);
        }

        const performance::PerfNetworkValue& Value(performance::PerfNetworkSeries series)
        {
            return snapshot->latest[uint32_t(series)];
        }
    };
} // namespace

TEST_F(PerformanceNetworkTest, FirstIntervalIsPendingThenReportsByteAndPacketRates)
{
    Update(kStart);
    EXPECT_TRUE(snapshot->enabled);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).state, ncl_perf::PerfSampleState::Pending);
    Packet(1, 1024);
    Packet(1, 2048);
    Packet(0, 512);
    Update(kStart + kInterval);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).value, 30);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::OutgoingTraffic).value, 5);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingPackets).value, 20);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::OutgoingPackets).value, 10);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Latency).value, 35);
}

TEST_F(PerformanceNetworkTest, PerPacketObservationPreservesMoreThan32Packets)
{
    Update(kStart);
    for (int index = 0; index < 200; ++index)
    {
        Packet(1, 1024);
    }
    Update(kStart + kInterval);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).value, 2000);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingPackets).value, 2000);
}

TEST_F(PerformanceNetworkTest, MissingFlowEntriesProduceGapsAndRecoverNextInterval)
{
    Update(kStart);
    cls->netchan.flow[1].current = 40;
    monitor.ObserveTraffic(cls->netchan);
    Update(kStart + kInterval);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).state, ncl_perf::PerfSampleState::Dropped);
    Packet(1, 1024);
    Update(kStart + 2 * kInterval);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).value, 10);
}

TEST_F(PerformanceNetworkTest, UnsignedFlowCounterWrapPreservesOnePacket)
{
    cls->netchan.flow[0].current = -1;
    Update(kStart);
    Packet(0, 1024);
    Update(kStart + kInterval);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::OutgoingTraffic).state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::OutgoingTraffic).value, 10);
}

TEST_F(PerformanceNetworkTest, MissingOrInvalidLatencyIsNotZero)
{
    Update(kStart);
    ASSERT_EQ(Value(performance::PerfNetworkSeries::Latency).state, ncl_perf::PerfSampleState::Measured);
    for (double received : {0.0, -1.0, -3.0})
    {
        monitor.Reset();
        client->frames[0].receivedtime = received;
        Update(kStart);
        EXPECT_NE(Value(performance::PerfNetworkSeries::Latency).state, ncl_perf::PerfSampleState::Measured);
        EXPECT_EQ(snapshot->summaries[uint32_t(performance::PerfNetworkSeries::Latency)].count, 0u);
    }
    client->frames[0].receivedtime = 10;
    client->frames[0].invalid = true;
    monitor.Reset();
    Update(kStart);
    EXPECT_NE(Value(performance::PerfNetworkSeries::Latency).state, ncl_perf::PerfSampleState::Measured);
}

TEST_F(PerformanceNetworkTest, IdleConnectionReportsZeroTrafficAndPacketAge)
{
    Update(kStart);
    monitor.Update(kStart + kInterval, cls.get(), client.get(), 12, 1);
    monitor.BuildSnapshot(kStart + kInterval, 0, *snapshot);
    EXPECT_DOUBLE_EQ(snapshot->packet_age_ms, 2000);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).value, 0);
    EXPECT_NE(Value(performance::PerfNetworkSeries::Latency).state, ncl_perf::PerfSampleState::Measured);
}

TEST_F(PerformanceNetworkTest, LossAndChokeUseEngineStateAndRecentSequenceSlots)
{
    cls->packet_loss = 12.5f;
    client->frames[2].receivedtime = 9.8;
    client->frames[2].choked = true;
    client->frames[3].receivedtime = 9.9;
    client->frames[3].choked = true;
    Update(kStart);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Loss).value, 12.5);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 3.125);
}

TEST_F(PerformanceNetworkTest, DisconnectDemoAndDisabledStateClearHistory)
{
    Update(kStart);
    cls->state = goldsrc::ca_disconnected;
    Update(kStart + kInterval);
    EXPECT_FALSE(snapshot->connected);
    EXPECT_EQ(snapshot->count, 0u);
    cls->state = goldsrc::ca_active;
    cls->demoplayback = true;
    Update(kStart + 2 * kInterval);
    EXPECT_TRUE(snapshot->demo_playback);
    EXPECT_EQ(snapshot->count, 0u);
    monitor.SetEnabled(false);
    Update(kStart + 3 * kInterval);
    EXPECT_FALSE(snapshot->enabled);
    EXPECT_EQ(snapshot->count, 0u);
}

TEST_F(PerformanceNetworkTest, MapAndConnectionChangesDiscardOldRates)
{
    Update(kStart);
    Packet(1, 1024);
    Update(kStart + kInterval);
    cls->netchan.connect_time = 2;
    Update(kStart + 2 * kInterval);
    EXPECT_EQ(snapshot->count, 1u);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).state, ncl_perf::PerfSampleState::Pending);
    monitor.Update(kStart + 3 * kInterval, cls.get(), client.get(), 10, 2);
    monitor.BuildSnapshot(kStart + 3 * kInterval, 0, *snapshot);
    EXPECT_EQ(snapshot->count, 1u);
    EXPECT_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).state, ncl_perf::PerfSampleState::Pending);
}

TEST_F(PerformanceNetworkTest, RateHistoryIsBoundedWithoutChangingThePacketWindow)
{
    for (uint32_t index = 0; index <= 300; ++index)
    {
        client->frames[0].latency = .030 + (index % 2) * .010;
        Update(kStart + index * kInterval);
    }
    EXPECT_LE(snapshot->count, 101u);
    EXPECT_EQ(snapshot->coverage_ns, performance::kPerfHistoryDuration);
    EXPECT_NEAR(snapshot->summaries[0].median, 30, 1e-6);
    EXPECT_NEAR(snapshot->summaries[0].maximum, 40, 1e-6);

    monitor.BuildSnapshot(kStart + 300 * kInterval, 1000000000, *snapshot);
    EXPECT_EQ(snapshot->coverage_ns, 1000000000u);
    EXPECT_EQ(snapshot->count, 11u);
    EXPECT_EQ(snapshot->graphs[0][230].count, 0u);
    EXPECT_EQ(snapshot->graphs[0][255].count, 1u);
    EXPECT_NEAR(snapshot->graphs[0][255].mean, 30, 1e-6);
    EXPECT_EQ(snapshot->packet_summaries[0].count, 1u);
}

TEST_F(PerformanceNetworkTest, IdleTimeDoesNotMoveThePacketWindow)
{
    Update(kStart);
    Update(kStart + 1000000000);
    EXPECT_EQ(snapshot->graphs[0][230].count, 0u);
    EXPECT_EQ(snapshot->graphs[0][231].count, 0u);
    EXPECT_EQ(snapshot->graphs[0][0].count, 0u);
    EXPECT_EQ(snapshot->graphs[0].back().count, 1u);
    EXPECT_EQ(snapshot->coverage_ns, 1000000000u);
}

TEST(PerformanceNetGraphTest, MaskRestoresNumericValueWithoutChangingPreference)
{
    char setting[] = "3";
    goldsrc::cvar_t cvar{"net_graph", setting, 1, 3};
    {
        performance::PerformanceNetGraphMask mask(&cvar, true);
        EXPECT_FLOAT_EQ(cvar.value, 0);
        EXPECT_EQ(cvar.string, setting);
        EXPECT_STREQ(cvar.string, "3");
    }
    EXPECT_FLOAT_EQ(cvar.value, 3);
    EXPECT_EQ(cvar.flags, 1);
}

TEST(PerformanceNetGraphTest, ChangesMadeDuringDrawingRemainEffective)
{
    char setting[] = "3";
    char new_setting[] = "0";
    goldsrc::cvar_t cvar{"net_graph", setting, 0, 3};
    {
        performance::PerformanceNetGraphMask mask(&cvar, true);
        cvar.value = 1;
    }
    EXPECT_FLOAT_EQ(cvar.value, 1);
    {
        performance::PerformanceNetGraphMask mask(&cvar, true);
        cvar.string = new_setting;
    }
    EXPECT_FLOAT_EQ(cvar.value, 0);
    performance::PerformanceNetGraphMask mask(&cvar, false);
    EXPECT_FLOAT_EQ(cvar.value, 0);
}

TEST(PerformanceNetGraphTest, ReusedStringStorageKeepsTheNewPreference)
{
    char setting[] = "3";
    goldsrc::cvar_t cvar{"net_graph", setting, 0, 3};
    {
        performance::PerformanceNetGraphMask mask(&cvar, true);
        setting[0] = '0';
    }
    EXPECT_FLOAT_EQ(cvar.value, 0);
    EXPECT_STREQ(cvar.string, "0");
}

TEST_F(PerformanceNetworkTest, RateIntervalDoesNotReplicatePacketSizeColumns)
{
    Update(kStart);
    Packet(1, 1024);
    Update(kStart + kInterval);
    monitor.BuildSnapshot(kStart + kInterval, 1000000000, *snapshot);
    const auto& traffic = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    EXPECT_EQ(traffic[254].count, 0u);
    EXPECT_EQ(traffic[255].count, 1u);
    EXPECT_DOUBLE_EQ(traffic[255].mean, 1024);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).value, 10);
}

TEST_F(PerformanceNetworkTest, EachTrafficPacketKeepsItsOwnColumn)
{
    Update(kStart);
    Packet(1, 128);
    Packet(1, 4096);
    Packet(0, 512);
    Update(kStart + kInterval);
    const auto& incoming = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    const auto& outgoing = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::OutgoingTraffic)];
    EXPECT_EQ(incoming[253].count, 0u);
    EXPECT_DOUBLE_EQ(incoming[254].maximum, 128);
    EXPECT_DOUBLE_EQ(incoming[255].maximum, 4096);
    EXPECT_DOUBLE_EQ(incoming[255].mean, 4096);
    EXPECT_EQ(incoming[255].count, 1u);
    EXPECT_DOUBLE_EQ(outgoing[255].maximum, 512);
    EXPECT_EQ(outgoing[254].count, 0u);
}

TEST_F(PerformanceNetworkTest, LatencyRecordsEveryIncomingSequenceBeforeTheRateIntervalEnds)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 65;
    client->frames[1].receivedtime = 10.001;
    client->frames[1].latency = .050;
    Update(kStart + 1000000);
    const auto& latency = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)];
    EXPECT_NEAR(latency[254].maximum, 35, 1e-6);
    EXPECT_NEAR(latency[255].maximum, 50, 1e-6);
    EXPECT_EQ(latency[253].count, 0u);
}

TEST_F(PerformanceNetworkTest, IdlePresentationsDoNotDuplicatePackets)
{
    Update(kStart);
    Packet(1, 1024);
    Update(kStart + kInterval);
    Update(kStart + 2 * kInterval);
    const auto& incoming = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    const auto& latency = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)];
    uint32_t incoming_count = 0;
    uint32_t latency_count = 0;
    for (uint32_t index = 0; index < performance::kPerfGraphCapacity; ++index)
    {
        incoming_count += incoming[index].count;
        latency_count += latency[index].count;
    }
    EXPECT_EQ(incoming_count, 1u);
    EXPECT_EQ(latency_count, 1u);
}

TEST_F(PerformanceNetworkTest, MissingAndChokedSequencesHaveIndividualMarkers)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 66;
    client->frames[1].receivedtime = -1;
    client->frames[2].receivedtime = -1;
    client->frames[2].choked = true;
    Update(kStart + kInterval);
    cls->netchan.incoming_sequence = 67;
    client->frames[3].receivedtime = 10.003;
    client->frames[3].latency = .040;
    Update(kStart + kInterval + 1000000);
    const auto& loss = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Loss)];
    const auto& choke = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)];
    EXPECT_DOUBLE_EQ(loss[253].maximum, 100);
    EXPECT_DOUBLE_EQ(choke[254].maximum, 100);
    EXPECT_DOUBLE_EQ(loss[254].maximum, 0);
    EXPECT_DOUBLE_EQ(loss[255].maximum, 0);
    EXPECT_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)][253].count, 0u);
    EXPECT_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)][254].count, 0u);
    EXPECT_NEAR(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)][255].maximum, 40, 1e-6);
}

TEST_F(PerformanceNetworkTest, EachDirectionRetainsItsLast256PacketsAndPacketStatistics)
{
    Update(kStart);
    for (int bytes = 1; bytes <= 300; ++bytes)
    {
        Packet(1, bytes);
    }
    Packet(0, 1024);
    Update(kStart + kInterval);
    EXPECT_EQ(snapshot->incoming_packet_count, 256u);
    EXPECT_EQ(snapshot->outgoing_packet_count, 1u);
    const auto& incoming = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    EXPECT_DOUBLE_EQ(incoming.front().maximum, 45);
    EXPECT_DOUBLE_EQ(incoming.back().maximum, 300);
    const performance::PerfNetworkSummary& sizes = snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    EXPECT_EQ(sizes.count, 256u);
    EXPECT_DOUBLE_EQ(sizes.mean, 172.5);
    EXPECT_DOUBLE_EQ(sizes.median, 172.5);
    EXPECT_DOUBLE_EQ(sizes.maximum, 300);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::OutgoingTraffic)].back().maximum, 1024);
}

TEST_F(PerformanceNetworkTest, OverwrittenFlowEntriesKeepTheirOwnGaps)
{
    Update(kStart);
    for (goldsrc::flowstats_t& entry : cls->netchan.flow[1].stats)
    {
        entry.size = 512;
    }
    cls->netchan.flow[1].current = 300;
    monitor.ObserveTraffic(cls->netchan);
    monitor.BuildSnapshot(kStart, 0, *snapshot);
    const auto& incoming = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    EXPECT_EQ(snapshot->incoming_packet_count, 256u);
    EXPECT_EQ(incoming[223].count, 0u);
    EXPECT_EQ(incoming[224].count, 1u);
    EXPECT_EQ(incoming[255].count, 1u);
    EXPECT_DOUBLE_EQ(incoming[255].maximum, 512);
    EXPECT_EQ(snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)].count, 32u);
}

TEST_F(PerformanceNetworkTest, IncomingSequenceHistoryWrapsWithoutReplayingOldSlots)
{
    Update(kStart);
    for (uint32_t index = 1; index <= 300; ++index)
    {
        cls->netchan.incoming_sequence = 64 + index;
        goldsrc::frame_t& frame = client->frames[(64 + index) % 64];
        frame.receivedtime = 10 + double(index) / 1000;
        frame.latency = double(index) / 1000;
        monitor.ObserveFrames(*cls, *client, true);
    }
    monitor.BuildSnapshot(kStart, 0, *snapshot);
    const auto& latency = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)];
    EXPECT_NEAR(latency.front().maximum, 45, 1e-6);
    EXPECT_NEAR(latency.back().maximum, 300, 1e-6);
    EXPECT_EQ(snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::Latency)].count, 256u);
}

TEST_F(PerformanceNetworkTest, FrameSequenceIsObservedOnlyAfterParsing)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 65;
    monitor.Update(kStart + kInterval, cls.get(), client.get(), 10, 1);
    monitor.BuildSnapshot(kStart + kInterval, 0, *snapshot);
    EXPECT_NEAR(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)].back().maximum, 35, 1e-6);
    client->frames[1].receivedtime = 10.1;
    client->frames[1].latency = .045;
    monitor.ObserveFrames(*cls, *client, true);
    monitor.BuildSnapshot(kStart + kInterval, 0, *snapshot);
    EXPECT_NEAR(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)].back().maximum, 45, 1e-6);
    EXPECT_NEAR(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)][254].maximum, 35, 1e-6);
}

TEST_F(PerformanceNetworkTest, RevisionsTrackPacketsAndResetsWithoutIdleRefreshes)
{
    Update(kStart);
    uint64_t initial = snapshot->revision;
    Update(kStart + 1);
    EXPECT_EQ(snapshot->revision, initial);
    Packet(1, 512);
    Update(kStart + 2);
    EXPECT_GT(snapshot->revision, initial);
    uint64_t packet_revision = snapshot->revision;
    cls->state = goldsrc::ca_disconnected;
    Update(kStart + 3);
    EXPECT_GT(snapshot->revision, packet_revision);
    EXPECT_EQ(snapshot->incoming_packet_count, 0u);
    EXPECT_EQ(snapshot->packet_summaries[0].count, 0u);
    uint64_t disconnected = snapshot->revision;
    Update(kStart + 4);
    EXPECT_EQ(snapshot->revision, disconnected);
}

TEST_F(PerformanceNetworkTest, DumpSeparatesPacketSizesFromSampledRates)
{
    Update(kStart);
    Packet(1, 1024);
    Update(kStart + kInterval);
    performance::PerfSnapshot performance;
    std::string dump(performance::dump::Format(performance, nullptr, 0, snapshot.get()), '\0');
    performance::dump::Format(performance, dump.data(), uint32_t(dump.size()), snapshot.get());
    EXPECT_NE(dump.find("Network Incoming KiB/s: Measured latest=10.000"), std::string::npos);
    EXPECT_NE(dump.find("Network packet window: capacity=256 incoming=1 outgoing=0"), std::string::npos);
    EXPECT_NE(dump.find("Network packets Incoming B: count=1 avg=1024.000 median=1024.000 max=1024.000"), std::string::npos);
}

TEST_F(PerformanceNetworkTest, ReliableFragmentDoesNotRecordAnUninitializedFrame)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 65;
    client->frames[1].receivedtime = 9;
    client->frames[1].latency = .125;
    monitor.ObserveFrames(*cls, *client, false);
    monitor.BuildSnapshot(kStart, 0, *snapshot);
    const auto& latency = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)];
    EXPECT_EQ(latency[254].count, 0u);
    EXPECT_NEAR(latency.back().maximum, 35, 1e-6);
    client->frames[1].receivedtime = -1;
    cls->netchan.incoming_sequence = 66;
    client->frames[2].receivedtime = 10.1;
    client->frames[2].latency = .040;
    monitor.ObserveFrames(*cls, *client, true);
    monitor.BuildSnapshot(kStart, 0, *snapshot);
    EXPECT_EQ(latency[254].count, 0u);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Loss)][254].maximum, 100);
    EXPECT_NEAR(latency.back().maximum, 40, 1e-6);
}

TEST_F(PerformanceNetworkTest, MissingSequenceDoesNotInheritAnOldChokeFlag)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 66;
    client->frames[1].receivedtime = -1;
    client->frames[1].choked = true;
    client->frames[2].receivedtime = 10.1;
    client->frames[2].latency = .040;
    monitor.ObserveFrames(*cls, *client, true);
    monitor.BuildSnapshot(kStart, 0, *snapshot);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Loss)][254].maximum, 100);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)][254].maximum, 0);

    Update(kStart + kInterval);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 0);
    EXPECT_DOUBLE_EQ(snapshot->summaries[uint32_t(performance::PerfNetworkSeries::Choke)].mean, 0);
    Update(kStart + 2 * kInterval);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 0);
    EXPECT_DOUBLE_EQ(snapshot->summaries[uint32_t(performance::PerfNetworkSeries::Choke)].maximum, 0);
}

TEST_F(PerformanceNetworkTest, CurrentNormalMessageKeepsItsFreshChokeFlag)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 65;
    client->frames[1].receivedtime = -1;
    client->frames[1].choked = true;
    monitor.ObserveFrames(*cls, *client, true);
    monitor.BuildSnapshot(kStart, 0, *snapshot);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Loss)].back().maximum, 0);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)].back().maximum, 100);
    EXPECT_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)].back().count, 0u);
}

TEST_F(PerformanceNetworkTest, FirstNormalChokeIsClassifiedBeforeTheInitialSummary)
{
    cls->netchan.incoming_sequence = 1;
    client->frames[1].receivedtime = -1;
    client->frames[1].choked = true;
    Update(kStart);

    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 100);
    EXPECT_DOUBLE_EQ(snapshot->summaries[uint32_t(performance::PerfNetworkSeries::Choke)].mean, 100);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)].back().maximum, 100);

    monitor.Update(kStart + kInterval, cls.get(), client.get(), 10, 1);
    monitor.BuildSnapshot(kStart + kInterval, 0, *snapshot);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 100);
}

TEST_F(PerformanceNetworkTest, ReliableFragmentDoesNotConfirmAStaleChokeInSummaries)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 65;
    client->frames[1].receivedtime = -1;
    client->frames[1].choked = true;
    monitor.ObserveFrames(*cls, *client, false);
    monitor.Update(kStart + kInterval, cls.get(), client.get(), 10, 1);
    monitor.BuildSnapshot(kStart + kInterval, 0, *snapshot);

    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 0);
    EXPECT_DOUBLE_EQ(snapshot->summaries[uint32_t(performance::PerfNetworkSeries::Choke)].maximum, 0);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)].back().maximum, 0);
}

TEST_F(PerformanceNetworkTest, FreshChokeRemainsInSummariesUntilItsSequenceLeavesTheWindow)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 65;
    client->frames[1].receivedtime = -1;
    client->frames[1].choked = true;
    monitor.ObserveFrames(*cls, *client, true);
    Update(kStart + kInterval);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 1.5625);

    cls->netchan.incoming_sequence = 66;
    client->frames[2].receivedtime = 10.2;
    monitor.ObserveFrames(*cls, *client, true);
    Update(kStart + 2 * kInterval);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 1.5625);
    EXPECT_DOUBLE_EQ(snapshot->summaries[uint32_t(performance::PerfNetworkSeries::Choke)].maximum, 1.5625);
    EXPECT_DOUBLE_EQ(snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::Choke)].mean, 100.0 / 3);

    cls->netchan.incoming_sequence = 130;
    client->frames[2].receivedtime = 10.3;
    monitor.ObserveFrames(*cls, *client, true);
    Update(kStart + 3 * kInterval);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 0);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)][254].maximum, 0);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Loss)][254].maximum, 100);
}

TEST_F(PerformanceNetworkTest, ResetDiscardsObservedChokesWhenTheirSlotsAreMissing)
{
    Update(kStart);
    cls->netchan.incoming_sequence = 65;
    client->frames[1].receivedtime = -1;
    client->frames[1].choked = true;
    monitor.ObserveFrames(*cls, *client, true);
    Update(kStart + kInterval);
    ASSERT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 1.5625);

    monitor.Reset();
    cls->netchan.incoming_sequence = 64;
    Update(kStart + 2 * kInterval);
    cls->netchan.incoming_sequence = 66;
    client->frames[2].receivedtime = 10.2;
    monitor.ObserveFrames(*cls, *client, true);
    Update(kStart + 3 * kInterval);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::Choke).value, 0);
    EXPECT_DOUBLE_EQ(snapshot->summaries[uint32_t(performance::PerfNetworkSeries::Choke)].maximum, 0);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)][254].maximum, 0);
}

TEST_F(PerformanceNetworkTest, PacketWindowUsesAllRequestedColumns)
{
    Update(kStart);
    for (uint32_t index = 1; index <= 450; ++index)
    {
        Packet(1, int(index));
        cls->netchan.incoming_sequence = 64 + index;
        goldsrc::frame_t& frame = client->frames[(64 + index) % 64];
        frame.receivedtime = 10 + double(index) / 1000;
        frame.latency = double(index) / 1000;
        monitor.ObserveFrames(*cls, *client, true);
    }
    Packet(0, 128);
    Packet(0, 256);
    monitor.BuildSnapshot(kStart, 0, *snapshot, 400);

    for (const std::vector<performance::PerfPlotBin>& graph : snapshot->graphs)
    {
        ASSERT_EQ(graph.size(), 400u);
    }
    const std::vector<performance::PerfPlotBin>& incoming = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    const std::vector<performance::PerfPlotBin>& outgoing = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::OutgoingTraffic)];
    const std::vector<performance::PerfPlotBin>& latency = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)];
    EXPECT_EQ(snapshot->incoming_packet_count, 400u);
    EXPECT_EQ(snapshot->outgoing_packet_count, 2u);
    EXPECT_DOUBLE_EQ(incoming.front().maximum, 51);
    EXPECT_DOUBLE_EQ(incoming.back().maximum, 450);
    EXPECT_NEAR(latency.front().maximum, 51, 1e-6);
    EXPECT_NEAR(latency.back().maximum, 450, 1e-6);
    EXPECT_EQ(outgoing[397].count, 0u);
    EXPECT_DOUBLE_EQ(outgoing[398].maximum, 128);
    EXPECT_DOUBLE_EQ(outgoing[399].maximum, 256);
}

TEST_F(PerformanceNetworkTest, ExpandingPacketWindowRecoversRetainedOlderPackets)
{
    Update(kStart);
    for (int bytes = 1; bytes <= 600; ++bytes)
    {
        Packet(1, bytes);
    }
    monitor.BuildSnapshot(kStart, 0, *snapshot, 200);
    const std::vector<performance::PerfPlotBin>& incoming = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    ASSERT_EQ(incoming.size(), 200u);
    EXPECT_DOUBLE_EQ(incoming.front().maximum, 401);
    EXPECT_DOUBLE_EQ(incoming.back().maximum, 600);
    const performance::PerfPlotBin* data = incoming.data();
    uint64_t revision = snapshot->revision;
    monitor.BuildSnapshot(kStart, 0, *snapshot, 200);
    EXPECT_EQ(incoming.data(), data);
    EXPECT_EQ(snapshot->revision, revision);

    monitor.BuildSnapshot(kStart, 0, *snapshot, 600);
    ASSERT_EQ(incoming.size(), 600u);
    EXPECT_EQ(snapshot->incoming_packet_count, 600u);
    EXPECT_DOUBLE_EQ(incoming.front().maximum, 1);
    EXPECT_DOUBLE_EQ(incoming.back().maximum, 600);
    EXPECT_EQ(snapshot->revision, revision);
}

TEST_F(PerformanceNetworkTest, PacketWindowStatisticsUseOnlyVisiblePacketPositions)
{
    Update(kStart);
    for (uint32_t index = 1; index <= 10; ++index)
    {
        Packet(1, int(index));
        if (index == 8)
        {
            client->frames[8].receivedtime = -1;
            client->frames[8].choked = true;
            continue;
        }
        cls->netchan.incoming_sequence = 64 + index;
        goldsrc::frame_t& frame = client->frames[(64 + index) % 64];
        frame.receivedtime = 10 + double(index) / 1000;
        frame.latency = double(index) / 1000;
        frame.choked = index == 10;
        monitor.ObserveFrames(*cls, *client, true);
    }
    Packet(0, 512);
    Packet(0, 1024);
    monitor.Update(kStart + kInterval, cls.get(), client.get(), 10, 1);
    monitor.BuildSnapshot(kStart + kInterval, 0, *snapshot, 3);

    for (const std::vector<performance::PerfPlotBin>& graph : snapshot->graphs)
    {
        ASSERT_EQ(graph.size(), 3u);
    }
    const performance::PerfNetworkSummary& sizes = snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    const performance::PerfNetworkSummary& latency = snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::Latency)];
    EXPECT_EQ(snapshot->incoming_packet_count, 3u);
    EXPECT_EQ(snapshot->outgoing_packet_count, 2u);
    EXPECT_EQ(sizes.count, 3u);
    EXPECT_DOUBLE_EQ(sizes.mean, 9);
    EXPECT_DOUBLE_EQ(sizes.median, 9);
    EXPECT_DOUBLE_EQ(sizes.maximum, 10);
    EXPECT_EQ(latency.count, 1u);
    EXPECT_NEAR(latency.mean, 9, 1e-6);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Loss)][0].maximum, 100);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)][0].maximum, 0);
    EXPECT_DOUBLE_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Choke)][2].maximum, 100);
    EXPECT_EQ(snapshot->graphs[uint32_t(performance::PerfNetworkSeries::OutgoingTraffic)][0].count, 0u);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingPackets).value, 100);
    EXPECT_DOUBLE_EQ(Value(performance::PerfNetworkSeries::IncomingTraffic).value, 55.0 / .1 / 1024);
}

TEST_F(PerformanceNetworkTest, PacketWindowCapsRetainedHistoryAndAllowsAnEmptyView)
{
    Update(kStart);
    for (uint32_t index = 1; index <= 5000; ++index)
    {
        Packet(1, int(index));
        cls->netchan.incoming_sequence = 64 + index;
        goldsrc::frame_t& frame = client->frames[(64 + index) % 64];
        frame.receivedtime = 10 + double(index) / 1000;
        frame.latency = double(index) / 1000;
        monitor.ObserveFrames(*cls, *client, true);
    }
    monitor.BuildSnapshot(kStart, 0, *snapshot, 9999);

    for (const std::vector<performance::PerfPlotBin>& graph : snapshot->graphs)
    {
        ASSERT_EQ(graph.size(), 4096u);
    }
    const std::vector<performance::PerfPlotBin>& incoming = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)];
    const std::vector<performance::PerfPlotBin>& latency = snapshot->graphs[uint32_t(performance::PerfNetworkSeries::Latency)];
    EXPECT_EQ(snapshot->incoming_packet_count, 4096u);
    EXPECT_DOUBLE_EQ(incoming.front().maximum, 905);
    EXPECT_DOUBLE_EQ(incoming.back().maximum, 5000);
    EXPECT_NEAR(latency.front().maximum, 905, 1e-6);
    EXPECT_NEAR(latency.back().maximum, 5000, 1e-6);
    EXPECT_EQ(snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)].count, 4096u);
    EXPECT_DOUBLE_EQ(snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)].mean, 2952.5);

    monitor.BuildSnapshot(kStart, 0, *snapshot, 0);
    for (const std::vector<performance::PerfPlotBin>& graph : snapshot->graphs)
    {
        EXPECT_TRUE(graph.empty());
    }
    EXPECT_EQ(snapshot->incoming_packet_count, 0u);
    EXPECT_EQ(snapshot->packet_summaries[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)].count, 0u);
    monitor.BuildSnapshot(kStart, 0, *snapshot, 4096);
    ASSERT_EQ(incoming.size(), 4096u);
    EXPECT_DOUBLE_EQ(incoming.front().maximum, 905);
    EXPECT_DOUBLE_EQ(incoming.back().maximum, 5000);
}
