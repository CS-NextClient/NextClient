#include <gtest/gtest.h>

#include <array>
#include <string>

#include <ncl_perf/PerfCpuAccounting.h>

namespace
{
    class TestClock : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t time{};
        uint32_t reads{};

        ncl_perf::PerfTimestamp_t Now() override
        {
            ++reads;
            return time;
        }
    };
} // namespace

TEST(CpuAccountingTest, NestedScopesKeepSelfTime)
{
    TestClock clock;
    ncl_perf::PerfCpuAccounting accounting(clock);
    accounting.RegisterScope({1, "parent", 0, ncl_perf::PerfScopeCapability::Cpu});
    accounting.RegisterScope({2, "child", 1, ncl_perf::PerfScopeCapability::Cpu});
    accounting.SetEnabled(true);
    clock.time = 100;
    ncl_perf::PerfScopeToken_t parent = accounting.Begin(1);
    clock.time = 200;
    ncl_perf::PerfScopeToken_t child = accounting.Begin(2);
    clock.time = 500;
    accounting.End(child);
    clock.time = 1100;
    accounting.End(parent);

    std::array<ncl_perf::PerfCpuSample, 2> samples;
    ASSERT_EQ(accounting.CopySamples(samples.data(), 2), 2u);
    EXPECT_EQ(samples[0].inclusive_ns, 1000u);
    EXPECT_EQ(samples[0].self_ns, 700u);
    EXPECT_EQ(samples[1].inclusive_ns, 300u);
    EXPECT_EQ(samples[1].self_ns, 300u);
    EXPECT_EQ(samples[0].calls, 1u);
}

TEST(CpuAccountingTest, DisabledScopesNeverReadClock)
{
    TestClock clock;
    ncl_perf::PerfCpuAccounting accounting(clock);
    accounting.RegisterScope({1, "scope", 0, ncl_perf::PerfScopeCapability::Cpu});
    EXPECT_EQ(accounting.Begin(1), 0u);
    accounting.End(0);
    EXPECT_EQ(clock.reads, 0u);
}

TEST(CpuAccountingTest, LastCompletedScopeKeepsIndividualDuration)
{
    TestClock clock;
    ncl_perf::PerfCpuAccounting accounting(clock);
    accounting.RegisterScope({1, "build", 0, ncl_perf::PerfScopeCapability::Cpu});
    accounting.SetEnabled(true);
    ncl_perf::PerfScopeToken_t first = accounting.Begin(1);
    clock.time = 100;
    accounting.End(first);
    ncl_perf::PerfScopeToken_t second = accounting.Begin(1);
    clock.time = 300;
    accounting.End(second);
    EXPECT_EQ(accounting.last_scope_id(), 1u);
    EXPECT_EQ(accounting.last_duration_ns(), 200u);
}

TEST(CpuAccountingTest, ResetRejectsOldToken)
{
    TestClock clock;
    ncl_perf::PerfCpuAccounting accounting(clock);
    accounting.RegisterScope({1, "scope", 0, ncl_perf::PerfScopeCapability::Cpu});
    accounting.Reset(1);
    accounting.SetEnabled(true);
    ncl_perf::PerfScopeToken_t token = accounting.Begin(1);
    accounting.Reset(2);
    clock.time = 400;
    accounting.End(token);
    ncl_perf::PerfCpuSample sample;
    ASSERT_EQ(accounting.CopySamples(&sample, 1), 1u);
    EXPECT_EQ(sample.inclusive_ns, 0u);
    EXPECT_EQ(sample.calls, 0u);
}

TEST(CpuAccountingTest, RegistryAndNameLimits)
{
    TestClock clock;
    ncl_perf::PerfCpuAccounting accounting(clock);
    std::string accepted(95, 'a');
    std::string rejected(96, 'b');
    EXPECT_EQ(accounting.RegisterScope({1, rejected.c_str(), 0, ncl_perf::PerfScopeCapability::Cpu}), 0u);
    ASSERT_EQ(accounting.RegisterScope({1, accepted.c_str(), 0, ncl_perf::PerfScopeCapability::Cpu}), 1u);
    for (uint32_t id = 2; id <= 128; ++id)
    {
        EXPECT_EQ(accounting.RegisterScope({id, "scope", 0, ncl_perf::PerfScopeCapability::Cpu}), id);
    }
    EXPECT_EQ(accounting.RegisterScope({129, "overflow", 0, ncl_perf::PerfScopeCapability::Cpu}), 0u);
    std::array<ncl_perf::PerfCpuSample, 128> samples;
    EXPECT_EQ(accounting.CopySamples(samples.data(), 128), 128u);
}

