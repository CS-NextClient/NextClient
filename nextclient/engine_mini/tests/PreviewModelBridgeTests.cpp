#include <gtest/gtest.h>

#include <limits>
#include <memory>

#include <glad/glad.h>

#include <goldsrc/entity.h>
#include <next_engine_mini/ScenePreviewInterface.h>

#include "engine.h"
#include "PreviewTestOwners.h"
#include "StudioModelTestSupport.h"
#include "common/model.h"
#include "graphics/preview/preview_model.h"
#include "graphics/preview/preview.h"
#include "graphics/preview/ScenePreview.h"
#include "graphics/render/render_device.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/model/studio_model_cache.h"
#include "graphics/render/RendererLifecycle.h"

namespace
{
    int g_DynamicLightCalls;

    void DynamicLight(goldsrc::cl_entity_t*, goldsrc::alight_t*)
    {
        g_DynamicLightCalls++;
    }

    int CheckBBox()
    {
        return 17;
    }

    int ReplacementBBox()
    {
        return 23;
    }
} // namespace

TEST(PreviewModelBridge, OwnedPoseUsesSequenceFrameUnitsAndDisablesClientClockTransitions)
{
    goldsrc::cl_entity_t entity{};

    for (int frame_count : {1, 5, 31, 258})
    {
        SCOPED_TRACE(frame_count);

        for (float progress : {0.0f, 0.125f, 0.5f, 0.875f, 1.0f})
        {
            SCOPED_TRACE(progress);
            PreviewModel_SetAnimationPose(entity, 2, frame_count, progress, 1234.0f);
            EXPECT_EQ(entity.curstate.sequence, 2);
            EXPECT_FLOAT_EQ(entity.curstate.frame, progress * 256.0f);
            EXPECT_FLOAT_EQ(entity.curstate.animtime, 1234.0f);
            EXPECT_FLOAT_EQ(entity.curstate.framerate, 0.0f);
            EXPECT_EQ(entity.latched.prevsequence, 2);
            EXPECT_FLOAT_EQ(entity.latched.prevframe, progress * static_cast<float>(frame_count - 1));
            EXPECT_FLOAT_EQ(entity.latched.prevanimtime, 1234.0f);
            EXPECT_FLOAT_EQ(entity.latched.sequencetime, 0.0f);
        }
    }
}

TEST(PreviewModelBridge, ColdStartReportsMissingStudioInterfaceWithoutLoading)
{
    goldsrc::r_studio_interface_t* saved = pStudioAPI;
    pStudioAPI = nullptr;
    preview_test::BindEngine(render::GetRendererState());
    PreviewModelInfo info{};
    info.sequence_count = 7;

    EXPECT_EQ(PreviewModel_Prepare("models/test.mdl", &info), PreviewModelStatus::RendererUnavailable);
    EXPECT_EQ(info.sequence_count, 0);

    pStudioAPI = saved;
    preview_test::BindEngine(render::GetRendererState());
}

TEST(PreviewModelBridge, CachedModelAndCompanionPrepareMetadataWithoutAContext)
{
    struct Restore
    {
        nitroapi::NitroApiInterface* api = g_NitroApi;
        goldsrc::r_studio_interface_t* studio = pStudioAPI;
        goldsrc::cl_entity_t** entity = p_currententity;
        float* blend = p_r_blend;

        ~Restore()
        {
            render::Studio_DiscardPending();
            render::Models_Clear();
            std::memset(mod_known, 0, sizeof(mod_known));
            mod_numknown = 0;
            PreviewModel_ResetModels();
            g_NitroApi = api;
            pStudioAPI = studio;
            p_currententity = entity;
            p_r_blend = blend;
            preview_test::BindEngine(render::GetRendererState());
        }
    } restore;
    ASSERT_EQ(mod_numknown, 0);
    render::Studio_DiscardPending();
    render::Device_OnContextDestroyed();
    render::Models_Clear();
    PreviewModel_ResetModels();
    studio_test::ModelEngine engine;
    studio_test::ModelBlock model;
    studio_test::ModelBlock companion;
    std::strcpy(model.sequences[0].label, "owned sequence");
    model.sequences[0].fps = 12.0f;
    model.sequences[0].numframes = 25;
    model.header.numtextures = 0;
    model.header.textureindex = 0;
    model.header.length = sizeof(model);
    companion.header.numbones = 0;
    companion.header.numbodyparts = 0;
    goldsrc::r_studio_interface_t studio{};
    studio.StudioDrawModel = [](int) { return 0; };
    goldsrc::cl_entity_t* entity = nullptr;
    float blend = 1;
    g_NitroApi = &engine;
    pStudioAPI = &studio;
    p_currententity = &entity;
    p_r_blend = &blend;
    preview_test::BindEngine(render::GetRendererState());
    mod_numknown = 2;
    std::strcpy(mod_known[0].name, "models/metadata_test.mdl");
    std::strcpy(mod_known[1].name, "models/metadata_testT.mdl");
    mod_known[0].type = mod_known[1].type = goldsrc::mod_studio;
    mod_known[0].cache.data = &model.header;
    mod_known[1].cache.data = &companion.header;
    PreviewModelInfo info{};
    ASSERT_FALSE(render::Device_IsContextAlive());

    ASSERT_EQ(PreviewModel_Prepare("models/metadata_test.mdl", &info), PreviewModelStatus::Ready);
    EXPECT_EQ(info.sequence_count, 3);
    EXPECT_EQ(info.skin_family_count, 2);
    EXPECT_EQ(info.body_count, 2);
    EXPECT_FLOAT_EQ(info.bounds_min[0], -8);
    EXPECT_FLOAT_EQ(info.bounds_max[2], 24);
    EXPECT_EQ(PreviewModel_LoadedEntries(), 0);
    EXPECT_EQ(render::Models_TextureCount("models/metadata_test.mdl"), 2);
    EXPECT_EQ(render::Models_PrepareGeometry(&mod_known[0], &model.header), nullptr);

    model.header.seqindex = std::numeric_limits<int>::max();
    model.header.numseq = std::numeric_limits<int>::max();
    std::memset(&model.sequences[0], 0, sizeof(model.sequences[0]));
    PreviewSequenceInfo sequence{};
    ASSERT_TRUE(PreviewModel_GetSequenceInfo("models/metadata_test.mdl", 0, &sequence));
    EXPECT_STREQ(sequence.name, "owned sequence");
    EXPECT_FLOAT_EQ(sequence.duration, 2.0f);
    EXPECT_FALSE(PreviewModel_GetSequenceInfo("models/metadata_test.mdl", -1, &sequence));
    EXPECT_FALSE(PreviewModel_GetSequenceInfo("models/metadata_test.mdl", 3, &sequence));
}

