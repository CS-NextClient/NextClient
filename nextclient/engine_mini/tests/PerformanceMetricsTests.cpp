#include <algorithm>

#include <gtest/gtest.h>

#include "performance/PerformanceMetrics.h"
#include "performance/performance_stages.h"

namespace
{
    constexpr std::array<performance::StageDescriptor, 4> kSeparatedStages = {
        {{{4, "Ordinary", 0, ncl_perf::PerfScopeCapability::Cpu}},
         {{91, "Detail", 4, ncl_perf::PerfScopeCapability::Cpu},
          performance::CollectionLevel::StudioDetail,
          performance::PerfSeries::Count,
          false,
          true},
         {{5, "Neighbor", 4, ncl_perf::PerfScopeCapability::Cpu}},
         {{110, "Other", 4, ncl_perf::PerfScopeCapability::Cpu},
          performance::CollectionLevel::StudioDetail,
          performance::PerfSeries::Count,
          false,
          true,
          true}}
    };

    static_assert(performance::ValidateStages(kSeparatedStages));
    static_assert(performance::FindStage(kSeparatedStages, 91)->studio_detail);
    static_assert(!performance::FindStage(kSeparatedStages, 5)->studio_detail);
    static_assert(performance::FindStage(kSeparatedStages, 110)->studio_other_cost);
    constexpr std::array kPublicStageIds = {
        performance::PerfStage::Host,
        performance::PerfStage::Commands,
        performance::PerfStage::Client,
        performance::PerfStage::Packets,
        performance::PerfStage::Prediction,
        performance::PerfStage::Entities,
        performance::PerfStage::TempEntities,
        performance::PerfStage::Screen,
        performance::PerfStage::View,
        performance::PerfStage::World,
        performance::PerfStage::ViewModel,
        performance::PerfStage::Hud,
        performance::PerfStage::Studio,
        performance::PerfStage::StudioGeometry,
        performance::PerfStage::StudioMaterial,
        performance::PerfStage::StudioShader,
        performance::PerfStage::StudioStock,
        performance::PerfStage::ShaderCompile,
        performance::PerfStage::ShaderLink,
        performance::PerfStage::Radar,
        performance::PerfStage::RadarPrepare,
        performance::PerfStage::RadarDraw,
        performance::PerfStage::RadarLayout,
        performance::PerfStage::RadarCapture,
        performance::PerfStage::RadarLayer,
        performance::PerfStage::RadarPolicy,
        performance::PerfStage::RadarVisibility,
        performance::PerfStage::RadarBackdrop,
        performance::PerfStage::RadarObjectives,
        performance::PerfStage::RadarPlayers,
        performance::PerfStage::RadarText,
        performance::PerfStage::RadarBatch,
        performance::PerfStage::RadarBackdropCapture,
        performance::PerfStage::RadarMapDraw,
        performance::PerfStage::Sound,
        performance::PerfStage::SoundExtra,
        performance::PerfStage::Swap,
        performance::PerfStage::Monitor,
        performance::PerfStage::StudioGlSave,
        performance::PerfStage::StudioGlRestore,
        performance::PerfStage::StudioProgramBind,
        performance::PerfStage::StudioBufferBind,
        performance::PerfStage::StudioBoneUpload,
        performance::PerfStage::StudioModelStateCapture,
        performance::PerfStage::StudioBatchStateSave,
        performance::PerfStage::StudioRestoreBindings,
        performance::PerfStage::StudioRestoreMatrices,
        performance::PerfStage::StudioRestoreTextures,
        performance::PerfStage::StudioDataPrepare,
        performance::PerfStage::StudioMeshSort,
        performance::PerfStage::StudioStateApply,
        performance::PerfStage::StudioUniformSetup,
        performance::PerfStage::StudioTextureSetup,
        performance::PerfStage::StudioInstancePlan,
        performance::PerfStage::StudioInstanceUpload,
        performance::PerfStage::StudioInstanceSubmit
    };

    constexpr std::array kExpectedStudioCosts = {
        performance::PerfStage::StudioBoneUpload,
        performance::PerfStage::StudioBufferBind,
        performance::PerfStage::StudioGlRestore,
        performance::PerfStage::StudioGlSave,
        performance::PerfStage::StudioProgramBind
    };
    constexpr std::array kExpectedStudioDetails = {
        performance::PerfStage::StudioInstancePlan,
        performance::PerfStage::StudioInstanceUpload,
        performance::PerfStage::StudioInstanceSubmit,
        performance::PerfStage::StudioModelStateCapture,
        performance::PerfStage::StudioBatchStateSave,
        performance::PerfStage::StudioRestoreBindings,
        performance::PerfStage::StudioRestoreMatrices,
        performance::PerfStage::StudioRestoreTextures,
        performance::PerfStage::StudioDataPrepare,
        performance::PerfStage::StudioMeshSort,
        performance::PerfStage::StudioStateApply,
        performance::PerfStage::StudioUniformSetup,
        performance::PerfStage::StudioTextureSetup
    };
    constexpr std::array kExpectedStudioOtherCosts = {
        performance::PerfStage::StudioInstancePlan,
        performance::PerfStage::StudioInstanceUpload,
        performance::PerfStage::StudioDataPrepare,
        performance::PerfStage::StudioMeshSort,
        performance::PerfStage::StudioStateApply,
        performance::PerfStage::StudioUniformSetup,
        performance::PerfStage::StudioTextureSetup
    };
} // namespace

