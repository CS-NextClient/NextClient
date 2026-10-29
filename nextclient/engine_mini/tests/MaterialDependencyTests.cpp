#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include <limits>
#include <functional>

#include <ncl_material/material_parse.h>

#include "MaterialOwnerFiles.h"
#include "StudioMaterialProviders.h"
#include "StudioModelTestSupport.h"
#include "client/ClientStringRegistry.h"
#include "client/ncl_entity/MaterialSyncSystem.h"
#include "graphics/render/material/MaterialEditor.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/model/StudioModelCache.h"

namespace
{
    bool Disconnected()
    {
        return false;
    }

    bool g_NetworkSession{};
    int g_NetworkReads{};

    bool NetworkSession()
    {
        g_NetworkReads++;
        return g_NetworkSession;
    }

    class DependencyFiles : public MaterialOwnerFiles
    {
    public:
        std::function<void()> before_read;

        render::MaterialReadStatus ReadFile(const char* path, std::string& text, size_t max_bytes) override
        {
            if (before_read)
            {
                std::function<void()> callback = std::move(before_read);
                callback();
            }
            return MaterialOwnerFiles::ReadFile(path, text, max_bytes);
        }
    };

    struct MaterialDependencies
    {
        StudioMaterialSourceFixture source;
        DependencyFiles files;
        render::MaterialDocumentStore documents{files};
        render::ServerMaterials server{documents, source.catalog};
        performance_test::Core performance;
        render::StudioModelCache models{source.device, documents, source.catalog, source.shaders, server, performance.monitor};
        render::MaterialEditor editor{models, server, source.shaders, source.catalog, documents};
        ClientStringRegistry strings;
        MaterialSyncSystem sync{server, strings};
        studio_test::ModelBlock block;
        goldsrc::model_t model{};

        explicit MaterialDependencies(float initial = 1)
        {
            ncl_material::ProgramManifest manifest;
            manifest.program = "test";
            ncl_material::ParamSchema wear;
            wear.name = "uWear";
            wear.instance = true;
            wear.has_default = true;
            wear.default_value.scalar = initial;
            manifest.parameters.push_back(wear);
            source.manifests["test"] = manifest;
            files.files["owned.mat"] = R"({"version":1,"materials":[
                {"name":"first","texture":"base.bmp","material":{"program":"test"}},
                {"name":"second","texture":"alternate.bmp","material":{"program":"test"}}
            ]})";
            files.files["models/owned.mat"] = R"({"version":1,"materials":{
                "base.bmp":{"program":"test"},"alternate.bmp":{"program":"test"},
                "untouched.bmp":{"program":"studio_legacy"}}})";
            strings.Register(1, "owned.mat");
            strings.Register(2, "first");
            strings.Register(3, "second");
            strings.Register(4, "uWear");
            std::strcpy(model.name, "models/owned.mdl");
            EXPECT_TRUE(models.PrepareMetadata(&model, &block.header, &block.header, &model));
        }

        float Wear(int entity, const char* texture = "base.bmp")
        {
            const ncl_material::Material* material = server.BindingFor({entity, texture}, true);
            EXPECT_NE(material, nullptr);
            return material != nullptr ? material->FindParam("uWear")->scalar : -1;
        }
    };

    NclEntityFields Snapshot(uint32_t entity, float value)
    {
        ncl_entity::ArrayStringIdPairsDelta materials;
        materials.added = {{1, 2}};
        ncl_entity::ArrayStringIdFloatPairsDelta params;
        params.added = {{4, value}};
        return {{1, entity}, {2, materials}, {4, params}};
    }
} // namespace

TEST(ClientStringRegistryTest, IndependentBoundedOwnersReplaceAtCapacityAndClear)
{
    ClientStringRegistry first;
    ClientStringRegistry second;
    first.Register(0, "ignored");
    EXPECT_EQ(first.Find(0), nullptr);
    for (uint16_t id = 1; id <= 2048; id++)
    {
        EXPECT_NE(first.Register(id, "first"), ClientStringRegistry::RegisterResult::CapacityExceeded);
    }
    EXPECT_EQ(first.Register(2049, "overflow"), ClientStringRegistry::RegisterResult::CapacityExceeded);
    EXPECT_NE(first.Register(1, "replacement"), ClientStringRegistry::RegisterResult::CapacityExceeded);
    second.Register(1, "second");
    EXPECT_STREQ(first.Find(1), "replacement");
    EXPECT_STREQ(second.Find(1), "second");
    EXPECT_EQ(first.get_entries().size(), 2048u);
    first.Clear();
    EXPECT_EQ(first.Find(1), nullptr);
    EXPECT_STREQ(second.Find(1), "second");
}

