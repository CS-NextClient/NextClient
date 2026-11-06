#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include <cstring>
#include <limits>
#include <string>

#include <glad/glad.h>

#include "StudioModelTestSupport.h"
#include "StudioMaterialProviders.h"
#include "MaterialOwnerFiles.h"
#include "common/model.h"
#include "graphics/render/model/StudioModelCache.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/render_device.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/model/studio_model_cache.h"

namespace
{
    int g_GlCalls{};

    void APIENTRY CountNames(GLsizei, GLuint*)
    {
        g_GlCalls++;
    }

    void APIENTRY CountDeletes(GLsizei, const GLuint*)
    {
        g_GlCalls++;
    }

    const GLubyte* APIENTRY CountString(GLenum)
    {
        g_GlCalls++;
        return nullptr;
    }

    class StudioModelCacheTest : public testing::Test
    {
    private:
        PFNGLGENBUFFERSPROC gen_buffers_{};
        PFNGLDELETEBUFFERSPROC delete_buffers_{};
        PFNGLGENVERTEXARRAYSPROC gen_arrays_{};
        PFNGLDELETEVERTEXARRAYSPROC delete_arrays_{};
        PFNGLGETSTRINGPROC get_string_{};
        goldsrc::cvar_t* saved_developer_{};
        goldsrc::cvar_t developer_{};

    protected:
        studio_test::ModelBlock block_;
        goldsrc::model_t model_{};

        void SetUp() override
        {
            render::Studio_DiscardPending();
            render::Device_OnContextDestroyed();
            render::Models_Clear();
            studio_test::g_Files.clear();
            std::strcpy(model_.name, "models/cache_test.mdl");
            saved_developer_ = developer;
            developer = &developer_;
            gen_buffers_ = glad_glGenBuffers;
            delete_buffers_ = glad_glDeleteBuffers;
            gen_arrays_ = glad_glGenVertexArrays;
            delete_arrays_ = glad_glDeleteVertexArrays;
            get_string_ = glad_glGetString;
            glad_glGenBuffers = CountNames;
            glad_glDeleteBuffers = CountDeletes;
            glad_glGenVertexArrays = CountNames;
            glad_glDeleteVertexArrays = CountDeletes;
            glad_glGetString = CountString;
            g_GlCalls = 0;
        }

        void TearDown() override
        {
            render::Studio_DiscardPending();
            render::Models_Clear();
            studio_test::g_Files.clear();
            EXPECT_EQ(g_GlCalls, 0);
            glad_glGenBuffers = gen_buffers_;
            glad_glDeleteBuffers = delete_buffers_;
            glad_glGenVertexArrays = gen_arrays_;
            glad_glDeleteVertexArrays = delete_arrays_;
            glad_glGetString = get_string_;
            developer = saved_developer_;
        }
    };
} // namespace

namespace
{
    struct IndependentModelOwners
    {
        StudioMaterialSourceFixture source;
        MaterialOwnerFiles files;
        render::MaterialDocumentStore documents{files};
        render::ServerMaterials server{documents, source.catalog};
        performance_test::Core performance;
        render::StudioModelCache models{source.device, documents, source.catalog, source.shaders, server, performance.monitor};

        explicit IndependentModelOwners(float fallback)
        {
            ncl_material::ProgramManifest manifest;
            manifest.program = "test";
            ncl_material::ParamSchema value;
            value.name = "uValue";
            value.instance = true;
            value.has_default = true;
            value.default_value.scalar = fallback;
            manifest.parameters.push_back(value);
            source.manifests["test"] = std::move(manifest);
            files.files["models/cache_test.mat"] = R"({"version":1,"materials":{"base.bmp":{"program":"test"}}})";
        }
    };

    int g_ReplacementBarriers{};

    void CountReplacementBarrier(void*)
    {
        ++g_ReplacementBarriers;
    }
} // namespace