TEST(PerformanceMetricsTest, RegistersEveryStageWithCatalogCapabilitiesAndNames)
{
    class Clock : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t Now() override
        {
            return 1;
        }
    } clock;

    ncl_perf::PerfCpuAccounting accounting(clock);
    performance::stages::Register(accounting);
    std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity> samples;
    uint32_t count = accounting.CopySamples(samples.data(), uint32_t(samples.size()));
    EXPECT_EQ(count, performance::kStages.size());

    for (const performance::StageDescriptor& stage : performance::kStages)
    {
        SCOPED_TRACE(stage.scope.name);
        auto sample = std::find_if(samples.begin(), samples.begin() + count, [&stage](const ncl_perf::PerfCpuSample& value) {
            return value.scope_id == stage.scope.scope_id;
        });
        ASSERT_NE(sample, samples.begin() + count);
        EXPECT_STREQ(sample->name, stage.scope.name);
        EXPECT_EQ(sample->parent_id, stage.scope.parent_id);
        EXPECT_EQ(
            sample->state,
            performance::HasCapability(stage, ncl_perf::PerfScopeCapability::Cpu) ? ncl_perf::PerfSampleState::Inactive
                                                                                  : ncl_perf::PerfSampleState::Unavailable
        );
        EXPECT_STREQ(performance::stages::ScopeName(stage.scope.scope_id), stage.scope.name);
        EXPECT_EQ(
            performance::stages::HasCapability(stage.scope.scope_id, ncl_perf::PerfScopeCapability::Cpu),
            performance::HasCapability(stage, ncl_perf::PerfScopeCapability::Cpu)
        );
        EXPECT_EQ(
            performance::stages::HasCapability(stage.scope.scope_id, ncl_perf::PerfScopeCapability::Gpu),
            performance::HasCapability(stage, ncl_perf::PerfScopeCapability::Gpu)
        );
        EXPECT_EQ(performance::stages::GpuSeries(stage.scope.scope_id), stage.gpu_series);
    }
}

TEST(PerformanceMetricsTest, RejectsInvalidReferencesCapabilitiesDomainsOrdersAndCapacities)
{
    EXPECT_TRUE(performance::ValidateCatalog(performance::kStages, performance::kSeries, performance::kCounters));
    std::array stages = performance::kStages;
    stages[0].scope.scope_id = 0;
    EXPECT_FALSE(performance::ValidateCatalog(stages, performance::kSeries, performance::kCounters));
    stages = performance::kStages;
    auto leaf = std::find_if(stages.begin(), stages.end(), [](const performance::StageDescriptor& stage) {
        return stage.scope.scope_id == performance::PerfStage::StudioDataPrepare;
    });
    ASSERT_NE(leaf, stages.end());
    leaf->scope.scope_id = ncl_perf::kPerfScopeCapacity;
    EXPECT_FALSE(performance::ValidateCatalog(stages, performance::kSeries, performance::kCounters));
    stages = performance::kStages;
    stages[1].scope.scope_id = stages[0].scope.scope_id;
    EXPECT_FALSE(performance::ValidateCatalog(stages, performance::kSeries, performance::kCounters));
    stages = performance::kStages;
    stages[0].scope.parent_id = 127;
    EXPECT_FALSE(performance::ValidateCatalog(stages, performance::kSeries, performance::kCounters));
    stages = performance::kStages;
    stages[0].gpu_series = performance::PerfSeries::RenderGpu;
    EXPECT_FALSE(performance::ValidateCatalog(stages, performance::kSeries, performance::kCounters));

    std::array series = performance::kSeries;
    series[uint32_t(performance::PerfSeries::HostCpu)].scope_id = 127;
    EXPECT_FALSE(performance::ValidateCatalog(performance::kStages, series, performance::kCounters));
    series = performance::kSeries;
    series[uint32_t(performance::PerfSeries::HostCpu)].domain = performance::PerfRecordKind::Output;
    EXPECT_FALSE(performance::ValidateCatalog(performance::kStages, series, performance::kCounters));
    series = performance::kSeries;
    series[uint32_t(performance::PerfSeries::HostCpu)].presentation.order = 0;
    EXPECT_FALSE(performance::ValidateCatalog(performance::kStages, series, performance::kCounters));
    series = performance::kSeries;
    series[uint32_t(performance::PerfSeries::StudioCpu)].unit = performance::MetricUnit::Count;
    EXPECT_FALSE(performance::ValidateCatalog(performance::kStages, series, performance::kCounters));
    series = performance::kSeries;
    series[uint32_t(performance::PerfSeries::StudioCpu)].presentation.part_order = performance::kGroupPartCapacity;
    EXPECT_FALSE(performance::ValidateCatalog(performance::kStages, series, performance::kCounters));

    std::array counters = performance::kCounters;
    counters[0].id = counters[1].id;
    EXPECT_FALSE(performance::ValidateCatalog(performance::kStages, performance::kSeries, counters));
}