TEST(MaterialSyncOwnerTest, UsesOnlyInjectedRegistryAndMaterialsAndCopiesRegistryStrings)
{
    MaterialDependencies first(1);
    MaterialDependencies second(2);
    first.sync.OnCreate(10, Snapshot(7, 0.5f));
    second.sync.OnCreate(10, Snapshot(7, 0.75f));
    EXPECT_FLOAT_EQ(first.Wear(7), 0.5f);
    EXPECT_FLOAT_EQ(second.Wear(7), 0.75f);
    first.strings.Clear();
    EXPECT_FLOAT_EQ(first.Wear(7), 0.5f);
    first.sync.OnDestroy(10);
    EXPECT_EQ(first.server.BindingFor({7, "base.bmp"}, true), nullptr);
    EXPECT_FLOAT_EQ(second.Wear(7), 0.75f);
}

TEST(MaterialSyncOwnerTest, CompleteAndParameterReplacementMigrationAndConsoleRemoval)
{
    MaterialDependencies owned;
    owned.server.SetBinding(7, "owned.mat", "second", false);
    owned.sync.OnCreate(10, Snapshot(7, 0.5f));
    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.clear = true;
    owned.sync.OnUpdate(10, 4, {{4, params}});
    EXPECT_FLOAT_EQ(owned.Wear(7), 1);
    ncl_entity::ArrayStringIdPairsDelta materials;
    materials.added = {{1, 3}};
    owned.sync.OnUpdate(10, 2, {{2, materials}});
    EXPECT_FLOAT_EQ(owned.Wear(7, "alternate.bmp"), 1);
    owned.sync.OnUpdate(10, 1, {{1, uint32_t{8}}});
    EXPECT_EQ(owned.server.BindingFor({7, "base.bmp"}, true), nullptr);
    EXPECT_NE(owned.server.BindingFor({7, "alternate.bmp"}, false), nullptr);
    EXPECT_FLOAT_EQ(owned.Wear(8), 1);
    owned.sync.OnClear();
    EXPECT_EQ(owned.server.BindingFor({8, "base.bmp"}, true), nullptr);
    EXPECT_NE(owned.server.BindingFor({7, "alternate.bmp"}, false), nullptr);
}

TEST(MaterialSyncOwnerTest, RejectedSnapshotDoesNotPublishMigrationOrPoisonFollowingDelta)
{
    MaterialDependencies owned;
    owned.sync.OnCreate(10, Snapshot(7, 0.5f));
    owned.sync.OnUpdate(10, 5, Snapshot(8, std::numeric_limits<float>::infinity()));
    EXPECT_FLOAT_EQ(owned.Wear(7), 0.5f);
    EXPECT_EQ(owned.server.BindingFor({8, "base.bmp"}, true), nullptr);
    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added = {{4, 0.25f}};
    owned.sync.OnUpdate(10, 4, {{4, params}});
    EXPECT_FLOAT_EQ(owned.Wear(7), 0.25f);
}

TEST(MaterialSyncOwnerTest, CopiesAllRegistryValuesBeforePresetLoadingClearsTheRegistry)
{
    MaterialDependencies owned;
    owned.files.before_read = [&] { owned.strings.Clear(); };
    owned.sync.OnCreate(10, Snapshot(7, 0.5f));
    EXPECT_FLOAT_EQ(owned.Wear(7), 0.5f);
    EXPECT_EQ(owned.strings.Find(1), nullptr);
}

TEST(MaterialSyncOwnerTest, RejectedMaterialReplacementRetainsPreviousCompleteWireSnapshot)
{
    MaterialDependencies owned(0);
    ncl_material::ParamSchema& schema = owned.source.manifests["test"].parameters.front();
    schema.widget = ncl_material::ParamWidget::Choice;
    schema.has_range = true;
    schema.max_value = 2;
    schema.choices = {"Off", "On", "Auto"};
    owned.sync.OnCreate(10, Snapshot(7, 1));
    ncl_entity::ArrayStringIdPairsDelta materials;
    materials.clear = true;
    materials.added = {{1, 3}};
    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added = {{4, 0.5f}};
    owned.sync.OnUpdate(10, 7, {{1, uint32_t{8}}, {2, materials}, {4, params}});
    EXPECT_FLOAT_EQ(owned.Wear(7), 1);
    EXPECT_EQ(owned.server.BindingFor({8, "alternate.bmp"}, true), nullptr);
    params.added = {{4, 2}};
    owned.sync.OnUpdate(10, 4, {{4, params}});
    EXPECT_FLOAT_EQ(owned.Wear(7), 2);
    owned.sync.OnUpdate(10, 1, {{1, uint32_t{8}}});
    EXPECT_FLOAT_EQ(owned.Wear(8), 2);
    EXPECT_EQ(owned.server.BindingFor({8, "alternate.bmp"}, true), nullptr);
}

