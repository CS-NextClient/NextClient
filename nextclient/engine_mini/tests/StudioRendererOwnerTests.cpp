#include <gtest/gtest.h>

#include "RendererOwnerTestSupport.h"
#include "graphics/render/StudioPreviewContext.h"

namespace
{
    int g_FirstStockDraws{};
    int g_SecondStockDraws{};
    void FirstStockDraw()
    {
        ++g_FirstStockDraws;
    }
    void SecondStockDraw()
    {
        ++g_SecondStockDraws;
    }
    void InterceptDraw() {}
    void SetHeader(void*) {}
} // namespace

TEST(StudioRendererOwner, IndependentRendererInputsAndPreviewStacksUseTheirInjectedOwners)
{
    renderer_test::Owners first;
    renderer_test::Owners second;
    goldsrc::cvar_t off{};
    goldsrc::cvar_t on{};
    on.value = 1;
    render::RenderConfig first_config{};
    render::RenderConfig second_config{};
    first_config.studio_shader = &off;
    second_config.studio_shader = &on;
    first.state.studio_engine.config = &first_config;
    second.state.studio_engine.config = &second_config;
    uint32_t serial = 1;
    render::StudioModelInputs inputs{nullptr, nullptr, nullptr, nullptr, nullptr, {}, serial, serial};
    EXPECT_FALSE(first.state.studio.TryDraw(inputs));
    EXPECT_FALSE(second.state.studio.TryDraw(inputs));
    EXPECT_STREQ(first.state.studio.get_last_rejection(), "cvar is off");
    EXPECT_STREQ(second.state.studio.get_last_rejection(), "shader pipeline unavailable, needs GL 3.3");
    goldsrc::cl_entity_t entity{};
    PreviewDrawParams params{};
    PreviewModel model{};
    {
        render::StudioPreviewContext context(first.state.studio, &entity, params, model);
        EXPECT_NE(first.state.studio.PreviewInputs(&entity), nullptr);
        EXPECT_EQ(second.state.studio.PreviewInputs(&entity), nullptr);
    }
    EXPECT_EQ(first.state.studio.PreviewInputs(&entity), nullptr);
}

TEST(StudioRendererOwner, IndependentInterceptorsOwnEpochLightingAndStockForwarding)
{
    renderer_test::Owners first;
    renderer_test::Owners second;
    goldsrc::engine_studio_api_t first_api{};
    goldsrc::engine_studio_api_t second_api{};
    first_api.StudioDrawPoints = FirstStockDraw;
    second_api.StudioDrawPoints = SecondStockDraw;
    first_api.StudioSetHeader = second_api.StudioSetHeader = SetHeader;
    render::StudioInterceptCallbacks callbacks;
    callbacks.studio.StudioDrawPoints = InterceptDraw;
    first.state.intercept.PatchApi(&first_api, nullptr, callbacks);
    second.state.intercept.PatchApi(&second_api, nullptr, callbacks);
    const uint32_t first_serial = first.state.intercept.get_model_draw_serial();
    const uint32_t second_serial = second.state.intercept.get_model_draw_serial();
    render::StudioModelInputs captured = first.state.intercept.CaptureInputs(nullptr, nullptr, nullptr);
    first.state.intercept.StudioSetHeader(nullptr);
    EXPECT_GT(captured.live_serial, captured.serial);
    EXPECT_GT(first.state.intercept.get_model_draw_serial(), first_serial);
    EXPECT_EQ(second.state.intercept.get_model_draw_serial(), second_serial);
    EXPECT_FALSE(first.state.studio.TryDraw(captured));
    EXPECT_STREQ(first.state.studio.get_last_rejection(), "studio model was invalidated before preparation");
    first.state.intercept.Shutdown();
    second.state.intercept.Shutdown();
    g_FirstStockDraws = g_SecondStockDraws = 0;
    first.state.intercept.StudioDrawPoints();
    EXPECT_EQ(g_FirstStockDraws, 1);
    EXPECT_EQ(g_SecondStockDraws, 0);
    second.state.intercept.StudioDrawPoints();
    EXPECT_EQ(g_SecondStockDraws, 1);
    EXPECT_EQ(first_api.StudioDrawPoints, FirstStockDraw);
    EXPECT_EQ(second_api.StudioDrawPoints, SecondStockDraw);
}

TEST(StudioRendererOwner, DiagnosticFramesAndOwnedRecordsRemainIndependent)
{
    render::StudioEngineBindings first_engine;
    render::StudioEngineBindings second_engine;
    render::StudioDiagnostics first(first_engine);
    render::StudioDiagnostics second(second_engine);
    first.Arm("first", true);
    second.Arm("second", true);
    std::string model = "models/owned.mdl";
    render::StudioSubmission submission{};
    render::StudioOutcome outcome{model.c_str(), "header", "reason", false};
    first.Note(submission, outcome);
    model = "changed";
    EXPECT_EQ(render::StudioDiagnosticsTestAccess::Count(first), 1);
    EXPECT_EQ(render::StudioDiagnosticsTestAccess::Count(second), 0);
    EXPECT_STREQ(render::StudioDiagnosticsTestAccess::Model(first, 0), "models/owned.mdl");
    first.OnSceneEnd("unused", 0);
    EXPECT_TRUE(first.is_armed());
    EXPECT_EQ(render::StudioDiagnosticsTestAccess::Count(first), 0);
    first.Clear();
    EXPECT_FALSE(first.needs_scene_end());
    EXPECT_TRUE(second.is_armed());
    second.Clear();
}

TEST(StudioRendererOwner, InterleavedStudioScopesMeasureEachOwnerAndKeepOneOuterSample)
{
    renderer_test::Owners first;
    renderer_test::Owners second;
    for (performance::PerformanceMonitor* monitor : {&first.performance.monitor, &second.performance.monitor})
    {
        monitor->SetMode(1);
        monitor->OnHostEntry();
        monitor->OnFrameAccepted();
    }
    {
        render::StudioCpuScope first_outer(first.state.studio.get_monitor(), first.state.studio.get_studio_depth());
        render::StudioCpuScope second_outer(second.state.studio.get_monitor(), second.state.studio.get_studio_depth());
        render::StudioCpuScope first_inner(first.state.studio.get_monitor(), first.state.studio.get_studio_depth());
    }
    for (performance::PerformanceMonitor* monitor : {&first.performance.monitor, &second.performance.monitor})
    {
        monitor->OnHostExit();
        performance::PerfSnapshot snapshot;
        monitor->GetSnapshot(snapshot);
        uint32_t measured = 0;
        for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
        {
            const ncl_perf::PerfCpuSample& sample = snapshot.cpu_tree[index];
            if (sample.scope_id == performance::PerfStage::Studio && sample.state == ncl_perf::PerfSampleState::Measured)
            {
                ++measured;
                EXPECT_EQ(sample.calls, 1u);
            }
        }
        EXPECT_EQ(measured, 1u);
    }
    EXPECT_EQ(first.state.studio.get_studio_depth(), 0u);
    EXPECT_EQ(second.state.studio.get_studio_depth(), 0u);
}