TEST(PerformanceMetricsTest, InvalidCallerIdsHaveNoDescriptorOrArrayAccess)
{
    EXPECT_EQ(performance::Stage(0), nullptr);
    EXPECT_EQ(performance::Stage(UINT32_MAX), nullptr);
    EXPECT_EQ(performance::Series(performance::PerfSeries(UINT32_MAX)), nullptr);
    EXPECT_EQ(performance::Counter(performance::PerfCounterId(UINT32_MAX)), nullptr);
    EXPECT_EQ(performance::stages::GpuSeries(performance::PerfStage::RadarPrepare), performance::PerfSeries::Count);
    EXPECT_EQ(performance::stages::GpuSeries(UINT32_MAX), performance::PerfSeries::Count);
    EXPECT_FALSE(performance::stages::HasCapability(UINT32_MAX, ncl_perf::PerfScopeCapability::Cpu));
    EXPECT_STREQ(performance::stages::SeriesName(performance::PerfSeries(UINT32_MAX)), "Unregistered");
    EXPECT_STREQ(performance::stages::CounterName(performance::PerfCounterId(UINT32_MAX)), "Unregistered");
}

TEST(PerformanceMetricsTest, StudioFlagsDoNotDependOnAdjacentNumericIds)
{
    EXPECT_EQ(performance::FindStage(kSeparatedStages, 91)->cpu_level, performance::CollectionLevel::StudioDetail);
    EXPECT_EQ(performance::FindStage(kSeparatedStages, 5)->cpu_level, performance::CollectionLevel::Ordinary);
    EXPECT_EQ(performance::CountStudioCosts(), 5u);
    EXPECT_TRUE(performance::Stage(performance::PerfStage::StudioBoneUpload)->studio_cost);
    EXPECT_FALSE(performance::Stage(performance::PerfStage::StudioBoneUpload)->studio_detail);
    EXPECT_TRUE(performance::Stage(performance::PerfStage::StudioModelStateCapture)->studio_detail);
    EXPECT_FALSE(performance::Stage(performance::PerfStage::StudioModelStateCapture)->studio_other_cost);
    EXPECT_TRUE(performance::Stage(performance::PerfStage::StudioTextureSetup)->studio_other_cost);
    EXPECT_FALSE(performance::Stage(performance::PerfStage::ShaderCompile)->studio_detail);
}

TEST(PerformanceMetricsTest, EveryPublicStageAndEnumIdHasExactlyOneDescriptor)
{
    EXPECT_EQ(kPublicStageIds.size(), performance::kStages.size());

    for (ncl_perf::PerfScopeId_t id : kPublicStageIds)
    {
        ASSERT_NE(performance::Stage(id), nullptr) << id;
    }

    for (uint32_t index = 0; index < uint32_t(performance::PerfSeries::Count); ++index)
    {
        ASSERT_NE(performance::Series(performance::PerfSeries(index)), nullptr) << index;
        EXPECT_STREQ(
            performance::stages::SeriesName(performance::PerfSeries(index)),
            performance::Series(performance::PerfSeries(index))->name
        );
    }

    for (uint32_t index = 0; index < uint32_t(performance::PerfCounterId::Count); ++index)
    {
        ASSERT_NE(performance::Counter(performance::PerfCounterId(index)), nullptr) << index;
        EXPECT_STREQ(
            performance::stages::CounterName(performance::PerfCounterId(index)),
            performance::Counter(performance::PerfCounterId(index))->name
        );
    }
}