TEST(MaterialEditorOwnerTest, CopiesSelectionsAndKeepsRepeatedOverrideIndependentOfBackends)
{
    MaterialDependencies first(1);
    MaterialDependencies second(2);
    const char* textures[] = {"base.bmp", "alternate.bmp", "BASE.BMP"};
    ASSERT_TRUE(first.editor.BeginGroup("models/owned.mdl", textures, 3, 0, Disconnected));
    ASSERT_TRUE(second.editor.BeginGroup("models/owned.mdl", textures, 2, 0, Disconnected));
    EXPECT_EQ(first.editor.get_selection_count(), 2);
    ASSERT_TRUE(first.editor.SetScalar("uWear", 0.5f));
    first.models.ReloadDocuments();
    const size_t reads = first.files.reads.size();
    const size_t queries = first.source.manifest_queries.size();
    for (int repeat = 0; repeat < 1000; repeat++)
    {
        EXPECT_FLOAT_EQ(first.editor.get_session().FindOverride("models/owned.mdl", "base.bmp")->FindParam("uWear")->scalar, 0.5f);
    }
    EXPECT_EQ(first.files.reads.size(), reads);
    EXPECT_EQ(first.source.manifest_queries.size(), queries);
    EXPECT_FLOAT_EQ(second.editor.get_session().FindOverride("models/owned.mdl", "base.bmp")->FindParam("uWear")->scalar, 2);
    first.editor.End();
    EXPECT_EQ(first.editor.get_session().FindOverride("models/owned.mdl", "base.bmp"), nullptr);
    EXPECT_TRUE(second.editor.is_editing());
}

TEST(MaterialEditorOwnerTest, RollbackSaveMergeAndReloadUseTheInjectedDocumentStore)
{
    MaterialDependencies owned;
    const char* textures[] = {"base.bmp", "alternate.bmp"};
    ASSERT_TRUE(owned.editor.BeginGroup("models/owned.mdl", textures, 2, 0, Disconnected));
    ASSERT_TRUE(owned.editor.BeginParamPreview("uWear"));
    ASSERT_TRUE(owned.editor.SetScalar("uWear", 0.5f));
    owned.editor.EndParamPreview(false);
    float value = 0;
    ASSERT_TRUE(owned.editor.get_scalar("uWear", &value));
    EXPECT_FLOAT_EQ(value, 1);
    ASSERT_TRUE(owned.editor.SetScalar("uWear", 0.75f));
    char error[256]{};
    ASSERT_TRUE(owned.editor.Save(nullptr, error, sizeof(error))) << error;
    const ncl_utils::ResultT<ncl_material::MaterialDoc> parsed = ncl_material::ParseMaterialDoc(owned.files.files.at("models/owned.mat"));
    ASSERT_FALSE(parsed.has_error());
    ASSERT_EQ(parsed->entries.size(), 3u);
    EXPECT_FLOAT_EQ(owned.models.ResolveMaterialByName("models/owned.mdl", "base.bmp", nullptr)->FindParam("uWear")->scalar, 0.75f);
    owned.editor.End();
    EXPECT_EQ(owned.editor.get_selection_count(), 0);
}

TEST(MaterialEditorOwnerTest, ConnectionStateIsReadAgainAfterModelDocumentLoading)
{
    MaterialDependencies owned;
    owned.server.SetBinding(7, "owned.mat", "second", false);
    ASSERT_TRUE(owned.server.SetParam(7, "uWear", 0.25f, false));
    g_NetworkSession = false;
    g_NetworkReads = 0;
    owned.files.before_read = [] { g_NetworkSession = true; };
    const char* textures[] = {"base.bmp", "alternate.bmp"};
    ASSERT_TRUE(owned.editor.BeginGroup("models/owned.mdl", textures, 2, 7, NetworkSession));
    EXPECT_EQ(g_NetworkReads, 2);
    EXPECT_FLOAT_EQ(owned.editor.get_session().FindOverride("models/owned.mdl", "alternate.bmp")->FindParam("uWear")->scalar, 1);
}

TEST(MaterialEditorOwnerTest, InvalidDocumentChoiceRetainsTheValidatedModelFallback)
{
    MaterialDependencies owned(0);
    ncl_material::ParamSchema& schema = owned.source.manifests["test"].parameters.front();
    schema.widget = ncl_material::ParamWidget::Choice;
    schema.has_range = true;
    schema.max_value = 1;
    schema.choices = {"Off", "On"};
    owned.files.files["models/owned.mat"] = R"({"version":1,"materials":{"base.bmp":{"program":"test","params":{"uWear":0.5}}}})";
    const char* textures[] = {"base.bmp"};
    ASSERT_TRUE(owned.editor.BeginGroup("models/owned.mdl", textures, 1, 0, Disconnected));
    EXPECT_STREQ(owned.editor.get_edited_program(), ncl_material::kLegacyProgram);
    EXPECT_FALSE(owned.editor.SetScalar("uWear", 1));
    EXPECT_FALSE(owned.files.reports.empty());
    ASSERT_TRUE(owned.editor.SetEditedProgram("test"));
    EXPECT_TRUE(owned.editor.SetScalar("uWear", 1));
}