TEST_F(StudioModelCacheTest, IndependentOwnersKeepSessionsDocumentsAndCatalogDefaults)
{
    IndependentModelOwners first(1.0f);
    IndependentModelOwners second(2.0f);
    ASSERT_TRUE(first.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    ASSERT_TRUE(second.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    const ncl_material::Material* first_material = first.models.ResolveMaterialByName(model_.name, "base.bmp", nullptr);
    const ncl_material::Material* second_material = second.models.ResolveMaterialByName(model_.name, "base.bmp", nullptr);
    ASSERT_NE(first_material, nullptr);
    ASSERT_NE(second_material, nullptr);
    ASSERT_NE(first_material->FindParam("uValue"), nullptr);
    ASSERT_NE(second_material->FindParam("uValue"), nullptr);
    EXPECT_FLOAT_EQ(first_material->FindParam("uValue")->scalar, 1.0f);
    EXPECT_FLOAT_EQ(second_material->FindParam("uValue")->scalar, 2.0f);
    EXPECT_EQ(first.files.reads.size(), 1u);
    EXPECT_EQ(second.files.reads.size(), 1u);

    first.models.ForgetSessionMetadata();
    EXPECT_EQ(first.models.Metadata(model_.name), nullptr);
    EXPECT_NE(second.models.Metadata(model_.name), nullptr);
    EXPECT_EQ(second.models.ResolveMaterialByName(model_.name, "base.bmp", nullptr), second_material);
    EXPECT_EQ(second.files.reads.size(), 1u);
}

TEST_F(StudioModelCacheTest, OnlyInjectedShaderGenerationReloadsTheDocument)
{
    IndependentModelOwners first(1.0f);
    IndependentModelOwners second(2.0f);
    ASSERT_TRUE(first.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    ASSERT_TRUE(second.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    ASSERT_NE(first.models.ResolveMaterialByName(model_.name, "base.bmp", nullptr), nullptr);
    ASSERT_NE(second.models.ResolveMaterialByName(model_.name, "base.bmp", nullptr), nullptr);
    first.files.files["models/cache_test.mat"] = R"({"version":1,"materials":{"base.bmp":{"program":"test","params":{"uValue":42}}}})";
    second.files.files["models/cache_test.mat"] = first.files.files["models/cache_test.mat"];

    first.source.shaders.OnDeviceLost();
    const ncl_material::Material* first_material = first.models.ResolveMaterialByName(model_.name, "base.bmp", nullptr);
    const ncl_material::Material* second_material = second.models.ResolveMaterialByName(model_.name, "base.bmp", nullptr);
    ASSERT_NE(first_material, nullptr);
    ASSERT_NE(second_material, nullptr);
    ASSERT_NE(first_material->FindParam("uValue"), nullptr);
    ASSERT_NE(second_material->FindParam("uValue"), nullptr);
    EXPECT_FLOAT_EQ(first_material->FindParam("uValue")->scalar, 42.0f);
    EXPECT_FLOAT_EQ(second_material->FindParam("uValue")->scalar, 2.0f);
    EXPECT_EQ(first.files.reads.size(), 2u);
    EXPECT_EQ(second.files.reads.size(), 1u);
}

TEST_F(StudioModelCacheTest, ReplacementBarrierRunsOnlyForActualIdentityReplacement)
{
    IndependentModelOwners owners(1.0f);
    g_ReplacementBarriers = 0;
    ASSERT_TRUE(owners.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_, {nullptr, CountReplacementBarrier}));
    ASSERT_TRUE(owners.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_, {nullptr, CountReplacementBarrier}));
    EXPECT_EQ(g_ReplacementBarriers, 0);

    studio_test::ModelBlock replacement;
    ASSERT_TRUE(
        owners.models.PrepareMetadata(&model_, &replacement.header, &replacement.header, &model_, {nullptr, CountReplacementBarrier})
    );
    EXPECT_EQ(g_ReplacementBarriers, 1);
    owners.models.ForgetSessionMetadata();
    ASSERT_TRUE(
        owners.models.PrepareMetadata(&model_, &replacement.header, &replacement.header, &model_, {nullptr, CountReplacementBarrier})
    );
    EXPECT_EQ(g_ReplacementBarriers, 2);
}

TEST_F(StudioModelCacheTest, MetadataAndMaterialLookupDoNotRequireGeometryOrGl)
{
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_STREQ(render::Models_ModelName(0), model_.name);
    EXPECT_EQ(render::Models_TextureCount(model_.name), 2);
    EXPECT_EQ(render::Models_SkinFamilyCount(model_.name), 2);
    EXPECT_EQ(render::Models_BodyCount(model_.name), 2);
    char body[64]{};
    ASSERT_TRUE(render::Models_BodyName(model_.name, 1, body, sizeof(body)));
    EXPECT_STREQ(body, "alternate");
    EXPECT_FALSE(render::Models_BodyName(model_.name, 2, body, sizeof(body)));
    EXPECT_NE(render::Models_ResolveMaterialByName(model_.name, "BASE.BMP", nullptr), nullptr);
    EXPECT_EQ(render::Models_PrepareGeometry(&model_, &block_.header), nullptr);
    EXPECT_EQ(render::Models_TextureCount(model_.name), 2);
    EXPECT_EQ(g_GlCalls, 0);
}

TEST_F(StudioModelCacheTest, SequenceOutsideCachedExtentPublishesNoMetadata)
{
    block_.header.seqindex = std::numeric_limits<int>::max();
    ASSERT_FALSE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    EXPECT_EQ(render::Models_Metadata(model_.name), nullptr);
    EXPECT_EQ(render::Models_ModelCount(), 0);
}

TEST_F(StudioModelCacheTest, ReloadDocumentsPreservesOwnedMetadataAndRefreshesByName)
{
    studio_test::g_Files["models/cache_test.mat"] =
        R"({"version":1,"materials":{"base.bmp":{"program":"studio_legacy","params":{"fixture":1}}}})";
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    const ncl_material::Material* material = render::Models_ResolveMaterialByName(model_.name, "base.bmp", nullptr);
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("fixture"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("fixture")->scalar, 1);
    studio_test::g_Files["models/cache_test.mat"] =
        R"({"version":1,"materials":{"base.bmp":{"program":"studio_legacy","params":{"fixture":2}}}})";
    render::Models_ReloadDocuments();
    EXPECT_EQ(render::Models_BodyCount(model_.name), 2);
    EXPECT_EQ(render::Models_TextureCount(model_.name), 2);
    material = render::Models_ResolveMaterialByName(model_.name, "base.bmp", nullptr);
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("fixture"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("fixture")->scalar, 2);
}

TEST_F(StudioModelCacheTest, TextureHeaderReplacementPublishesOwnedNamesAndImplicitFlags)
{
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    studio_test::ModelBlock companion;
    std::strcpy(companion.textures[0].name, "new.bmp");
    companion.textures[0].flags = goldsrc::STUDIO_NF_FULLBRIGHT;
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &companion.header, &model_));
    std::strcpy(companion.textures[0].name, "mutated.bmp");
    EXPECT_STREQ(render::Models_TextureName(model_.name, 0), "new.bmp");
    EXPECT_EQ(render::Models_ResolveMaterialByName(model_.name, "base.bmp", nullptr), nullptr);
    EXPECT_NE(render::Models_ResolveMaterialByName(model_.name, "new.bmp", nullptr), nullptr);
}