TEST(PreviewModelBridge, MissingSequenceDoesNotExposeStaleMetadata)
{
    goldsrc::r_studio_interface_t* saved = pStudioAPI;
    pStudioAPI = nullptr;
    preview_test::BindEngine(render::GetRendererState());
    PreviewSequenceInfo info{};
    info.name[0] = 'x';
    info.duration = 9.0f;

    EXPECT_FALSE(PreviewModel_GetSequenceInfo("models/test.mdl", 0, &info));
    EXPECT_EQ(info.name[0], '\0');
    EXPECT_FLOAT_EQ(info.duration, 0.0f);

    pStudioAPI = saved;
    preview_test::BindEngine(render::GetRendererState());
}

TEST(PreviewModelBridge, MainMenuEntryRequiresADisconnectedClient)
{
    goldsrc::client_static_t* saved = cls;
    std::unique_ptr<goldsrc::client_static_t> client = std::make_unique<goldsrc::client_static_t>();
    cls = client.get();
    preview_test::BindEngine(render::GetRendererState());
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    cls->state = goldsrc::ca_disconnected;
    EXPECT_TRUE(preview.is_authoring_available());
    cls->state = goldsrc::ca_connecting;
    EXPECT_FALSE(preview.is_authoring_available());
    cls->state = goldsrc::ca_connected;
    EXPECT_FALSE(preview.is_authoring_available());
    cls->state = goldsrc::ca_active;
    EXPECT_FALSE(preview.is_authoring_available());
    cls = nullptr;
    preview_test::BindEngine(render::GetRendererState());
    EXPECT_FALSE(preview.is_authoring_available());
    cls = saved;
    preview_test::BindEngine(render::GetRendererState());
}

TEST(PreviewModelBridge, RestoringStudioApiPreservesAnotherOwnersReplacement)
{
    goldsrc::engine_studio_api_t api{};
    api.StudioDynamicLight = DynamicLight;
    api.StudioCheckBBox = CheckBBox;
    PreviewModel_PatchStudioApi(&api);
    api.StudioCheckBBox = ReplacementBBox;

    PreviewModel_RestoreStudioApi();

    EXPECT_EQ(api.StudioCheckBBox, ReplacementBBox);
    EXPECT_EQ(api.StudioDynamicLight, DynamicLight);
}

TEST(PreviewModelBridge, CopiedStudioCallbacksForwardAfterShutdown)
{
    goldsrc::engine_studio_api_t api{};
    api.StudioDynamicLight = DynamicLight;
    api.StudioCheckBBox = CheckBBox;
    PreviewModel_PatchStudioApi(&api);
    goldsrc::engine_studio_api_t copied = api;
    render::RendererLifecycle lifecycle = preview_test::MakeLifecycle(render::GetRendererState());
    lifecycle.BeginShutdown();
    goldsrc::cl_entity_t entity{};
    goldsrc::alight_t light{};
    g_DynamicLightCalls = 0;
    copied.StudioDynamicLight(&entity, &light);

    EXPECT_EQ(g_DynamicLightCalls, 1);
    EXPECT_EQ(copied.StudioCheckBBox(), 17);
}

TEST(PreviewModelBridge, ProviderUnbindRestoresCallbacksAndRetiresBorrowedBindingsIdempotently)
{
    goldsrc::engine_studio_api_t api{};
    api.StudioDynamicLight = DynamicLight;
    api.StudioCheckBBox = CheckBBox;
    PreviewModel_PatchStudioApi(&api);
    goldsrc::engine_studio_api_t copied = api;

    Preview_UnbindEngine();
    Preview_UnbindEngine();

    EXPECT_EQ(api.StudioDynamicLight, DynamicLight);
    EXPECT_EQ(api.StudioCheckBBox, CheckBBox);
    EXPECT_EQ(render::GetRendererState().preview_engine.studio, nullptr);
    EXPECT_EQ(render::GetRendererState().preview_engine.current_entity, nullptr);
    EXPECT_EQ(render::GetRendererState().preview_engine.blend, nullptr);
    EXPECT_EQ(render::GetRendererState().preview_engine.print, nullptr);
    EXPECT_FALSE(render::GetRendererState().preview_assets.is_available());
    PreviewModelInfo info{};
    EXPECT_EQ(PreviewModel_Prepare("models/test.mdl", &info), PreviewModelStatus::RendererUnavailable);
    EXPECT_EQ(copied.StudioCheckBBox(), 17);
    goldsrc::cl_entity_t entity{};
    goldsrc::alight_t light{};
    g_DynamicLightCalls = 0;
    copied.StudioDynamicLight(&entity, &light);
    EXPECT_EQ(g_DynamicLightCalls, 1);
    preview_test::BindEngine(render::GetRendererState());
}