TEST(PerformanceMetricsTest, StudioMembershipRetainsTheCuratedOperations)
{
    for (const performance::StageDescriptor& stage : performance::kStages)
    {
        SCOPED_TRACE(stage.scope.name);
        EXPECT_EQ(
            stage.studio_cost,
            std::find(kExpectedStudioCosts.begin(), kExpectedStudioCosts.end(), stage.scope.scope_id) != kExpectedStudioCosts.end()
        );
        EXPECT_EQ(
            stage.studio_detail,
            std::find(kExpectedStudioDetails.begin(), kExpectedStudioDetails.end(), stage.scope.scope_id) != kExpectedStudioDetails.end()
        );
        EXPECT_EQ(
            stage.studio_other_cost,
            std::find(kExpectedStudioOtherCosts.begin(), kExpectedStudioOtherCosts.end(), stage.scope.scope_id) !=
                kExpectedStudioOtherCosts.end()
        );
    }
}

TEST(PerformanceMetricsTest, BoundsStageCountParentDepthAndCycles)
{
    std::array<performance::StageDescriptor, 0> empty{};
    EXPECT_FALSE(performance::ValidateStages(empty));
    std::array<performance::StageDescriptor, ncl_perf::kPerfScopeCapacity + 1> oversized{};
    EXPECT_FALSE(performance::ValidateStages(oversized));
    std::array<performance::StageDescriptor, ncl_perf::kPerfStackCapacity + 1> chain{};

    for (uint32_t index = 0; index < chain.size(); ++index)
    {
        chain[index].scope = {index + 1, "Scope", index, ncl_perf::PerfScopeCapability::Cpu};
    }

    EXPECT_FALSE(performance::ValidateStages(chain));
    chain.back().scope.parent_id = 0;
    EXPECT_TRUE(performance::ValidateStages(chain));
    std::array cyclic = kSeparatedStages;
    cyclic[0].scope.parent_id = cyclic.back().scope.scope_id;
    EXPECT_FALSE(performance::ValidateStages(cyclic));
}

TEST(PerformanceMetricsTest, CatalogValidationAcceptsTableReorderingAndRejectsMissingEnumCoverage)
{
    std::array stages = performance::kStages;
    std::array series = performance::kSeries;
    std::array counters = performance::kCounters;
    std::reverse(stages.begin(), stages.end());
    std::reverse(series.begin(), series.end());
    std::reverse(counters.begin(), counters.end());
    EXPECT_TRUE(performance::ValidateCatalog(stages, series, counters));

    std::array<performance::SeriesDescriptor, 0> no_series{};
    std::array<performance::CounterDescriptor, 0> no_counters{};
    EXPECT_FALSE(performance::ValidateCatalog(stages, no_series, counters));
    EXPECT_FALSE(performance::ValidateCatalog(stages, series, no_counters));
}

TEST(PerformanceMetricsTest, FooterPresentationUsesDeclaredOrderAndExcludesHiddenSeries)
{
    std::array catalog = performance::kSeries;
    performance::SeriesPresentation& swap = catalog[uint32_t(performance::PerfSeries::SwapCpu)].presentation;
    swap.placement = performance::SeriesPlacement::Footer;
    swap.order = 1;
    swap.overview = false;
    swap.role = performance::GroupRole::None;
    EXPECT_TRUE(performance::ValidateCatalog(performance::kStages, catalog, performance::kCounters));

    for (uint32_t iteration = 0; iteration < 2; ++iteration)
    {
        EXPECT_EQ(performance::CountSeries(catalog, performance::SeriesPlacement::Footer), 2u);
        const performance::SeriesDescriptor* first = performance::OrderedSeries(catalog, performance::SeriesPlacement::Footer, 0);
        const performance::SeriesDescriptor* second = performance::OrderedSeries(catalog, performance::SeriesPlacement::Footer, 1);
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        EXPECT_EQ(first->id, performance::PerfSeries::MonitorCpu);
        EXPECT_EQ(second->id, performance::PerfSeries::SwapCpu);
        EXPECT_EQ(performance::OrderedSeries(catalog, performance::SeriesPlacement::Footer, 2), nullptr);
        std::reverse(catalog.begin(), catalog.end());
    }

    EXPECT_EQ(performance::CountSeries(catalog, performance::SeriesPlacement::Cpu), 4u);
    EXPECT_EQ(performance::CountSeries(catalog, performance::SeriesPlacement::Gpu), 3u);
    swap.order = 2;
    EXPECT_FALSE(performance::ValidateCatalog(performance::kStages, catalog, performance::kCounters));
    swap.placement = performance::SeriesPlacement::Hidden;
    EXPECT_TRUE(performance::ValidateCatalog(performance::kStages, catalog, performance::kCounters));
    EXPECT_EQ(performance::CountSeries(catalog, performance::SeriesPlacement::Footer), 1u);
    EXPECT_EQ(performance::CountSeries(catalog, performance::SeriesPlacement::Hidden), 0u);
    EXPECT_EQ(performance::OrderedSeries(catalog, performance::SeriesPlacement::Hidden, 2), nullptr);
}