TEST_F(StudioModelCacheTest, TextureOwnerNameChangesAtSameHeaderAndInvalidationHidesEveryAuthoringGetter)
{
    studio_test::ModelBlock companion;
    goldsrc::model_t texture_owner{};
    std::strcpy(texture_owner.name, "models/first_texture_owner.mdl");
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &companion.header, &texture_owner));
    EXPECT_STREQ(render::Models_TextureName(model_.name, 0), "base.bmp");
    std::strcpy(texture_owner.name, "models/next_texture_owner.mdl");
    std::strcpy(companion.textures[0].name, "next.bmp");
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &companion.header, &texture_owner));
    EXPECT_STREQ(render::Models_TextureName(model_.name, 0), "next.bmp");
    render::Models_ForgetModel("models/first_texture_owner.mdl");
    EXPECT_STREQ(render::Models_TextureName(model_.name, 0), "next.bmp");
    ASSERT_EQ(render::Models_ModelCount(), 1);

    render::Models_ForgetModel(texture_owner.name);

    EXPECT_EQ(render::Models_ModelName(0), nullptr);
    EXPECT_EQ(render::Models_ModelCount(), 0);
    EXPECT_EQ(render::Models_TextureCount(model_.name), 0);
    EXPECT_EQ(render::Models_TextureName(model_.name, 0), nullptr);
    EXPECT_EQ(render::Models_SkinFamilyCount(model_.name), 0);
    EXPECT_EQ(render::Models_BodyCount(model_.name), 0);
    char name[32]{};
    EXPECT_FALSE(render::Models_BodyName(model_.name, 0, name, sizeof(name)));
    EXPECT_EQ(render::Models_ResolveMaterialByName(model_.name, "next.bmp", nullptr), nullptr);
    const ncl_studio::StudioMetadata* metadata = render::Models_Metadata(model_.name);
    ASSERT_NE(metadata, nullptr);
    EXPECT_EQ(metadata->sequences.size(), 3u);
    EXPECT_EQ(metadata->bodyparts.size(), 1u);
    EXPECT_EQ(g_GlCalls, 0);
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &companion.header, &texture_owner));
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_STREQ(render::Models_TextureName(model_.name, 0), "next.bmp");
}