TEST(CpuAccountingTest, StackExhaustionPreservesClosure)
{
    TestClock clock;
    ncl_perf::PerfCpuAccounting accounting(clock);
    accounting.RegisterScope({1, "scope", 0, ncl_perf::PerfScopeCapability::Cpu});
    accounting.SetEnabled(true);
    std::array<ncl_perf::PerfScopeToken_t, 65> tokens;
    for (uint32_t depth = 0; depth < tokens.size(); ++depth)
    {
        clock.time = depth;
        tokens[depth] = accounting.Begin(1);
    }
    EXPECT_EQ(tokens[64], 0u);
    for (uint32_t depth = 65; depth > 0; --depth)
    {
        clock.time = 130 - depth;
        accounting.End(tokens[depth - 1]);
    }
    ncl_perf::PerfCpuSample sample;
    accounting.CopySamples(&sample, 1);
    EXPECT_EQ(sample.calls, 64u);
    EXPECT_EQ(sample.state, ncl_perf::PerfSampleState::Incomplete);
    EXPECT_EQ(sample.self_ns, 0u);
    accounting.ClearFrame();
    accounting.CopySamples(&sample, 1);
    EXPECT_EQ(sample.calls, 0u);
    EXPECT_EQ(sample.scope_id, 1u);
    clock.time = 200;
    ncl_perf::PerfScopeToken_t next = accounting.Begin(1);
    clock.time = 210;
    accounting.End(next);
    accounting.CopySamples(&sample, 1);
    EXPECT_EQ(sample.inclusive_ns, 10u);
    EXPECT_EQ(sample.state, ncl_perf::PerfSampleState::Measured);
}
TEST(CpuAccountingTest, ReportsActualParentAndSeparatesMultiParentTotals)
{
    TestClock clock;
    ncl_perf::PerfCpuAccounting accounting(clock);
    accounting.RegisterScope({1, "Host", 0, ncl_perf::PerfScopeCapability::Cpu});
    accounting.RegisterScope({2, "HUD", 1, ncl_perf::PerfScopeCapability::Cpu});
    accounting.RegisterScope({3, "Child", 2, ncl_perf::PerfScopeCapability::Cpu});
    accounting.SetEnabled(true);
    ncl_perf::PerfScopeToken_t host = accounting.Begin(1);
    ncl_perf::PerfScopeToken_t radar = accounting.Begin(3);
    clock.time = 10;
    accounting.End(radar);
    std::array<ncl_perf::PerfCpuSample, 3> samples;
    accounting.CopySamples(samples.data(), 3);
    EXPECT_EQ(samples[2].parent_id, 1u);
    ncl_perf::PerfScopeToken_t hud = accounting.Begin(2);
    radar = accounting.Begin(3);
    clock.time = 20;
    accounting.End(radar);
    accounting.End(hud);
    accounting.End(host);
    accounting.CopySamples(samples.data(), 3);
    EXPECT_EQ(samples[2].parent_id, 0u);
    EXPECT_EQ(samples[2].inclusive_ns, 20u);
    EXPECT_EQ(samples[0].self_ns, 0u);
    accounting.ClearFrame();
    hud = accounting.Begin(2);
    radar = accounting.Begin(3);
    accounting.End(radar);
    accounting.End(hud);
    accounting.CopySamples(samples.data(), 3);
    EXPECT_EQ(samples[2].parent_id, 2u);
}

namespace ncl_perf
{
    struct PerfCpuAccountingTestAccess
    {
        static void SeedSequence(ncl_perf::PerfCpuAccounting& accounting, uint32_t value)
        {
            accounting.sequence_ = value;
        }
    };
} // namespace ncl_perf


TEST(CpuAccountingTest, NewGenerationRenewsSequenceWithoutExhaustingLongSessions)
{
    EXPECT_EXIT(
        {
            TestClock clock;
            ncl_perf::PerfCpuAccounting accounting(clock);
            accounting.RegisterScope({1, "Host", 0, ncl_perf::PerfScopeCapability::Cpu});
            accounting.SetEnabled(true);
            ncl_perf::PerfCpuAccountingTestAccess::SeedSequence(accounting, UINT32_MAX);
            accounting.Reset(2);
            ncl_perf::PerfScopeToken_t token = accounting.Begin(1);
            clock.time = 10;
            accounting.End(token);
            ncl_perf::PerfCpuSample sample;
            accounting.CopySamples(&sample, 1);
            std::exit(sample.calls == 1 && sample.inclusive_ns == 10 ? 0 : 1);
        },
        testing::ExitedWithCode(0),
        ""
    );
}

TEST(CpuAccountingTest, SameGenerationResetKeepsTokenSequenceDistinct)
{
    TestClock clock;
    ncl_perf::PerfCpuAccounting accounting(clock);
    accounting.RegisterScope({1, "Host", 0, ncl_perf::PerfScopeCapability::Cpu});
    accounting.SetEnabled(true);
    ncl_perf::PerfScopeToken_t first = accounting.Begin(1);
    accounting.End(first);
    accounting.Reset(1);
    ncl_perf::PerfScopeToken_t second = accounting.Begin(1);
    EXPECT_NE(first, second);
    accounting.End(second);
}