TEST_F(StudioModelCacheTest, NameAndHeaderBothParticipateInIdentity)
{
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    std::strcpy(model_.name, "models/other.mdl");
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    EXPECT_EQ(render::Models_ModelCount(), 2);
    studio_test::ModelBlock replacement;
    std::strcpy(replacement.submodels[0].name, "replacement");
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &replacement.header, &replacement.header, &model_));
    char body[64]{};
    ASSERT_TRUE(render::Models_BodyName(model_.name, 0, body, sizeof(body)));
    EXPECT_STREQ(body, "replacement");
    ASSERT_TRUE(render::Models_BodyName("models/cache_test.mdl", 0, body, sizeof(body)));
    EXPECT_STREQ(body, "body");
    EXPECT_EQ(render::Models_ModelCount(), 2);
}

TEST_F(StudioModelCacheTest, SessionForgetInvalidatesSnapshotAndSameStorageMetadataWithoutGl)
{
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    ASSERT_EQ(render::Models_ModelCount(), 1);
    render::Studio_DiscardPending();
    render::Models_ForgetSessionMetadata();
    EXPECT_EQ(render::Models_ModelName(0), nullptr);
    EXPECT_EQ(render::Models_Metadata(model_.name), nullptr);
    EXPECT_EQ(render::Models_TextureCount(model_.name), 0);
    EXPECT_EQ(render::Models_ResolveMaterialByName(model_.name, "base.bmp", nullptr), nullptr);
    EXPECT_EQ(render::Models_ModelCount(), 0);
    EXPECT_EQ(g_GlCalls, 0);
    std::strcpy(block_.textures[0].name, "new_session.bmp");
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    EXPECT_STREQ(render::Models_TextureName(model_.name, 0), "new_session.bmp");
}

TEST_F(StudioModelCacheTest, InvalidMetadataPublishesNoPartialAuthoringEntry)
{
    block_.body.modelindex = block_.header.texturedataindex;
    EXPECT_FALSE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    EXPECT_EQ(render::Models_ModelCount(), 0);
    EXPECT_EQ(render::Models_Metadata(model_.name), nullptr);
    EXPECT_EQ(render::Models_TextureCount(model_.name), 0);
}

TEST_F(StudioModelCacheTest, BodyChoiceCapStaysAtTheConsumer)
{
    block_.body.nummodels = 65;
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    EXPECT_EQ(render::Models_Metadata(model_.name)->bodyparts[0].submodels.size(), 65u);
    EXPECT_EQ(render::Models_BodyCount(model_.name), 64);
}

TEST_F(StudioModelCacheTest, FilteredUnloadInvalidatesSelectedModelBeforeFreeAndPreservesOthers)
{
    ASSERT_EQ(mod_numknown, 0);
    studio_test::ModelEngine engine;
    struct RestoreGlobals
    {
        nitroapi::NitroApiInterface* api = g_NitroApi;

        ~RestoreGlobals()
        {
            g_NitroApi = api;
            std::memset(mod_known, 0, sizeof(mod_known));
            mod_numknown = 0;
        }
    } restore;
    g_NitroApi = &engine;
    mod_numknown = 2;
    std::strcpy(mod_known[0].name, model_.name);
    std::strcpy(mod_known[1].name, "models/retained.mdl");
    mod_known[0].type = mod_known[1].type = goldsrc::mod_studio;
    mod_known[0].cache.data = mod_known[1].cache.data = &block_.header;
    ASSERT_TRUE(render::Models_PrepareMetadata(&mod_known[0], &block_.header, &block_.header, &mod_known[0]));
    ASSERT_TRUE(render::Models_PrepareMetadata(&mod_known[1], &block_.header, &block_.header, &mod_known[1]));
    ASSERT_EQ(render::Models_ModelCount(), 2);
    int filters = 0;
    int frees = 0;
    engine.free.callback = [&](goldsrc::cache_user_t* cache) {
        frees++;
        EXPECT_EQ(render::Models_Metadata(model_.name), nullptr);
        EXPECT_NE(render::Models_Metadata("models/retained.mdl"), nullptr);
        EXPECT_EQ(render::Models_ModelName(0), nullptr);
        EXPECT_EQ(render::Models_ModelCount(), 1);
        cache->data = nullptr;
    };
    Mod_UnloadFiltered([&](goldsrc::model_t* model) {
        filters++;
        return model == &mod_known[0];
    });
    EXPECT_EQ(filters, 2);
    EXPECT_EQ(frees, 1);
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_STREQ(render::Models_ModelName(0), "models/retained.mdl");
}

TEST_F(StudioModelCacheTest, GeometryReleasePreservesMetadataAndResolvedMaterialsWithoutGl)
{
    ASSERT_TRUE(render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    const ncl_studio::StudioMetadata* metadata = render::Models_Metadata(model_.name);
    const ncl_material::Material* material = render::Models_ResolveMaterialByName(model_.name, "base.bmp", nullptr);
    ASSERT_NE(metadata, nullptr);
    ASSERT_NE(material, nullptr);
    ASSERT_EQ(render::Models_ModelCount(), 1);
    const char* name = render::Models_ModelName(0);

    render::Models_ReleaseGeometry();
    render::Models_ReleaseGeometry();

    EXPECT_EQ(render::Models_Metadata(model_.name), metadata);
    EXPECT_EQ(render::Models_ResolveMaterialByName(model_.name, "base.bmp", nullptr), material);
    EXPECT_EQ(render::Models_ModelName(0), name);
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_EQ(render::Models_TextureCount(model_.name), 2);
    EXPECT_EQ(render::Models_SkinFamilyCount(model_.name), 2);
    EXPECT_EQ(render::Models_BodyCount(model_.name), 2);
    EXPECT_EQ(g_GlCalls, 0);
}

TEST_F(StudioModelCacheTest, ReplacementBarrierInvalidationStopsBeforeUsingRetiredEntries)
{
    IndependentModelOwners owners(1.0f);
    studio_test::ModelBlock replacement;
    for (bool forget_session : {false, true})
    {
        SCOPED_TRACE(forget_session);
        ASSERT_TRUE(owners.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
        struct Invalidation
        {
            render::StudioModelCache& models;
            bool forget_session;
            int calls{};
        } invalidation{owners.models, forget_session};
        render::StudioModelBarrier barrier{&invalidation, [](void* user) {
                                               Invalidation& state = *static_cast<Invalidation*>(user);
                                               ++state.calls;
                                               if (state.forget_session)
                                               {
                                                   state.models.ForgetSessionMetadata();
                                               }
                                               else
                                               {
                                                   state.models.Clear();
                                               }
                                           }};
        EXPECT_FALSE(owners.models.PrepareMetadata(&model_, &replacement.header, &replacement.header, &model_, barrier));
        EXPECT_EQ(invalidation.calls, 1);
        EXPECT_EQ(owners.models.Metadata(model_.name), nullptr);
        EXPECT_EQ(owners.models.PrepareGeometry(&model_, &replacement.header, barrier), nullptr);
        owners.models.Clear();
    }
}

TEST_F(StudioModelCacheTest, NestedDifferentModelReplacementRejectsTheOuterBarrierAndPreservesBothEntries)
{
    IndependentModelOwners owners(1.0f);
    goldsrc::model_t other = model_;
    std::strcpy(other.name, "models/other_cache_test.mdl");
    block_.sequences[0].numframes = 3;
    studio_test::ModelBlock replacement;
    replacement.sequences[0].numframes = 9;
    ASSERT_TRUE(owners.models.PrepareMetadata(&other, &block_.header, &block_.header, &other));
    ASSERT_TRUE(owners.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    struct NestedReplacement
    {
        render::StudioModelCache& cache;
        goldsrc::model_t& model;
        studio_test::ModelBlock& replacement;
        unsigned int calls{};
        bool prepared{};
    } nested{owners.models, other, replacement};
    const render::StudioModelBarrier barrier{&nested, [](void* user) {
                                                 NestedReplacement& state = *static_cast<NestedReplacement*>(user);
                                                 ++state.calls;
                                                 if (state.calls == 1)
                                                 {
                                                     state.prepared = state.cache.PrepareMetadata(
                                                         &state.model, &state.replacement.header, &state.replacement.header, &state.model
                                                     );
                                                 }
                                             }};

    EXPECT_FALSE(owners.models.PrepareMetadata(&model_, &replacement.header, &replacement.header, &model_, barrier));
    EXPECT_EQ(nested.calls, 1u);
    ASSERT_TRUE(nested.prepared);
    EXPECT_EQ(owners.models.ModelCount(), 2);
    const ncl_studio::StudioMetadata* other_metadata = owners.models.Metadata(other.name);
    ASSERT_NE(other_metadata, nullptr);
    ASSERT_FALSE(other_metadata->sequences.empty());
    EXPECT_EQ(other_metadata->sequences[0].frame_count, 9);
    const ncl_studio::StudioMetadata* original_metadata = owners.models.Metadata(model_.name);
    ASSERT_NE(original_metadata, nullptr);
    ASSERT_FALSE(original_metadata->sequences.empty());
    EXPECT_EQ(original_metadata->sequences[0].frame_count, 3);
}
