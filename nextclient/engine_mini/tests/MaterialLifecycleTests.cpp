#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <ncl_material/material_parse.h>
#include <ncl_material/MaterialPreset.h>
#include <ncl_material/MaterialResolve.h>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <easylogging++.h>
#include <ncl_gl/capture.h>
#include <next_engine_mini/MaterialEditInterface.h>

#include "engine.h"
#include "StudioModelTestSupport.h"
#include "client/ClientStringRegistry.h"
#include "client/ncl_entity/MaterialSyncSystem.h"
#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/filesystem.h"
#include "graphics/render/material/material_edit.h"
#include "graphics/render/RendererResources.h"
#include "graphics/render/material/MaterialEditor.h"
#include "graphics/render/model/StudioModelCache.h"
#include "graphics/render/render_state.h"
#include "graphics/render/material/MaterialGameFiles.h"
#include "StudioMaterialProviders.h"
#include "MaterialOwnerFiles.h"
#include "graphics/render/material/material_manifests.h"
#include "graphics/render/shader_manager.h"
#include "graphics/render/model/studio_model_cache.h"
#include "graphics/render/RenderCaptureSession.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/render_device.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/material/StudioProgramContract.h"
#include "console/console.h"
#include "performance/PerformanceMonitor.h"

INITIALIZE_EASYLOGGINGPP

namespace
{
    enum class OutputFailure
    {
        None,
        Write,
        Completion,
        Replacement
    };

    class FailingMaterialGameFiles : public render::MaterialGameFiles
    {
    public:
        OutputFailure failure{};

    protected:
        bool WriteOutput(std::ofstream& file, const std::string& text) override
        {
            if (failure == OutputFailure::Write)
            {
                file.write(text.data(), 3);
                file.setstate(std::ios::badbit);
                return false;
            }

            return MaterialGameFiles::WriteOutput(file, text);
        }

        bool CompleteOutput(std::ofstream& file) override
        {
            const bool completed = MaterialGameFiles::CompleteOutput(file);
            return failure != OutputFailure::Completion && completed;
        }

        bool ReplaceOutput(const std::filesystem::path& temporary, const std::filesystem::path& destination) override
        {
            return failure != OutputFailure::Replacement && MaterialGameFiles::ReplaceOutput(temporary, destination);
        }
    };

    class NativeMaterialWriteTest : public testing::Test
    {
    protected:
        FailingMaterialGameFiles files_;
        std::filesystem::path directory_;
        const std::string original_ = R"({"version":1,"materials":{"unselected.bmp":{"program":"studio_legacy"}}})";
        const std::string replacement_ = R"({"version":1,"materials":{"selected.bmp":{"program":"studio_legacy"}}})";

        void SetUp() override
        {
            directory_ = std::filesystem::temp_directory_path() /
                         ("nextclient-material-write-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            ASSERT_TRUE(std::filesystem::create_directory(directory_));
            files_.SetGameDirectory(directory_.string().c_str());
            std::ofstream original(directory_ / "authored.mat", std::ios::binary);
            original << original_;
            original.close();
            ASSERT_TRUE(original);
        }

        void TearDown() override
        {
            std::error_code code;
            std::filesystem::remove_all(directory_, code);
        }

        std::string ReadAuthored()
        {
            std::ifstream file(directory_ / "authored.mat", std::ios::binary);
            return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        }

        void ExpectOnlyAuthoredFile()
        {
            std::vector<std::filesystem::path> names;
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory_))
            {
                names.push_back(entry.path().filename());
            }

            EXPECT_EQ(names, std::vector<std::filesystem::path>{"authored.mat"});
        }

        void ExpectFailurePreservesDocument(OutputFailure failure)
        {
            files_.failure = failure;
            std::string error;
            EXPECT_FALSE(files_.Write("authored.mat", replacement_, error));
            EXPECT_FALSE(error.empty());
            EXPECT_EQ(ReadAuthored(), original_);
            ExpectOnlyAuthoredFile();
        }
    };
} // namespace

TEST_F(NativeMaterialWriteTest, ImmediateWriteFailurePreservesDocumentAndCleansTemporaryOutput)
{
    ExpectFailurePreservesDocument(OutputFailure::Write);
}

TEST_F(NativeMaterialWriteTest, CompletionFailurePreservesDocumentAndCleansTemporaryOutput)
{
    ExpectFailurePreservesDocument(OutputFailure::Completion);
}

TEST_F(NativeMaterialWriteTest, ReplacementFailurePreservesDocumentAndCleansTemporaryOutput)
{
    ExpectFailurePreservesDocument(OutputFailure::Replacement);
}

TEST_F(NativeMaterialWriteTest, SuccessfulReplacementPublishesCompleteDocumentAndCleansTemporaryOutput)
{
    std::string error;
    ASSERT_TRUE(files_.Write("authored.mat", replacement_, error)) << error;
    EXPECT_EQ(ReadAuthored(), replacement_);
    ExpectOnlyAuthoredFile();
}

TEST(MaterialOwnerTest, IndependentPresetTablesKeepValuesFailuresAndGenerations)
{
    StudioMaterialSourceFixture first_source;
    StudioMaterialSourceFixture second_source;
    ncl_material::ProgramManifest manifest;
    manifest.program = "test";
    ncl_material::ParamSchema value;
    value.name = "uValue";
    value.instance = true;
    manifest.parameters.push_back(value);
    first_source.manifests["test"] = manifest;
    second_source.manifests["test"] = manifest;
    MaterialOwnerFiles first_files;
    MaterialOwnerFiles second_files;
    first_files.files["owned.mat"] = R"({"version":1,"materials":[
        {"name":"owned","texture":"base.bmp","material":{"program":"test","params":{"uValue":1}}}
    ]})";
    second_files.files["owned.mat"] = R"({"version":1,"materials":[
        {"name":"owned","texture":"base.bmp","material":{"program":"test","params":{"uValue":2}}}
    ]})";
    render::MaterialDocumentStore first_documents(first_files);
    render::MaterialDocumentStore second_documents(second_files);
    render::ServerMaterials first(first_documents, first_source.catalog);
    render::ServerMaterials second(second_documents, second_source.catalog);
    const render::ServerMaterialBindingOwner owner{7, 10};
    first.SetBinding(owner, "owned.mat", "owned");
    second.SetBinding(owner, "owned.mat", "owned");
    const ncl_material::Material* first_material = first.BindingFor({7, "base.bmp"}, true);
    const ncl_material::Material* second_material = second.BindingFor({7, "base.bmp"}, true);
    ASSERT_NE(first_material, nullptr);
    ASSERT_NE(second_material, nullptr);
    ASSERT_NE(first_material->FindParam("uValue"), nullptr);
    ASSERT_NE(second_material->FindParam("uValue"), nullptr);
    EXPECT_FLOAT_EQ(first_material->FindParam("uValue")->scalar, 1.0f);
    EXPECT_FLOAT_EQ(second_material->FindParam("uValue")->scalar, 2.0f);
    EXPECT_EQ(first_files.reads.size(), 1u);
    EXPECT_EQ(second_files.reads.size(), 1u);
    EXPECT_EQ(first_files.limits.front(), 1u << 20);
    const unsigned int second_generation = second.get_generation();

    first.Clear();
    EXPECT_EQ(first.BindingFor({7, "base.bmp"}, true), nullptr);
    EXPECT_EQ(second.BindingFor({7, "base.bmp"}, true), second_material);
    EXPECT_EQ(second.get_generation(), second_generation);
    first.SetBinding(owner, "missing.mat", "owned");
    const size_t after_failure = first_files.reads.size();
    first.SetBinding(owner, "MISSING.MAT", "owned");
    EXPECT_EQ(first_files.reads.size(), after_failure);
    EXPECT_EQ(second_files.reads.size(), 1u);
}

TEST(MaterialOwnerTest, DocumentReadsHaveNoServerPresetByteLimit)
{
    MaterialOwnerFiles files;
    files.files["large.mat"] = std::string((1u << 20) + 1, ' ');
    render::MaterialDocumentStore documents(files);
    std::string text;
    ASSERT_TRUE(documents.Read("large.mat", text));
    EXPECT_EQ(text.size(), (1u << 20) + 1);
    EXPECT_EQ(files.limits.back(), 0u);
    EXPECT_FALSE(documents.Read("large.mat", text, 1u << 20));
}

TEST(MaterialOwnerTest, FailedDocumentReadPreservesAuthoredEntriesAndDoesNotReload)
{
    for (render::MaterialReadStatus status : {render::MaterialReadStatus::ReadFailed, render::MaterialReadStatus::TooLarge})
    {
        SCOPED_TRACE(static_cast<int>(status));
        StudioMaterialSourceFixture source;
        MaterialOwnerFiles files;
        const std::string original = R"({"version":1,"materials":{
            "first.bmp":{"program":"studio_legacy"},
            "unselected.bmp":{"program":"studio_legacy"}
        }})";
        files.files["models/read_failure.mat"] = original;
        render::RendererResources resources(source, source, source);
        performance_test::Core performance;
        render::MaterialDocumentStore documents(files);
        render::ServerMaterials server(documents, resources.manifests);
        render::StudioModelCache models(resources.device, documents, resources.manifests, resources.shaders, server, performance.monitor);
        render::MaterialEditor editor(models, server, resources.shaders, resources.manifests, documents);
        studio_test::ModelBlock block;
        goldsrc::model_t model{};
        std::strcpy(model.name, "models/read_failure.mdl");
        std::strcpy(block.textures[0].name, "first.bmp");
        models.PrepareMetadata(&model, &block.header, &block.header, &model);
        ASSERT_TRUE(editor.Begin("models/read_failure.mdl", "first.bmp"));
        files.read_statuses["models/read_failure.mat"] = status;
        files.reads.clear();
        char error[512]{};

        EXPECT_FALSE(editor.Save(nullptr, error, sizeof(error)));
        EXPECT_NE(std::string(error).find("models/read_failure.mat"), std::string::npos);
        EXPECT_TRUE(files.writes.empty());
        ncl_material::MaterialOrigin origin = ncl_material::MaterialOrigin::Implicit;
        ASSERT_NE(models.ResolveMaterialByName("models/read_failure.mdl", "first.bmp", &origin), nullptr);
        EXPECT_EQ(origin, ncl_material::MaterialOrigin::ModelDocument);
        EXPECT_EQ(files.reads, std::vector<std::string>{"models/read_failure.mat"});
        EXPECT_EQ(files.files.at("models/read_failure.mat"), original);
    }
}

TEST(MaterialOwnerTest, ConsoleSelectionUsesTheSuppliedNetworkState)
{
    StudioMaterialSourceFixture source;
    ncl_material::ProgramManifest manifest;
    manifest.program = "test";
    source.manifests["test"] = manifest;
    MaterialOwnerFiles files;
    files.files["owned.mat"] = R"({"version":1,"materials":[
        {"name":"console","texture":"base.bmp","material":{"program":"test"}},
        {"name":"synchronized","texture":"base.bmp","material":{"program":"test"}}
    ]})";
    render::MaterialDocumentStore documents(files);
    render::ServerMaterials server(documents, source.catalog);
    server.SetBinding(7, "owned.mat", "console", false);
    const ncl_material::Material* console = server.BindingFor({7, "base.bmp"}, false);
    ASSERT_NE(console, nullptr);
    EXPECT_EQ(server.BindingFor({7, "base.bmp"}, true), nullptr);
    server.SetBinding(render::ServerMaterialBindingOwner{7, 10}, "owned.mat", "synchronized");
    EXPECT_EQ(server.BindingFor({7, "base.bmp"}, false), console);
    ASSERT_NE(server.BindingFor({7, "base.bmp"}, true), nullptr);
    EXPECT_NE(server.BindingFor({7, "base.bmp"}, true), console);
}

TEST(MaterialOwnerTest, SynchronizedPolicyUsesTheInjectedCatalogContract)
{
    std::ifstream source_file(NEXTCLIENT_STUDIO_MANIFEST_PATH, std::ios::binary);
    const std::string json((std::istreambuf_iterator<char>(source_file)), std::istreambuf_iterator<char>());
    ncl_utils::ResultT<ncl_material::ProgramManifest> parsed = ncl_material::ParseProgramManifest(json);
    ASSERT_FALSE(parsed.has_error()) << parsed.error_str();
    ncl_material::ProgramManifest restricted = *parsed;
    restricted.program = "test";
    for (ncl_material::ParamSchema& schema : restricted.parameters)
    {
        if (schema.name == "uOccludedAmount")
        {
            schema.default_value.scalar = 1;
        }
        if (schema.name == "uOutlineWidth")
        {
            schema.default_value.scalar = 2;
        }
    }

    ncl_material::ProgramManifest ordinary = restricted;
    ordinary.render_contract.clear();
    for (ncl_material::ParamSchema& schema : ordinary.parameters)
    {
        if (schema.name == "uOccludedAmount")
        {
            schema.default_value.scalar = 0.5f;
        }
        if (schema.name == "uOutlineWidth")
        {
            schema.default_value.scalar = 3;
        }
    }

    StudioMaterialSourceFixture first_source;
    StudioMaterialSourceFixture second_source;
    first_source.manifests["test"] = restricted;
    second_source.manifests["test"] = ordinary;
    MaterialOwnerFiles files;
    files.files["owned.mat"] = R"({"version":1,"materials":[
        {"name":"owned","texture":"base.bmp","material":{"program":"test"}}
    ]})";
    render::MaterialDocumentStore documents(files);
    render::ServerMaterials first(documents, first_source.catalog);
    render::ServerMaterials second(documents, second_source.catalog);
    const render::ServerMaterialBindingOwner owner{7, 10};
    first.SetBinding(owner, "owned.mat", "owned");
    second.SetBinding(owner, "owned.mat", "owned");
    for (int replacement = 0; replacement < 2; replacement++)
    {
        const ncl_material::Material* first_material = first.BindingFor({7, "base.bmp"}, true);
        const ncl_material::Material* second_material = second.BindingFor({7, "base.bmp"}, true);
        ASSERT_NE(first_material, nullptr);
        ASSERT_NE(second_material, nullptr);
        EXPECT_FLOAT_EQ(first_material->FindParam("uOccludedAmount")->scalar, 0);
        EXPECT_FLOAT_EQ(first_material->FindParam("uOutlineWidth")->scalar, 0);
        EXPECT_FLOAT_EQ(second_material->FindParam("uOccludedAmount")->scalar, 0.5f);
        EXPECT_FLOAT_EQ(second_material->FindParam("uOutlineWidth")->scalar, 3);
        ASSERT_TRUE(first.ReplaceOwnerParams(owner, {}));
        ASSERT_TRUE(second.ReplaceOwnerParams(owner, {}));
    }
}

namespace
{
    constexpr uint16_t kEntityIndex = 1;
    constexpr uint16_t kMaterials = 2;
    constexpr uint16_t kParams = 4;

    const std::string kPreset = R"({"version":1,"materials":[
        {"name":"first","texture":"first.bmp","material":{"program":"test"}},
        {"name":"second","texture":"second.bmp","material":{"program":"test"}}
    ]})";

    ClientStringRegistry g_Strings;

    void ForgetString(uint16_t id)
    {
        const auto entries = g_Strings.get_entries();
        g_Strings.Clear();
        for (const auto& [key, value] : entries)
        {
            if (key != id)
            {
                g_Strings.Register(key, value);
            }
        }
    }
    std::map<std::string, goldsrc::xcommand_t> g_Commands;
    std::map<goldsrc::cvar_t*, std::string> g_CvarStrings;
    std::vector<std::string> g_Arguments;
    ncl_material::ProgramManifest g_Manifest;
    bool g_CaptureSucceeds = true;
    std::map<std::string, std::string> g_Documents;
    std::string g_FailedOpenPath;
    int g_FailedOpenAttempts{};
    std::map<std::string, ncl_material::Material> g_EditorMaterials;

    class LifecycleMaterialBackend : public StudioMaterialSourceFixture
    {
    public:
        bool LoadSource(const char* name, std::string& out) override
        {
            manifests[g_Manifest.program] = g_Manifest;
            return StudioMaterialSourceFixture::LoadSource(name, out);
        }
    };

    struct MaterialOwners
    {
        LifecycleMaterialBackend backend;
        render::RendererResources resources{backend, backend, backend};
        render::MaterialGameFiles files;
        struct State
        {
            performance_test::Core performance;
            render::MaterialDocumentStore documents;
            render::ServerMaterials server_materials;
            render::StudioModelCache models;
            render::MaterialEditor editor;

            State(render::RendererResources& resources, render::MaterialGameFiles& files) :
                documents(files),
                server_materials(documents, resources.manifests),
                models(resources.device, documents, resources.manifests, resources.shaders, server_materials, performance.monitor),
                editor(models, server_materials, resources.shaders, resources.manifests, documents)
            {}
        } state{resources, files};
        studio_test::ModelBlock block;
        goldsrc::model_t model{};

        MaterialOwners()
        {
            files.SetGameDirectory(".");
            std::strcpy(block.textures[0].name, "first.bmp");
            std::strcpy(block.textures[1].name, "second.bmp");
            for (const char* name : {"models/shared.mdl", "models/test.mdl", "models/mixed.mdl", "models/replacement.mdl"})
            {
                std::strcpy(model.name, name);
                state.models.PrepareMetadata(&model, &block.header, &block.header, &model);
            }
        }
    };

    std::vector<std::string> ParameterNames(const ncl_material::Material& material)
    {
        std::vector<std::string> names;
        for (const ncl_material::Param& param : material.params)
        {
            names.push_back(param.name);
        }
        return names;
    }

    std::unique_ptr<MaterialOwners> g_MaterialOwners;

    void ResetMaterialOwners()
    {
        g_MaterialOwners = std::make_unique<MaterialOwners>();
    }

    void InstallStudioContract(float occluded_default = 0, float outline_default = 0)
    {
        std::ifstream source(NEXTCLIENT_STUDIO_MANIFEST_PATH, std::ios::binary);
        const std::string json((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
        ncl_utils::ResultT<ncl_material::ProgramManifest> parsed = ncl_material::ParseProgramManifest(json);
        if (parsed.has_error())
        {
            throw std::runtime_error(parsed.error_str());
        }

        g_Manifest = *parsed;
        g_Manifest.program = "test";
        for (ncl_material::ParamSchema& schema : g_Manifest.parameters)
        {
            if (schema.name == "uOccludedAmount")
            {
                schema.default_value.scalar = occluded_default;
            }
            if (schema.name == "uOutlineWidth")
            {
                schema.default_value.scalar = outline_default;
            }
        }
    }

    char* Print(const char*, ...)
    {
        return nullptr;
    }

    class ConsoleHook : public nitroapi::GenericHookInterface<char*, const char*>
    {
    public:
        void UnsetCallbackAndDisableHook() override {}
        uint32_t GetTrampolineAddress() const override
        {
            return GetOriginalFunctionAddress();
        }
        uint32_t GetOriginalFunctionAddress() const override
        {
            return reinterpret_cast<uint32_t>(Print);
        }
        bool IsHookActive() const override
        {
            return false;
        }
        void SetCallbackAndEnableHook(std::function<char*(const char*)>&&) override {}
        char* InvokeOriginal(const char*) override
        {
            return nullptr;
        }
        char* Invoke(const char*) override
        {
            return nullptr;
        }
    };

    class ConsoleChain : public nitroapi::HandlersChainInterface<char*, const char*>
    {
    public:
        void DetachHook() override {}
        bool IsHookAttached() override
        {
            return false;
        }
        uint32_t GetSubscribersCount() const override
        {
            return 0;
        }
        uint32_t GetHandlersCount() const override
        {
            return 0;
        }
        void AttachHook(nitroapi::GenericHookInterface<char*, const char*>*) override {}
        nitroapi::GenericHookInterface<char*, const char*>* GetHook() const override
        {
            return nullptr;
        }
        bool AddHandler(nitroapi::HandlerInterface<char*, const char*>*) override
        {
            return false;
        }
        bool RemoveHandler(nitroapi::HandlerInterface<char*, const char*>*) override
        {
            return false;
        }
        bool Subscribe(nitroapi::SubscriberInterface<char*, const char*>*) override
        {
            return false;
        }
        bool Unsubscribe(nitroapi::SubscriberInterface<char*, const char*>*) override
        {
            return false;
        }
    };

    class TestEngine : public nitroapi::NitroApiInterface
    {
    private:
        nitroapi::EngineData data_;
        ConsoleHook console_;
        ConsoleChain console_chain_;

    public:
        TestEngine()
        {
            data_.Con_Printf.SetChain(&console_chain_);
            data_.Con_Printf.SetHook(&console_);
        }

        void WriteLog(const char*, const char*) override {}
        void SetSEHCallback(GlobalExceptionHandlerFunc) override {}
        GlobalExceptionHandlerFunc GetSEHCallback() override
        {
            return nullptr;
        }
        bool Initialize(ICommandLine*, IFileSystem*, IRegistry*) override
        {
            return false;
        }
        bool UnInitialize() override
        {
            return true;
        }
        bool IsInitialized() override
        {
            return false;
        }
        nitroapi::ClientData* GetClientData() override
        {
            return nullptr;
        }
        nitroapi::EngineData* GetEngineData() override
        {
            return &data_;
        }
        nitroapi::WindowsData* GetWindowsData() override
        {
            return nullptr;
        }
        nitroapi::SDL2Data* GetSDL2Data() override
        {
            return nullptr;
        }
        nitroapi::BuildVersion* GetBuildVersion() override
        {
            return nullptr;
        }
        void GetVersion(char*, int) override {}
    };

    TestEngine g_TestEngine;

    NclEntityFields Fields(uint32_t index, uint16_t material, float value)
    {
        ncl_entity::ArrayStringIdPairsDelta materials;
        materials.added.emplace_back(1, material);
        ncl_entity::ArrayStringIdFloatPairsDelta params;
        params.added.emplace_back(4, value);

        return {{kEntityIndex, index}, {kMaterials, materials}, {kParams, params}};
    }

    void ExpectWear(int index, const char* texture, float expected)
    {
        const ncl_material::Material* material = render::ServerMaterials_BindingFor({index, texture});
        ASSERT_NE(material, nullptr);
        const ncl_material::ParamValue* wear = material->FindParam("uWear");
        ASSERT_NE(wear, nullptr);
        EXPECT_FLOAT_EQ(wear->scalar, expected) << texture;
    }

    class MaterialSyncTest : public testing::Test
    {
    protected:
        std::unique_ptr<MaterialSyncSystem> sync_;

        void SetUp() override
        {
            ResetMaterialOwners();
            g_Strings.Clear();
            g_Strings.Register(1, "test.mat");
            g_Strings.Register(2, "first");
            g_Strings.Register(3, "second");
            g_Strings.Register(4, "uWear");
            sync_ = std::make_unique<MaterialSyncSystem>(g_MaterialOwners->state.server_materials, g_Strings);
            g_Manifest = ncl_material::ProgramManifest{};
            g_Manifest.program = "test";
            ncl_material::ParamSchema wear;
            wear.name = "uWear";
            wear.instance = true;
            g_Manifest.parameters.push_back(wear);
            ncl_material::ParamSchema other;
            other.name = "uOther";
            g_Manifest.parameters.push_back(other);
            g_Documents.clear();
            g_FailedOpenPath.clear();
            g_FailedOpenAttempts = 0;
            g_EditorMaterials.clear();
        }

        void TearDown() override
        {
            cls = nullptr;
            render::MaterialEdit_Get()->End();
            sync_->OnClear();
            render::ServerMaterials_Clear();
        }
    };

    const char* LevelName()
    {
        return "maps/test.bsp";
    }

    const char* GameDirectory()
    {
        return ".";
    }

    class CaptureBackend final : public render::RenderCaptureBackend
    {
    public:
        void SetCvar(goldsrc::cvar_t* cvar, const char* value) override
        {
            Cvar_DirectSet(cvar, value);
        }
        bool Capture(const char* name, const char* suffix, std::filesystem::path& path) override
        {
            const ncl_gl::FrameImage image = ncl_gl::CaptureFramebuffer();
            path = std::string(name) + suffix + ".tga";
            return image.is_valid() && ncl_gl::WriteTga(path, image);
        }
        void ReportCompleted(const char*, const std::filesystem::path&) override {}
    };

    class RenderCaptureTest : public testing::Test
    {
    protected:
        CaptureBackend backend_;
        render::RenderCaptureSession capture_{backend_};
        goldsrc::cvar_t shader_{};
        goldsrc::cvar_t materials_{};

        void SetUp() override
        {
            g_CaptureSucceeds = true;
            shader_.name = "r_studio_shader";
            materials_.name = "r_studio_materials";
            Cvar_DirectSet(&shader_, "2.500");
            Cvar_DirectSet(&materials_, "0.7500");
            render::g_RenderCfg.studio_shader = &shader_;
            render::g_RenderCfg.studio_materials = &materials_;
            gEngfuncs.pfnGetLevelName = LevelName;
            gEngfuncs.pfnGetGameDirectory = GameDirectory;
            render::Renderer_InitializeMaterialFiles(GameDirectory());
        }

        void TearDown() override
        {
            capture_.Cancel();
            render::g_RenderCfg = render::RenderConfig{};
            g_CvarStrings.clear();
        }

        void Start()
        {
            capture_.Begin("studio_ab", *render::g_RenderCfg.studio_shader, render::g_RenderCfg.studio_materials);
        }

        void ExpectRestored()
        {
            EXPECT_STREQ(shader_.string, "2.500");
            EXPECT_STREQ(materials_.string, "0.7500");
        }
    };
} // namespace

nitroapi::NitroApiInterface* g_NitroApi = &g_TestEngine;
goldsrc::cl_enginefunc_t gEngfuncs{};
goldsrc::client_static_t* cls{};
render::RenderConfig render::g_RenderCfg{};

nitroapi::EngineData* eng()
{
    return g_TestEngine.GetEngineData();
}

std::string InitialModelDocument(const char* model)
{
    static const ncl_material::Material resource = []() {
        ncl_material::Material material;
        material.program = "test";
        ncl_material::ParamValue value;
        value.scalar = 0.25f;
        material.params.push_back({"uWear", value});
        value.scalar = 2;
        material.params.push_back({"uOther", value});

        return material;
    }();

    static const ncl_material::Material second = []() {
        ncl_material::Material material = resource;
        material.params.front().value.scalar = .75f;
        material.params.back().value.scalar = 3;
        return material;
    }();
    static const ncl_material::Material legacy = []() {
        ncl_material::Material material;
        material.program = "studio_legacy";
        return material;
    }();
    ncl_material::MaterialDoc document;
    document.entries.push_back({"first.bmp", resource});
    document.entries.push_back({"second.bmp", std::strcmp(model, "models/mixed.mat") == 0 ? legacy : second});
    for (ncl_material::MaterialEntry& entry : document.entries)
    {
        auto found = g_EditorMaterials.find(entry.texture_name);
        if (found != g_EditorMaterials.end())
        {
            entry.material = found->second;
        }
    }
    for (ncl_material::MaterialEntry& entry : document.entries)
    {
        for (ncl_material::Param& param : entry.material.params)
        {
            const ncl_material::ParamSchema* schema = g_Manifest.FindParam(param.name);
            if (schema != nullptr && schema->widget == ncl_material::ParamWidget::Choice)
            {
                param.value.scalar = schema->default_value.scalar;
            }
        }
    }

    return ncl_material::WriteMaterialDoc(document);
}

FileHandle_t FS_Open(const char* path, const char*)
{
    if (g_FailedOpenPath == path)
    {
        g_FailedOpenAttempts++;
        return nullptr;
    }

    if (std::strcmp(path, "test.mat") == 0)
    {
        return const_cast<std::string*>(&kPreset);
    }
    auto found = g_Documents.find(path);
    if (found != g_Documents.end())
    {
        return &found->second;
    }
    if (std::string_view(path).starts_with("models/"))
    {
        static std::string initial;
        initial = InitialModelDocument(path);
        return &initial;
    }
    return nullptr;
}

bool FS_FileExists(const char* path)
{
    return std::strcmp(path, "test.mat") == 0 || g_Documents.contains(path) || std::string_view(path).starts_with("models/");
}

unsigned int FS_Size(FileHandle_t file)
{
    return static_cast<unsigned int>(static_cast<const std::string*>(file)->size());
}

int FS_Read(void* output, int size, FileHandle_t file)
{
    std::memcpy(output, static_cast<const std::string*>(file)->data(), size);

    return size;
}

void FS_Close(FileHandle_t) {}

void Con_DPrintf(ConLogType, const char*, ...) {}

const char* render::Device_GlString(unsigned int name, const char* fallback)
{
    const GLubyte* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : fallback;
}

render::MaterialEditor& render::GetMaterialEditor()
{
    if (!g_MaterialOwners)
    {
        ResetMaterialOwners();
    }
    return g_MaterialOwners->state.editor;
}

render::ServerMaterials& render::GetServerMaterials()
{
    GetMaterialEditor();
    return g_MaterialOwners->state.server_materials;
}

render::MaterialDocumentStore& render::GetMaterialDocuments()
{
    GetMaterialEditor();
    return g_MaterialOwners->state.documents;
}

void render::Renderer_InitializeMaterialFiles(const char* directory)
{
    GetMaterialEditor();
    g_MaterialOwners->files.SetGameDirectory(directory);
}

const ncl_material::ProgramManifest* render::Manifest_Get(const std::string& program)
{
    GetMaterialEditor();
    return g_MaterialOwners->resources.manifests.Get(program);
}

const ncl_material::ParamValue* render::Manifest_GetParam(const ncl_material::Material* material, std::string_view name)
{
    GetMaterialEditor();
    return g_MaterialOwners->resources.manifests.GetParam(material, name);
}

void Cmd_AddCommand(const char* name, goldsrc::xcommand_t command)
{
    g_Commands[name] = command;
}

int Cmd_Argc()
{
    return static_cast<int>(g_Arguments.size());
}

const char* Cmd_Argv(int index)
{
    return g_Arguments.at(index).c_str();
}

void Cvar_DirectSet(goldsrc::cvar_t* cvar, const char* value)
{
    std::string& stored = g_CvarStrings[cvar];
    stored = value;
    cvar->string = stored.data();
    cvar->value = std::strtof(value, nullptr);
}

bool render::Device_CheckAvailable()
{
    return true;
}

ncl_gl::FrameImage ncl_gl::CaptureFramebuffer()
{
    return {1, 1, {0, 0, 0}};
}

bool ncl_gl::WriteTga(const std::filesystem::path&, const ncl_gl::FrameImage&)
{
    return g_CaptureSucceeds;
}

TEST_F(MaterialSyncTest, SharedPlayerIndexRetainsEachOwnersUnqualifiedParameters)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));

    ExpectWear(7, "first.bmp", 0.5f);
    ExpectWear(7, "second.bmp", 1.5f);
}

TEST_F(RenderCaptureTest, CompletedComparisonRestoresExactCvarStrings)
{
    Start();
    capture_.OnSceneEnd();
    capture_.OnSceneEnd();

    ExpectRestored();
}

TEST_F(MaterialSyncTest, ParameterUpdateChangesOnlyTheSynchronizedOwner)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));

    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added.emplace_back(4, -0.75f);
    sync_->OnUpdate(10, kParams, {{kParams, params}});

    ExpectWear(7, "first.bmp", -0.75f);
    ExpectWear(7, "second.bmp", 1.5f);
}

TEST_F(MaterialSyncTest, ParameterUpdatePreservesUnrelatedOwnerInstance)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "second.bmp/uWear", 2.25f));

    const ncl_material::Material* untouched = render::ServerMaterials_BindingFor({7, "second.bmp"});
    ASSERT_NE(untouched, nullptr);
    ExpectWear(7, "second.bmp", 2.25f);

    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added.emplace_back(4, -0.75f);
    sync_->OnUpdate(10, kParams, {{kParams, params}});

    ExpectWear(7, "first.bmp", -0.75f);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "second.bmp"}), untouched);
    ExpectWear(7, "second.bmp", 2.25f);
}

TEST_F(MaterialSyncTest, DestroyingOneOwnerPreservesTheOtherOwnersBindingsAndParameters)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));
    sync_->OnDestroy(20);

    ExpectWear(7, "first.bmp", 0.5f);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "second.bmp"}), nullptr);
}

TEST_F(MaterialSyncTest, HiddenOwnerParameterUpdatePreservesWinnerAndAppearsAfterRemoval)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 2, 1.5f));
    ASSERT_TRUE(render::ServerMaterials_SetParam(render::ServerMaterialBindingOwner{7, 20}, "uWear", 2.25f));
    const ncl_material::Material* winner = render::ServerMaterials_BindingFor({7, "first.bmp"});

    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added.emplace_back(4, -0.75f);
    sync_->OnUpdate(10, kParams, {{kParams, params}});

    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "first.bmp"}), winner);
    ExpectWear(7, "first.bmp", 2.25f);

    sync_->OnDestroy(20);
    ExpectWear(7, "first.bmp", -0.75f);
}

TEST_F(MaterialSyncTest, BindingReplacementPreservesUnrelatedOwnerInstance)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "second.bmp/uWear", 2.25f));
    const ncl_material::Material* untouched = render::ServerMaterials_BindingFor({7, "second.bmp"});

    ncl_entity::ArrayStringIdPairsDelta materials;
    materials.added.emplace_back(1, 3);
    sync_->OnUpdate(10, kMaterials, {{kMaterials, materials}});

    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "second.bmp"}), untouched);
    ExpectWear(7, "second.bmp", 2.25f);

    sync_->OnDestroy(10);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "second.bmp"}), untouched);
    ExpectWear(7, "second.bmp", 2.25f);
}

TEST_F(MaterialSyncTest, RepeatedCreationMovesOnlyTheRecordedOwner)
{
    render::ServerMaterials_SetBinding(7, "test.mat", "second");
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "second.bmp/uWear", 2.25f));
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 2, 1.0f));
    sync_->OnCreate(10, Fields(8, 2, 1.5f));

    ExpectWear(7, "first.bmp", 1.0f);
    ExpectWear(7, "second.bmp", 2.25f);
    ExpectWear(8, "first.bmp", 1.5f);

    sync_->OnCreate(10, Fields(8, 3, 0.75f));
    EXPECT_EQ(render::ServerMaterials_BindingFor({8, "first.bmp"}), nullptr);
    ExpectWear(8, "second.bmp", 0.75f);

    sync_->OnDestroy(10);
    sync_->OnDestroy(10);
    ExpectWear(7, "first.bmp", 1.0f);
    ExpectWear(7, "second.bmp", 2.25f);
    EXPECT_EQ(render::ServerMaterials_BindingFor({8, "second.bmp"}), nullptr);
}

TEST_F(MaterialSyncTest, ClearRemovesOnlyRecordedOwnersAndPreservesConsoleState)
{
    render::ServerMaterials_SetBinding(7, "test.mat", "second");
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "second.bmp/uWear", 2.25f));
    render::ServerMaterials_SetBinding(render::ServerMaterialBindingOwner{8, 99}, "test.mat", "first");
    ASSERT_TRUE(render::ServerMaterials_SetParam(render::ServerMaterialBindingOwner{8, 99}, "uWear", 0.75f));
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(8, 3, 1.5f));

    for (int clear = 0; clear < 2; clear++)
    {
        sync_->OnClear();
        EXPECT_EQ(render::ServerMaterials_BindingFor({7, "first.bmp"}), nullptr);
        EXPECT_EQ(render::ServerMaterials_BindingFor({8, "second.bmp"}), nullptr);
        ExpectWear(7, "second.bmp", 2.25f);
        ExpectWear(8, "first.bmp", 0.75f);
    }
}

TEST_F(MaterialSyncTest, ParameterUpdateKeepsPublishedBindingNamesAfterRegistryMutation)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "second.bmp/uWear", 2.25f));
    const ncl_material::Material* untouched = render::ServerMaterials_BindingFor({7, "second.bmp"});
    const unsigned int generation = render::ServerMaterials_Generation();

    g_Strings.Register(1, "missing.mat");
    g_Strings.Register(2, "missing");
    ForgetString(3);
    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added.emplace_back(4, -0.75f);
    sync_->OnUpdate(10, kParams, {{kParams, params}});

    ExpectWear(7, "first.bmp", -0.75f);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "second.bmp"}), untouched);
    ExpectWear(7, "second.bmp", 2.25f);
    EXPECT_EQ(render::ServerMaterials_Generation(), generation);
}

TEST_F(MaterialSyncTest, OwnerSnapshotsCopyCallerStringsAndEmptyParamsRestorePresetDefaults)
{
    render::ServerMaterials_SetBinding(7, "test.mat", "second");
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "second.bmp/uWear", 2.25f));
    g_Documents["owned.mat"] = R"({"version":1,"materials":[
        {"name":"owned","texture":"owned.bmp","material":{"program":"test","params":{"uWear":0.75}}}
    ]})";
    const render::ServerMaterialBindingOwner owner{7, 0};
    std::vector<render::ServerMaterialBinding> bindings = {{"owned.mat", "owned"}};
    std::vector<render::ServerMaterialParam> params = {{"uWear", 1.5f}};
    render::ServerMaterials_ReplaceOwner(owner, bindings, params);
    bindings.front() = {"missing.mat", "missing"};
    params.front() = {"missing", 1};
    ExpectWear(7, "owned.bmp", 1.5f);

    params = {{"uWear", -0.75f}};
    render::ServerMaterials_ReplaceOwnerParams(owner, params);
    params.clear();
    ExpectWear(7, "owned.bmp", -0.75f);

    render::ServerMaterials_ReplaceOwnerParams(owner, {});
    ExpectWear(7, "owned.bmp", 0.75f);
    render::ServerMaterials_RemoveOwner(owner);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "owned.bmp"}), nullptr);
    ExpectWear(7, "second.bmp", 2.25f);
}

TEST_F(MaterialSyncTest, RemovedAndClearedParamsRestorePresetDefaultsWithSignedClamping)
{
    g_Manifest.parameters.front().has_range = true;
    g_Manifest.parameters.front().min_value = -1.0f;
    g_Manifest.parameters.front().max_value = 2.0f;
    g_Manifest.parameters[1].instance = true;
    g_Strings.Register(5, "defaults.mat");
    g_Strings.Register(6, "defaults");
    g_Strings.Register(7, "uOther");
    g_Documents["defaults.mat"] = R"({"version":1,"materials":[
        {"name":"defaults","texture":"first.bmp","material":{"program":"test","params":{"uWear":0.75,"uOther":1.5}}}
    ]})";
    ncl_entity::ArrayStringIdPairsDelta materials;
    materials.added.emplace_back(5, 6);
    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added = {{4, -8192.0f}, {7, 2.5f}};
    sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, materials}, {kParams, params}});
    ExpectWear(7, "first.bmp", -1.0f);

    params.added = {{4, 8191.75f}};
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    ExpectWear(7, "first.bmp", 2.0f);

    params.added.clear();
    params.removed = {4};
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    ExpectWear(7, "first.bmp", 0.75f);
    const ncl_material::Material* material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("uOther"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("uOther")->scalar, 2.5f);

    params.removed.clear();
    params.clear = true;
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ASSERT_NE(material, nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("uOther")->scalar, 1.5f);
    ExpectWear(7, "first.bmp", 0.75f);
}

TEST_F(MaterialSyncTest, InvalidRenderedIndicesRetainWireSnapshotsForLaterValidMigration)
{
    for (uint32_t invalid : {uint32_t{0}, uint32_t{4096}, uint32_t{0x80000000}, uint32_t{0xffffffff}})
    {
        SCOPED_TRACE(invalid);
        sync_->OnCreate(10, Fields(invalid, 2, 0.5f));
        EXPECT_EQ(render::ServerMaterials_BindingFor({7, "first.bmp"}), nullptr);
        ncl_entity::ArrayStringIdFloatPairsDelta params;
        params.added.emplace_back(4, -0.75f);
        sync_->OnUpdate(10, kParams, {{kParams, params}});
        sync_->OnUpdate(10, kEntityIndex, {{kEntityIndex, uint32_t{7}}});
        ExpectWear(7, "first.bmp", -0.75f);

        sync_->OnUpdate(10, kEntityIndex, {{kEntityIndex, invalid}});
        EXPECT_EQ(render::ServerMaterials_BindingFor({7, "first.bmp"}), nullptr);
        sync_->OnDestroy(10);
    }
}

TEST_F(MaterialSyncTest, UpdatesWithoutChangedMaterialFieldsPreservePublishedInstance)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "uWear", 2.25f));
    const ncl_material::Material* material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ForgetString(1);
    ForgetString(2);

    sync_->OnUpdate(10, 0, {});
    sync_->OnUpdate(10, kMaterials | kParams, {});
    sync_->OnUpdate(10, kEntityIndex, {{kEntityIndex, uint32_t{7}}});

    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "first.bmp"}), material);
    ExpectWear(7, "first.bmp", 2.25f);
}

TEST_F(MaterialSyncTest, RebindingOwnerToAnotherPlayerRebuildsBothPlayerIndices)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));
    sync_->OnUpdate(20, kEntityIndex, {{kEntityIndex, uint32_t{8}}});

    ExpectWear(7, "first.bmp", 0.5f);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "second.bmp"}), nullptr);
    ExpectWear(8, "second.bmp", 1.5f);
}

TEST_F(MaterialSyncTest, TextureConflictUsesOwnerIdAndRestoresRemainingOwnerOnDestruction)
{
    sync_->OnCreate(20, Fields(7, 2, 1.5f));
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    ExpectWear(7, "first.bmp", 1.5f);

    sync_->OnDestroy(20);
    ExpectWear(7, "first.bmp", 0.5f);
}

TEST_F(MaterialSyncTest, LowerPriorityOwnerCannotOverrideTheSelectedOwnersParameters)
{
    const render::ServerMaterialBindingOwner first{7, 0};
    const render::ServerMaterialBindingOwner second{7, 20};
    render::ServerMaterials_SetBinding(second, "test.mat", "first");
    render::ServerMaterials_SetBinding(first, "test.mat", "first");
    ASSERT_TRUE(render::ServerMaterials_SetParam(second, "uWear", 1.5f));
    ASSERT_TRUE(render::ServerMaterials_SetParam(first, "uWear", 0.5f));

    ExpectWear(7, "first.bmp", 1.5f);
}

TEST_F(MaterialSyncTest, SharedTexturesDoNotConsumeAdditionalResolvedTextureSlots)
{
    for (uint16_t owner_id = 1; owner_id <= 20; owner_id++)
    {
        sync_->OnCreate(owner_id, Fields(7, 2, 0.5f));
    }

    sync_->OnCreate(21, Fields(7, 3, 1.5f));

    ExpectWear(7, "first.bmp", 0.5f);
    ExpectWear(7, "second.bmp", 1.5f);
}

TEST_F(MaterialSyncTest, ConsoleApiRetainsEntityWideParameterUpdatesAndBindingPriority)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "uWear", 1.0f));

    ExpectWear(7, "first.bmp", 1.0f);
    ExpectWear(7, "second.bmp", 1.0f);

    render::ServerMaterials_SetBinding(7, "test.mat", "first");
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "first.bmp/uWear", 0.75f));
    ExpectWear(7, "first.bmp", 0.75f);
    ExpectWear(7, "second.bmp", 1.0f);
}

TEST_F(RenderCaptureTest, CancelPendingOffRestoresCvarsAndMakesLaterSceneEndInert)
{
    Start();
    EXPECT_STREQ(shader_.string, "0");
    EXPECT_STREQ(materials_.string, "0");

    capture_.Cancel();
    ExpectRestored();
    capture_.OnSceneEnd();
    ExpectRestored();
}

TEST_F(RenderCaptureTest, CancelPendingOnRestoresCvars)
{
    Start();
    capture_.OnSceneEnd();
    ASSERT_STREQ(shader_.string, "1");
    ASSERT_STREQ(materials_.string, "0");

    capture_.Cancel();
    ExpectRestored();
}

TEST_F(RenderCaptureTest, CancelIdlePreservesCurrentSettings)
{
    Cvar_DirectSet(&shader_, "0.250");
    Cvar_DirectSet(&materials_, "3.000");
    capture_.Cancel();

    EXPECT_STREQ(shader_.string, "0.250");
    EXPECT_STREQ(materials_.string, "3.000");
}

TEST_F(RenderCaptureTest, CanceledCaptureCanRestartWithANewSettingsSnapshot)
{
    Start();
    capture_.Cancel();
    Cvar_DirectSet(&shader_, "0.250");
    Cvar_DirectSet(&materials_, "3.000");
    Start();
    capture_.OnSceneEnd();
    capture_.Cancel();
    capture_.Cancel();

    EXPECT_STREQ(shader_.string, "0.250");
    EXPECT_STREQ(materials_.string, "3.000");
}

TEST_F(RenderCaptureTest, RepeatedBeginCannotReplaceTheActiveSettingsSnapshot)
{
    Start();
    Start();
    capture_.Cancel();

    ExpectRestored();
}

TEST_F(RenderCaptureTest, FailedOffFrameRestoresCvars)
{
    g_CaptureSucceeds = false;
    Start();
    capture_.OnSceneEnd();

    ExpectRestored();
}

TEST_F(RenderCaptureTest, FailedOnFrameRestoresCvars)
{
    Start();
    capture_.OnSceneEnd();
    g_CaptureSucceeds = false;
    capture_.OnSceneEnd();

    ExpectRestored();
}

TEST_F(RenderCaptureTest, CancelRestoresTheCapturedCvarOwnerWhenConfigurationReferencesChange)
{
    Start();
    render::g_RenderCfg.studio_shader = nullptr;
    render::g_RenderCfg.studio_materials = nullptr;
    capture_.Cancel();

    ExpectRestored();
}

TEST_F(RenderCaptureTest, CaptureWithoutMaterialCvarRestoresShader)
{
    render::g_RenderCfg.studio_materials = nullptr;
    Start();
    capture_.OnSceneEnd();
    capture_.OnSceneEnd();

    ExpectRestored();
}

std::vector<std::string> render::Shaders_ListPrograms()
{
    return {"test"};
}

TEST_F(MaterialSyncTest, MaterialEditorUsesSelectedEntityForSharedModel)
{
    sync_->OnCreate(10, Fields(11, 2, 0.5f));
    sync_->OnCreate(20, Fields(12, 2, 1.5f));

    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    float wear = 0;

    EXPECT_EQ(editor->GetMaterialSource("models/shared.mdl", "first.bmp", 11), render::MaterialEditSource::Instance);
    ASSERT_TRUE(editor->Begin("models/shared.mdl", "first.bmp", 11));

    EXPECT_EQ(editor->get_edited_entity_index(), 11);

    ASSERT_TRUE(editor->get_scalar("uWear", &wear));
    EXPECT_FLOAT_EQ(wear, 0.5f);

    ASSERT_TRUE(editor->Begin("models/shared.mdl", "first.bmp", 12));

    EXPECT_EQ(editor->get_edited_entity_index(), 12);
    ASSERT_TRUE(editor->get_scalar("uWear", &wear));
    EXPECT_FLOAT_EQ(wear, 1.5f);
}

TEST_F(MaterialSyncTest, MaterialEditorResourceSourceSkipsInstanceBindings)
{
    sync_->OnCreate(10, Fields(11, 2, 0.5f));
    sync_->OnCreate(20, Fields(12, 2, 1.5f));

    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    float wear = 0;

    EXPECT_EQ(editor->GetMaterialSource("models/shared.mdl", "first.bmp"), render::MaterialEditSource::ModelDocument);
    ASSERT_TRUE(editor->Begin("models/shared.mdl", "first.bmp"));

    EXPECT_EQ(editor->get_edited_entity_index(), 0);

    ASSERT_TRUE(editor->get_scalar("uWear", &wear));
    EXPECT_FLOAT_EQ(wear, 0.25f);
}

TEST_F(MaterialSyncTest, MaterialOverrideStopsAsSoonAsAConnectionStarts)
{
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(editor->Begin("models/test.mdl", "first.bmp", 0));
    goldsrc::client_static_t client{};
    cls = &client;
    client.state = goldsrc::ca_disconnected;
    EXPECT_NE(render::MaterialEdit_Override("models/test.mdl", "first.bmp"), nullptr);
    client.state = goldsrc::ca_connecting;
    EXPECT_EQ(render::MaterialEdit_Override("models/test.mdl", "first.bmp"), nullptr);
    client.state = goldsrc::ca_active;
    EXPECT_EQ(render::MaterialEdit_Override("models/test.mdl", "first.bmp"), nullptr);
    cls = nullptr;
    editor->End();
}

TEST_F(MaterialSyncTest, BindingPredicateTracksCurrentValuesAndOwnerRemoval)
{
    const auto matches = [](const ncl_material::Material& material) {
        const ncl_material::ParamValue* wear = material.FindParam("uWear");
        return wear != nullptr && wear->scalar > 1.0f;
    };
    EXPECT_FALSE(render::ServerMaterials_AnyBindingMatches(7, matches));
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    EXPECT_FALSE(render::ServerMaterials_AnyBindingMatches(7, matches));
    sync_->OnCreate(20, Fields(7, 3, 1.5f));
    EXPECT_TRUE(render::ServerMaterials_AnyBindingMatches(7, matches));
    EXPECT_FALSE(render::ServerMaterials_AnyBindingMatches(8, matches));
    sync_->OnDestroy(20);
    EXPECT_FALSE(render::ServerMaterials_AnyBindingMatches(7, matches));
}

TEST_F(MaterialSyncTest, ChoiceMetadataExposesDeclaredLabelsAndRejectsInvalidLookups)
{
    const ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version":1,"program":"test","parameters":{
            "mode":{"type":"scalar","widget":"choice","choices":["#Mode_Off","#Mode_On"],
                "instance":true,"min":0,"max":1,"default":0},
            "amount":{"type":"scalar"}
        }
    })");
    ASSERT_FALSE(manifest.has_error()) << manifest.error_str();
    g_Manifest = *manifest;

    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->GetChoiceCount("test", "mode"), 2);
    EXPECT_STREQ(editor->GetChoiceLabel("test", "mode", 0), "#Mode_Off");
    EXPECT_STREQ(editor->GetChoiceLabel("test", "mode", 1), "#Mode_On");
    EXPECT_STREQ(editor->GetChoiceLabel("test", "mode", -1), "");
    EXPECT_STREQ(editor->GetChoiceLabel("test", "mode", 2), "");
    const char* invalid_params[] = {nullptr, "", "missing", "amount"};

    for (const char* param : invalid_params)
    {
        EXPECT_EQ(editor->GetChoiceCount("test", param), 0);
        EXPECT_STREQ(editor->GetChoiceLabel("test", param, 0), "");
    }
    const char* invalid_programs[] = {nullptr, "", "missing"};

    for (const char* program : invalid_programs)
    {
        EXPECT_EQ(editor->GetChoiceCount(program, "mode"), 0);
        EXPECT_STREQ(editor->GetChoiceLabel(program, "mode", 0), "");
    }

    render::MaterialEditParam param{};
    bool found_mode = false;

    for (int index = 0; index < render::MaterialEdit_Get()->GetParamCount("test"); index++)
    {
        ASSERT_TRUE(render::MaterialEdit_Get()->GetParam("test", index, &param));
        if (std::strcmp(param.name, "mode") == 0)
        {
            found_mode = true;
            EXPECT_EQ(param.widget, render::MaterialEditParamWidget::Choice);
            EXPECT_TRUE(param.instance);
            EXPECT_FLOAT_EQ(param.min_value, 0.0f);
            EXPECT_FLOAT_EQ(param.max_value, 1.0f);
        }
    }

    EXPECT_TRUE(found_mode);
}

TEST_F(MaterialSyncTest, ScalarChoicesClampIntegerIndicesAcrossTheTextureGroup)
{
    const ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version":1,"program":"test","parameters":{
            "uWear":{"type":"scalar","widget":"choice","choices":["Off","On","Auto"],
                "instance":true,"min":0,"max":2,"default":0},
            "uOther":{"type":"scalar"}
        }
    })");
    ASSERT_FALSE(manifest.has_error()) << manifest.error_str();
    g_Manifest = *manifest;

    const char* textures[] = {"first.bmp", "second.bmp"};
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginGroup("models/shared.mdl", textures, 2, 0));
    for (const std::pair<float, float>& value : {std::pair{-5.0f, 0.0f}, {1.0f, 1.0f}, {8.0f, 2.0f}})
    {
        ASSERT_TRUE(editor->SetScalar("uWear", value.first));
        EXPECT_FALSE(render::MaterialEdit_Get()->is_param_mixed("uWear"));
        for (const char* texture : textures)
        {
            const ncl_material::Material* material = render::MaterialEdit_Override("models/shared.mdl", texture);
            ASSERT_NE(material, nullptr);
            EXPECT_FLOAT_EQ(material->FindParam("uWear")->scalar, value.second);
        }
    }
}

TEST_F(MaterialSyncTest, TextureGroupChangesOnlySelectedEntriesAndTheEditedParameter)
{
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    const char* textures[] = {"first.bmp", "second.bmp"};
    ASSERT_TRUE(editor->BeginGroup("models/shared.mdl", textures, 2, 0));
    EXPECT_EQ(editor->get_selection_count(), 2);
    EXPECT_TRUE(editor->is_param_mixed("uWear"));
    ASSERT_TRUE(editor->SetScalar("uWear", 1.25f));
    EXPECT_FALSE(editor->is_param_mixed("uWear"));
    for (const char* texture : textures)
    {
        const ncl_material::Material* material = render::MaterialEdit_Override("models/shared.mdl", texture);
        ASSERT_NE(material, nullptr);
        EXPECT_FLOAT_EQ(material->FindParam("uWear")->scalar, 1.25f);
    }
    EXPECT_EQ(render::MaterialEdit_Override("models/shared.mdl", "unselected.bmp"), nullptr);
    editor->End();
    EXPECT_EQ(editor->get_selection_count(), 0);
}

TEST_F(MaterialSyncTest, InvalidGroupRetainsTheWorkingSessionAndDuplicatesAreCollapsed)
{
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(editor->Begin("models/shared.mdl", "first.bmp"));
    ASSERT_TRUE(editor->SetScalar("uWear", 1.5f));
    const char* invalid[] = {"first.bmp", "missing.bmp"};
    EXPECT_FALSE(editor->BeginGroup("models/shared.mdl", invalid, 2, 0));
    EXPECT_FALSE(editor->BeginGroup("models/shared.mdl", invalid, 0, 0));
    float value = 0;
    ASSERT_TRUE(editor->get_scalar("uWear", &value));
    EXPECT_FLOAT_EQ(value, 1.5f);
    const char* duplicate[] = {"first.bmp", "FIRST.BMP"};
    ASSERT_TRUE(editor->BeginGroup("models/shared.mdl", duplicate, 2, 0));
    EXPECT_EQ(editor->get_selection_count(), 1);
}

TEST_F(MaterialSyncTest, MixedProgramGroupRequiresAProgramChoiceBeforeEditingParameters)
{
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    const char* textures[] = {"first.bmp", "second.bmp"};
    ASSERT_TRUE(editor->BeginGroup("models/mixed.mdl", textures, 2, 0));
    EXPECT_TRUE(editor->has_mixed_programs());
    EXPECT_STREQ(editor->get_edited_program(), "");
    EXPECT_FALSE(editor->SetScalar("uWear", .75f));
    ASSERT_TRUE(editor->SetEditedProgram("test"));
    EXPECT_FALSE(editor->has_mixed_programs());
    ASSERT_TRUE(editor->SetScalar("uWear", .75f));
    EXPECT_FLOAT_EQ(render::MaterialEdit_Override("models/mixed.mdl", "second.bmp")->FindParam("uWear")->scalar, .75f);
}

TEST_F(MaterialSyncTest, GroupSavePreservesUnselectedEntriesAndIndependentParameterValues)
{
    const char* textures[] = {"first.bmp", "second.bmp"};
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginGroup("models/shared.mdl", textures, 2, 0));
    ASSERT_TRUE(editor->SetScalar("uWear", 1.25f));
    EXPECT_FLOAT_EQ(render::MaterialEdit_Override("models/shared.mdl", textures[0])->FindParam("uOther")->scalar, 2);
    EXPECT_FLOAT_EQ(render::MaterialEdit_Override("models/shared.mdl", textures[1])->FindParam("uOther")->scalar, 3);
    const ncl_utils::ResultT<ncl_material::MaterialPreset> preset = ncl_material::ParseMaterialPreset(editor->GetPresetStub(), {});
    ASSERT_FALSE(preset.has_error()) << preset.error_str();
    ASSERT_EQ(preset->materials.size(), 2u);
    g_Documents["models/shared.mat"] =
        R"({"version":1,"materials":{"unselected.bmp":{"program":"test","params":{"uWear":1.75,"uOther":4}}}})";
    decltype(gEngfuncs.pfnGetGameDirectory) previous = gEngfuncs.pfnGetGameDirectory;
    gEngfuncs.pfnGetGameDirectory = GameDirectory;
    render::Renderer_InitializeMaterialFiles(GameDirectory());
    const std::string target = ".superpowers/material-effects-refinement-20261025/group-save-fixture.mdl";
    g_Documents[target.substr(0, target.size() - 4) + ".mat"] = g_Documents["models/shared.mat"];
    char error[512]{};
    EXPECT_TRUE(editor->Save(target.c_str(), error, sizeof(error))) << error;
    gEngfuncs.pfnGetGameDirectory = previous;
    std::ifstream file(target.substr(0, target.size() - 4) + ".mat", std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const ncl_utils::ResultT<ncl_material::MaterialDoc> doc = ncl_material::ParseMaterialDoc(saved);
    ASSERT_FALSE(doc.has_error()) << doc.error_str();
    ASSERT_EQ(doc->entries.size(), 3u);
    EXPECT_FLOAT_EQ(doc->FindByTexture("unselected.bmp")->FindParam("uWear")->scalar, 1.75f);
    EXPECT_FLOAT_EQ(doc->FindByTexture("first.bmp")->FindParam("uWear")->scalar, 1.25f);
    EXPECT_FLOAT_EQ(doc->FindByTexture("second.bmp")->FindParam("uOther")->scalar, 3);
}

namespace
{
    std::string g_StateReportDirectory;

    class MaterialStateReportTest : public MaterialSyncTest
    {
    private:
        goldsrc::cl_enginefunc_t previous_engine_functions_{};
        std::filesystem::path directory_{};

    protected:
        void SetUp() override
        {
            previous_engine_functions_ = gEngfuncs;
            MaterialSyncTest::SetUp();
            const std::string prefix =
                "nextclient-material-state-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            std::error_code error;

            for (int suffix = 0;; suffix++)
            {
                const std::filesystem::path candidate = std::filesystem::temp_directory_path() / (prefix + "-" + std::to_string(suffix));

                if (std::filesystem::create_directory(candidate, error))
                {
                    directory_ = candidate;
                    break;
                }

                ASSERT_FALSE(error) << error.message();
            }

            g_StateReportDirectory = directory_.string();
            gEngfuncs.pfnGetGameDirectory = []() { return g_StateReportDirectory.c_str(); };
            render::Renderer_InitializeMaterialFiles(g_StateReportDirectory.c_str());
            render::MaterialEdit_RegisterCommands();
        }

        void TearDown() override
        {
            gEngfuncs = previous_engine_functions_;
            g_StateReportDirectory.clear();

            if (!directory_.empty())
            {
                std::error_code error;
                std::filesystem::remove_all(directory_, error);
                EXPECT_FALSE(error) << error.message();
            }

            MaterialSyncTest::TearDown();
        }
    };
} // namespace

TEST_F(MaterialStateReportTest, FailedOpenOfExistingDocumentRejectsSaveWithoutReplacingOrReloading)
{
    const std::string original = R"({"version":1,"materials":{
        "first.bmp":{"program":"test","params":{"uWear":0.25}},
        "unselected.bmp":{"program":"test","params":{"uWear":1.75}}
    }})";
    g_Documents["models/shared.mat"] = original;
    const std::filesystem::path destination = std::filesystem::path(g_StateReportDirectory) / "models/shared.mat";
    ASSERT_TRUE(std::filesystem::create_directory(destination.parent_path()));
    {
        std::ofstream file(destination, std::ios::binary);
        file << original;
        file.close();
        ASSERT_TRUE(file);
    }

    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(editor->Begin("models/shared.mdl", "first.bmp"));
    ASSERT_TRUE(editor->SetScalar("uWear", 0.75f));
    g_FailedOpenPath = "models/shared.mat";
    char error[512]{};

    EXPECT_FALSE(editor->Save(nullptr, error, sizeof(error)));
    EXPECT_NE(std::string(error).find("models/shared.mat"), std::string::npos);
    ncl_material::MaterialOrigin origin = ncl_material::MaterialOrigin::Implicit;
    const ncl_material::Material* cached = g_MaterialOwners->state.models.ResolveMaterialByName("models/shared.mdl", "first.bmp", &origin);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(origin, ncl_material::MaterialOrigin::ModelDocument);
    const ncl_material::ParamValue* wear = cached->FindParam("uWear");
    EXPECT_NE(wear, nullptr);
    if (wear != nullptr)
    {
        EXPECT_FLOAT_EQ(wear->scalar, 0.25f);
    }
    EXPECT_EQ(g_FailedOpenAttempts, 1);
    std::ifstream file(destination, std::ios::binary);
    const std::string saved((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_EQ(saved, original);
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(destination.parent_path()), std::filesystem::directory_iterator()), 1);
}

TEST_F(MaterialStateReportTest, MaterialStateCommandReportsAnInactiveOrEndedSession)
{
    const std::filesystem::path report_path = std::filesystem::path(g_StateReportDirectory) / "materials_state.txt";

    for (bool ended : {false, true})
    {
        SCOPED_TRACE(ended);

        if (ended)
        {
            ASSERT_TRUE(render::MaterialEdit_Get()->Begin("models/shared.mdl", "first.bmp"));
        }

        render::MaterialEdit_Get()->End();

        {
            std::ofstream stale(report_path, std::ios::binary);
            stale << "session editing no\nrenderer prepared models\n";
            ASSERT_TRUE(stale.good());
        }

        ASSERT_TRUE(std::filesystem::remove(report_path));
        ASSERT_FALSE(std::filesystem::exists(report_path));
        g_Commands.at("r_mat_state")();
        ASSERT_TRUE(std::filesystem::exists(report_path));
        std::ifstream file(report_path, std::ios::binary);
        ASSERT_TRUE(file.is_open());
        const std::string report((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        EXPECT_NE(report.find("session editing no"), std::string::npos);
        EXPECT_NE(report.find("renderer prepared models"), std::string::npos);
    }
}

TEST_F(MaterialSyncTest, NetworkOcclusionRequiresSynchronizedInstanceValuesAndRevokesRemovedValues)
{
    InstallStudioContract(0.5f, 2);
    g_Strings.Register(5, "uOccludedAmount");
    g_Strings.Register(6, "first.bmp/uOutlineWidth");
    goldsrc::client_static_t client{};
    client.state = goldsrc::ca_active;
    cls = &client;
    auto expect = [&](const char* name, float value) {
        const ncl_material::Material* material = render::ServerMaterials_BindingFor({7, "first.bmp"});
        ASSERT_NE(material, nullptr);
        ASSERT_NE(material->FindParam(name), nullptr);
        EXPECT_FLOAT_EQ(material->FindParam(name)->scalar, value) << name;
    };
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    expect("uOccludedAmount", 0);
    expect("uOutlineWidth", 0);
    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added = {{5, 0.5f}, {6, 2.0f}};
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    expect("uOccludedAmount", .5f);
    expect("uOutlineWidth", 2);
    EXPECT_FALSE(render::ServerMaterials_SetParam(7, "uOccludedAmount", 0.015625f));
    expect("uOccludedAmount", .5f);
    params.removed = {5, 6};
    params.added.clear();
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    expect("uOccludedAmount", 0);
    expect("uOutlineWidth", 0);
    sync_->OnDestroy(10);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "first.bmp"}), nullptr);
}

TEST_F(MaterialSyncTest, NetworkSessionIgnoresPreconnectionConsoleBindingsAndRejectsMaterialCommands)
{
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    render::ServerMaterials_SetBinding(7, "test.mat", "second");
    ASSERT_NE(render::ServerMaterials_BindingFor({7, "second.bmp"}), nullptr);
    goldsrc::client_static_t client{};
    client.state = goldsrc::ca_connecting;
    cls = &client;
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "second.bmp"}), nullptr);
    render::ServerMaterials_RegisterCommands();
    g_Arguments = {"r_mat_bind", "8", "test.mat", "first"};
    g_Commands.at("r_mat_bind")();
    EXPECT_EQ(render::ServerMaterials_BindingFor({8, "first.bmp"}), nullptr);
    g_Arguments = {"r_mat_param", "7", "uWear", "6"};
    g_Commands.at("r_mat_param")();
    ExpectWear(7, "first.bmp", .5f);
    g_Arguments = {"r_mat_presets", "clear"};
    g_Commands.at("r_mat_presets")();
    ExpectWear(7, "first.bmp", .5f);
    client.state = goldsrc::ca_disconnected;
    EXPECT_NE(render::ServerMaterials_BindingFor({7, "second.bmp"}), nullptr);
}

TEST_F(MaterialSyncTest, ClearingCompleteParamsRevokesOmittedPositiveEffectDefaults)
{
    InstallStudioContract(0.5f, 2);
    g_Strings.Register(5, "uOccludedAmount");
    g_Strings.Register(6, "uOutlineWidth");
    sync_->OnCreate(10, Fields(7, 2, 0.5f));
    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added = {{5, 0.5f}, {6, 2.0f}};
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    const ncl_material::Material* material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("uOccludedAmount"), nullptr);
    ASSERT_NE(material->FindParam("uOutlineWidth"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("uOccludedAmount")->scalar, 0.5f);
    EXPECT_FLOAT_EQ(material->FindParam("uOutlineWidth")->scalar, 2.0f);

    params.added.clear();
    params.clear = true;
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("uOccludedAmount"), nullptr);
    ASSERT_NE(material->FindParam("uOutlineWidth"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("uOccludedAmount")->scalar, 0.0f);
    EXPECT_FLOAT_EQ(material->FindParam("uOutlineWidth")->scalar, 0.0f);

    const render::ServerMaterialParam activations[] = {{"uOccludedAmount", 0.5f}, {"uOutlineWidth", 2.0f}};
    render::ServerMaterials_ReplaceOwnerParams({7, 10}, activations);
    render::ServerMaterials_ReplaceOwnerParams({7, 10}, {});
    material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("uOccludedAmount"), nullptr);
    ASSERT_NE(material->FindParam("uOutlineWidth"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("uOccludedAmount")->scalar, 0.0f);
    EXPECT_FLOAT_EQ(material->FindParam("uOutlineWidth")->scalar, 0.0f);
}

TEST_F(MaterialSyncTest, MaterialEditorRejectsNonfiniteScalarsWithoutChangingTheGroup)
{
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    const char* textures[] = {"first.bmp", "second.bmp"};
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginGroup("models/shared.mdl", textures, 2));
    const std::string original = editor->GetPresetStub();

    for (float value : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()})
    {
        EXPECT_FALSE(editor->SetScalar("uWear", value));
        EXPECT_STREQ(editor->GetPresetStub(), original.c_str());
        EXPECT_TRUE(render::MaterialEdit_Get()->is_param_mixed("uWear"));
    }
}

TEST_F(MaterialSyncTest, MaterialEditorClampsFiniteScalarExtremes)
{
    g_Manifest.parameters.front().has_range = true;
    g_Manifest.parameters.front().min_value = 0.0f;
    g_Manifest.parameters.front().max_value = 1.0f;
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(editor->Begin("models/shared.mdl", "first.bmp"));

    ASSERT_TRUE(editor->SetScalar("uWear", std::numeric_limits<float>::max()));
    float value = 0.0f;
    ASSERT_TRUE(editor->get_scalar("uWear", &value));
    EXPECT_EQ(value, 1.0f);
    ASSERT_TRUE(editor->SetScalar("uWear", -std::numeric_limits<float>::max()));
    ASSERT_TRUE(editor->get_scalar("uWear", &value));
    EXPECT_EQ(value, 0.0f);
}

TEST_F(MaterialSyncTest, MaterialEditorRejectsEveryNonfiniteVectorComponentBeforeMutation)
{
    ncl_material::ParamSchema tint;
    tint.name = "uTint";
    tint.type = ncl_material::ParamType::Vec4;
    tint.has_range = true;
    tint.min_value = 0.0f;
    tint.max_value = 1.0f;
    g_Manifest.parameters.push_back(tint);
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    const char* textures[] = {"first.bmp", "second.bmp"};
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginGroup("models/shared.mdl", textures, 2));
    const float initial[] = {0.1f, 0.2f, 0.3f, 0.4f};
    ASSERT_TRUE(editor->SetVec4("uTint", initial));
    const std::string original = editor->GetPresetStub();

    for (int component = 0; component < 4; component++)
    {
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()})
        {
            float value[] = {0.5f, 0.6f, 0.7f, 0.8f};
            value[component] = invalid;
            EXPECT_FALSE(editor->SetVec4("uTint", value));
            EXPECT_STREQ(editor->GetPresetStub(), original.c_str());
        }
    }
}

class MaterialParamPreviewTest : public MaterialSyncTest
{
protected:
    void SetUp() override
    {
        MaterialSyncTest::SetUp();
        ncl_material::ParamSchema image;
        image.name = "uImage";
        image.type = ncl_material::ParamType::TextureRef;
        g_Manifest.parameters.push_back(image);
        ncl_material::ParamSchema tint;
        tint.name = "uTint";
        tint.type = ncl_material::ParamType::Vec4;
        g_Manifest.parameters.push_back(tint);

        ncl_material::Material first;
        first.program = "test";
        ncl_material::ParamValue image_value;
        image_value.type = ncl_material::ParamType::TextureRef;
        image_value.texture_ref = "materials/first.tga";
        first.params.push_back({"uImage", image_value});
        ncl_material::ParamValue wear;
        wear.scalar = 0.25f;
        first.params.push_back({"uWear", wear});
        ncl_material::ParamValue color;
        color.type = ncl_material::ParamType::Vec4;
        color.vec4 = {1.0f, 0.0f, 0.0f, 0.5f};
        first.params.push_back({"uTint", color});
        g_EditorMaterials["first.bmp"] = first;
        first.params[0].value.texture_ref = "materials/second.tga";
        first.params[2].value.vec4 = {0.0f, 1.0f, 0.0f, 1.0f};
        g_EditorMaterials["second.bmp"] = first;
    }

    void BeginGroup()
    {
        const char* textures[] = {"first.bmp", "second.bmp"};
        ASSERT_TRUE(render::MaterialEdit_Get()->BeginGroup("models/shared.mdl", textures, 2));
    }
};

TEST_F(MaterialParamPreviewTest, CancelRestoresEachImageAndItsOriginalParameterOrder)
{
    BeginGroup();
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    const std::string original = editor->GetPresetStub();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/preview.tga"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/another.tga"));
    EXPECT_FALSE(render::MaterialEdit_Get()->is_param_mixed("uImage"));

    render::MaterialEdit_Get()->EndParamPreview(false);
    EXPECT_STREQ(editor->GetPresetStub(), original.c_str());
    EXPECT_TRUE(render::MaterialEdit_Get()->is_param_mixed("uImage"));
}

TEST_F(MaterialParamPreviewTest, CancelRestoresEachColorWithoutDiscardingOtherEdits)
{
    BeginGroup();
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uTint"));
    const float preview[] = {0.3f, 0.4f, 0.5f, 0.6f};
    ASSERT_TRUE(editor->SetVec4("uTint", preview));
    ASSERT_TRUE(editor->SetScalar("uWear", 0.75f));

    render::MaterialEdit_Get()->EndParamPreview(false);
    for (const char* texture : {"first.bmp", "second.bmp"})
    {
        const ncl_material::Material* material = render::MaterialEdit_Override("models/shared.mdl", texture);
        ASSERT_NE(material, nullptr);
        EXPECT_EQ(material->FindParam("uTint")->vec4, g_EditorMaterials[texture].FindParam("uTint")->vec4);
        EXPECT_FLOAT_EQ(material->FindParam("uWear")->scalar, 0.75f);
    }
    EXPECT_TRUE(render::MaterialEdit_Get()->is_param_mixed("uTint"));
}

TEST_F(MaterialParamPreviewTest, CancelRestoresAnAbsentImageAfterClearAndPreview)
{
    g_EditorMaterials["second.bmp"].params.erase(g_EditorMaterials["second.bmp"].params.begin());
    BeginGroup();
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    const std::string original = editor->GetPresetStub();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    ASSERT_TRUE(editor->ClearParam("uImage"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/preview.tga"));

    render::MaterialEdit_Get()->EndParamPreview(false);
    EXPECT_STREQ(editor->GetPresetStub(), original.c_str());
    EXPECT_EQ(render::MaterialEdit_Override("models/shared.mdl", "second.bmp")->FindParam("uImage"), nullptr);
}

TEST_F(MaterialParamPreviewTest, AcceptKeepsThePreviewAndAllowsANewSnapshot)
{
    BeginGroup();
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    EXPECT_FALSE(render::MaterialEdit_Get()->BeginParamPreview("uTint"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/accepted.tga"));
    render::MaterialEdit_Get()->EndParamPreview(true);
    const std::string accepted = editor->GetPresetStub();
    EXPECT_FALSE(render::MaterialEdit_Get()->is_param_mixed("uImage"));

    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/preview.tga"));
    render::MaterialEdit_Get()->EndParamPreview(false);
    EXPECT_STREQ(editor->GetPresetStub(), accepted.c_str());
}

TEST_F(MaterialParamPreviewTest, ANewEditingSessionDropsThePreviousPreviewSnapshot)
{
    BeginGroup();
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/preview.tga"));
    editor->End();
    render::MaterialEdit_Get()->EndParamPreview(false);
    ASSERT_TRUE(editor->Begin("models/shared.mdl", "second.bmp"));
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/other.tga"));
    render::MaterialEdit_Get()->EndParamPreview(false);

    EXPECT_STREQ(editor->get_texture("uImage"), "materials/second.tga");
    EXPECT_EQ(render::MaterialEdit_Get()->get_selection_count(), 1);
}

TEST_F(MaterialParamPreviewTest, FailedGroupReplacementPreservesPendingPreview)
{
    g_EditorMaterials["second.bmp"].params.erase(g_EditorMaterials["second.bmp"].params.begin());
    BeginGroup();
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(editor->SetScalar("uWear", 0.75f));
    const std::string original = editor->GetPresetStub();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/preview.tga"));
    const std::string preview = editor->GetPresetStub();

    const char* invalid[] = {"first.bmp", "missing.bmp"};
    EXPECT_FALSE(editor->BeginGroup("models/replacement.mdl", invalid, 2));

    EXPECT_STREQ(editor->get_edited_model(), "models/shared.mdl");
    ASSERT_EQ(editor->get_selection_count(), 2);
    EXPECT_STREQ(editor->get_selected_texture(0), "first.bmp");
    EXPECT_STREQ(editor->get_selected_texture(1), "second.bmp");
    EXPECT_STREQ(editor->GetPresetStub(), preview.c_str());
    EXPECT_FALSE(render::MaterialEdit_Get()->BeginParamPreview("uTint"));

    render::MaterialEdit_Get()->EndParamPreview(false);

    EXPECT_STREQ(editor->GetPresetStub(), original.c_str());

    const ncl_material::Material* first = render::MaterialEdit_Override("models/shared.mdl", "first.bmp");
    const ncl_material::Material* second = render::MaterialEdit_Override("models/shared.mdl", "second.bmp");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(first->params.size(), 3u);
    EXPECT_EQ(
        ParameterNames(*first),
        ParameterNames(*g_MaterialOwners->state.models.ResolveMaterialByName("models/shared.mdl", "first.bmp", nullptr))
    );
    EXPECT_EQ(second->FindParam("uImage"), nullptr);
    ASSERT_NE(second->FindParam("uWear"), nullptr);
    EXPECT_FLOAT_EQ(second->FindParam("uWear")->scalar, 0.75f);
}

TEST_F(MaterialParamPreviewTest, SuccessfulGroupReplacementDiscardsPendingPreview)
{
    BeginGroup();
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/preview.tga"));

    const char* replacement[] = {"second.bmp"};
    ASSERT_TRUE(editor->BeginGroup("models/replacement.mdl", replacement, 1));
    const std::string replaced = editor->GetPresetStub();

    render::MaterialEdit_Get()->EndParamPreview(false);

    EXPECT_TRUE(editor->is_editing());
    EXPECT_STREQ(editor->get_edited_model(), "models/replacement.mdl");
    ASSERT_EQ(editor->get_selection_count(), 1);
    EXPECT_STREQ(editor->get_selected_texture(0), "second.bmp");
    EXPECT_STREQ(editor->get_texture("uImage"), "materials/second.tga");
    EXPECT_STREQ(editor->GetPresetStub(), replaced.c_str());
    EXPECT_EQ(render::MaterialEdit_Override("models/shared.mdl", "second.bmp"), nullptr);
    EXPECT_EQ(render::MaterialEdit_Override("models/replacement.mdl", "first.bmp"), nullptr);

    const ncl_material::Material* material = render::MaterialEdit_Override("models/replacement.mdl", "second.bmp");
    ASSERT_NE(material, nullptr);
    ASSERT_EQ(material->params.size(), 3u);
    EXPECT_EQ(
        ParameterNames(*material),
        ParameterNames(*g_MaterialOwners->state.models.ResolveMaterialByName("models/replacement.mdl", "second.bmp", nullptr))
    );
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    render::MaterialEdit_Get()->EndParamPreview(true);
}

TEST_F(MaterialParamPreviewTest, RepeatedEndMakesLatePreviewCompletionInert)
{
    BeginGroup();
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
    ASSERT_TRUE(editor->SetTexture("uImage", "materials/preview.tga"));

    editor->End();
    editor->End();
    render::MaterialEdit_Get()->EndParamPreview(false);
    render::MaterialEdit_Get()->EndParamPreview(true);

    EXPECT_FALSE(editor->is_editing());
    EXPECT_EQ(render::MaterialEdit_Get()->get_selection_count(), 0);
    EXPECT_STREQ(editor->get_edited_model(), "");
    EXPECT_STREQ(editor->get_texture("uImage"), "");
    EXPECT_EQ(render::MaterialEdit_Override("models/shared.mdl", "first.bmp"), nullptr);
    EXPECT_EQ(render::MaterialEdit_Override("models/shared.mdl", "second.bmp"), nullptr);
    EXPECT_FALSE(render::MaterialEdit_Get()->BeginParamPreview("uImage"));
}

namespace
{
    void PrepareReplacementPreset(const std::vector<std::string>& textures)
    {
        std::string document = R"({"version":1,"materials":[)";
        g_Strings.Register(5, "replacements.mat");

        for (size_t i = 0; i < textures.size(); i++)
        {
            const std::string name = "replacement" + std::to_string(i);
            g_Strings.Register(static_cast<uint16_t>(100 + i), name);

            if (i != 0)
            {
                document += ',';
            }

            document += "{\"name\":\"" + name + "\",\"texture\":\"" + textures[i] +
                        "\",\"material\":{\"program\":\"test\",\"params\":{\"uWear\":" + std::to_string(i) + "}}}";
        }

        document += "]}";
        g_Documents["replacements.mat"] = std::move(document);
    }

    ncl_entity::ArrayStringIdPairsDelta ReplacementBindings(size_t count)
    {
        ncl_entity::ArrayStringIdPairsDelta delta;

        for (size_t i = 0; i < count; i++)
        {
            delta.added.emplace_back(5, static_cast<uint16_t>(100 + i));
        }

        return delta;
    }
}

TEST_F(MaterialSyncTest, AdmissionUsesDescendingOwnerIdInEitherCreationOrder)
{
    std::vector<std::string> textures;

    for (size_t i = 0; i < 17; i++)
    {
        textures.push_back("slot" + std::to_string(i) + ".bmp");
    }

    PrepareReplacementPreset(textures);

    ncl_entity::ArrayStringIdPairsDelta higher_bindings;
    higher_bindings.added.emplace_back(5, 100);
    higher_bindings.added.emplace_back(5, 116);
    ncl_entity::ArrayStringIdFloatPairsDelta higher_params;
    higher_params.added.emplace_back(4, 1.5f);
    const NclEntityFields higher_fields = {
        {kEntityIndex, uint32_t{7}},
        {kMaterials, higher_bindings},
        {kParams, higher_params},
    };
    const NclEntityFields lower_fields = {
        {kEntityIndex, uint32_t{7}},
        {kMaterials, ReplacementBindings(16)},
    };
    std::vector<std::string> first_admitted;

    for (bool higher_first : {false, true})
    {
        SCOPED_TRACE(higher_first);
        sync_->OnClear();
        render::ServerMaterials_Clear();

        if (higher_first)
        {
            sync_->OnCreate(20, higher_fields);
            sync_->OnCreate(10, lower_fields);
        }
        else
        {
            sync_->OnCreate(10, lower_fields);
            sync_->OnCreate(20, higher_fields);
        }

        ExpectWear(7, "slot0.bmp", 1.5f);
        ExpectWear(7, "slot16.bmp", 1.5f);

        for (size_t i = 1; i <= 14; i++)
        {
            ExpectWear(7, textures[i].c_str(), static_cast<float>(i));
        }

        EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot15.bmp"}), nullptr);

        std::vector<std::string> admitted;

        for (const std::string& texture : textures)
        {
            if (render::ServerMaterials_BindingFor({7, texture}) != nullptr)
            {
                admitted.push_back(texture);
            }
        }

        EXPECT_EQ(admitted.size(), 16u);

        if (higher_first)
        {
            EXPECT_EQ(admitted, first_admitted);
        }
        else
        {
            first_admitted = std::move(admitted);
        }
    }
}

TEST_F(MaterialSyncTest, RepeatedReplacementsKeepLatestMaterialAfterSixteenAssignments)
{
    PrepareReplacementPreset(std::vector<std::string>(20, "first.bmp"));
    sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(1)}});

    for (uint16_t material_id = 101; material_id < 120; material_id++)
    {
        ncl_entity::ArrayStringIdPairsDelta delta;
        delta.added.emplace_back(5, material_id);
        sync_->OnUpdate(10, kMaterials, {{kMaterials, delta}});
    }

    ExpectWear(7, "first.bmp", 19.0f);
}

TEST_F(MaterialSyncTest, ConsoleSlotsReserveCapacityAndRemovalPromotesRetainedSynchronizedCandidate)
{
    std::vector<std::string> textures;

    for (size_t i = 0; i < 17; i++)
    {
        textures.push_back("slot" + std::to_string(i) + ".bmp");
    }

    textures.push_back("second.bmp");
    PrepareReplacementPreset(textures);
    render::ServerMaterials_SetBinding(7, "test.mat", "second");
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "second.bmp/uWear", 2.25f));
    sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(16)}});
    ncl_entity::ArrayStringIdPairsDelta higher;
    higher.added.emplace_back(5, 116);
    sync_->OnCreate(20, {{kEntityIndex, uint32_t{7}}, {kMaterials, higher}});

    ExpectWear(7, "second.bmp", 2.25f);
    ExpectWear(7, "slot16.bmp", 16.0f);
    ExpectWear(7, "slot13.bmp", 13.0f);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot14.bmp"}), nullptr);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot15.bmp"}), nullptr);

    render::ServerMaterials_SetBinding(7, "test.mat", "first");
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "first.bmp"}), nullptr);
    render::ServerMaterials_SetBinding(7, "replacements.mat", "replacement17");
    ExpectWear(7, "second.bmp", 17.0f);

    render::ServerMaterials_RemoveBinding(7, "replacements.mat", "replacement17");
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "second.bmp"}), nullptr);
    ExpectWear(7, "slot14.bmp", 14.0f);
    ExpectWear(7, "slot16.bmp", 16.0f);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot15.bmp"}), nullptr);
}

TEST_F(MaterialSyncTest, OwnerRemovalOrMigrationPromotesRetainedCandidateWithoutRebuildingOtherOwners)
{
    for (bool migrate : {false, true})
    {
        SCOPED_TRACE(migrate);
        sync_->OnClear();
        render::ServerMaterials_Clear();
        std::vector<std::string> textures;

        for (size_t i = 0; i < 17; i++)
        {
            textures.push_back("slot" + std::to_string(i) + ".bmp");
        }

        PrepareReplacementPreset(textures);
        sync_->OnCreate(20, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(16)}});
        ncl_entity::ArrayStringIdPairsDelta lower;
        lower.added.emplace_back(5, 116);
        sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, lower}});
        sync_->OnCreate(30, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(1)}});
        ASSERT_TRUE(render::ServerMaterials_SetParam(render::ServerMaterialBindingOwner{7, 30}, "uWear", 2.25f));
        const ncl_material::Material* untouched = render::ServerMaterials_BindingFor({7, "slot0.bmp"});
        ASSERT_EQ(render::ServerMaterials_BindingFor({7, "slot16.bmp"}), nullptr);

        ncl_entity::ArrayStringIdFloatPairsDelta params;
        params.added.emplace_back(4, -0.75f);
        sync_->OnUpdate(10, kParams, {{kParams, params}});
        g_Strings.Register(116, "missing");

        if (migrate)
        {
            sync_->OnUpdate(20, kEntityIndex, {{kEntityIndex, uint32_t{8}}});
            ExpectWear(8, "slot15.bmp", 15.0f);
        }
        else
        {
            sync_->OnDestroy(20);
        }

        EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot0.bmp"}), untouched);
        ExpectWear(7, "slot0.bmp", 2.25f);
        ExpectWear(7, "slot16.bmp", -0.75f);
        EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot15.bmp"}), nullptr);
    }
}

TEST_F(MaterialSyncTest, PredicateIgnoresExcludedCandidatesAndKeepsAdmittedShadowedOwners)
{
    std::vector<std::string> textures;

    for (size_t i = 0; i < 17; i++)
    {
        textures.push_back("slot" + std::to_string(i) + ".bmp");
    }

    PrepareReplacementPreset(textures);
    sync_->OnCreate(20, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(16)}});
    ncl_entity::ArrayStringIdPairsDelta excluded;
    excluded.added.emplace_back(5, 116);
    sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, excluded}});
    auto matches = [](const ncl_material::Material& material) {
        const ncl_material::ParamValue* wear = material.FindParam("uWear");
        return wear != nullptr && wear->scalar == 16.0f;
    };
    EXPECT_FALSE(render::ServerMaterials_AnyBindingMatches(7, matches));

    sync_->OnDestroy(20);
    EXPECT_TRUE(render::ServerMaterials_AnyBindingMatches(7, matches));
    sync_->OnCreate(30, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(1)}});
    const render::ServerMaterialBinding bindings[] = {{"replacements.mat", "replacement0"}};
    const render::ServerMaterialParam params[] = {{"uWear", 16.0f}};
    render::ServerMaterials_ReplaceOwner({7, 25}, bindings, params);
    sync_->OnDestroy(10);

    ExpectWear(7, "slot0.bmp", 0.0f);
    EXPECT_TRUE(render::ServerMaterials_AnyBindingMatches(7, matches));
}

TEST_F(MaterialSyncTest, InvalidLaterRequestKeepsLastValidSameTextureAssignment)
{
    PrepareReplacementPreset({"first.bmp", "first.bmp"});
    ncl_entity::ArrayStringIdPairsDelta materials = ReplacementBindings(2);
    g_Strings.Register(102, "missing");
    materials.added.emplace_back(5, 102);
    sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, materials}});

    ExpectWear(7, "first.bmp", 1.0f);
}

TEST_F(MaterialSyncTest, FullSnapshotKeepsLatestMaterialAfterSixteenReplacements)
{
    PrepareReplacementPreset(std::vector<std::string>(20, "first.bmp"));
    sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(20)}});

    ExpectWear(7, "first.bmp", 19.0f);
}

TEST_F(MaterialSyncTest, ResolvedTextureLimitStillAllowsReplacingAnExistingTexture)
{
    std::vector<std::string> textures;

    for (size_t i = 0; i < 17; i++)
    {
        textures.push_back("slot" + std::to_string(i) + ".bmp");
    }

    textures.push_back("slot0.bmp");
    PrepareReplacementPreset(textures);
    sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(17)}});

    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot16.bmp"}), nullptr);

    ncl_entity::ArrayStringIdPairsDelta delta;
    delta.added.emplace_back(5, 117);
    sync_->OnUpdate(10, kMaterials, {{kMaterials, delta}});

    ExpectWear(7, "slot0.bmp", 17.0f);
    ExpectWear(7, "slot15.bmp", 15.0f);
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot16.bmp"}), nullptr);
}

TEST_F(MaterialSyncTest, RemovingABindingFreesASlotForAnAlreadySynchronizedTexture)
{
    std::vector<std::string> textures;

    for (size_t i = 0; i < 17; i++)
    {
        textures.push_back("slot" + std::to_string(i) + ".bmp");
    }

    PrepareReplacementPreset(textures);
    sync_->OnCreate(10, {{kEntityIndex, uint32_t{7}}, {kMaterials, ReplacementBindings(17)}});
    ASSERT_EQ(render::ServerMaterials_BindingFor({7, "slot16.bmp"}), nullptr);

    ncl_entity::ArrayStringIdPairsDelta delta;
    delta.removed.emplace_back(5, 100);
    sync_->OnUpdate(10, kMaterials, {{kMaterials, delta}});

    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "slot0.bmp"}), nullptr);
    ExpectWear(7, "slot16.bmp", 16.0f);
}

TEST_F(MaterialSyncTest, RestrictedPolicyMaterializesZeroForOmittedPositiveDefaults)
{
    InstallStudioContract(0.5f, 2);
    ASSERT_FALSE(render::StudioContract_Validate(g_Manifest).has_error());
    ncl_material::Material material;
    material.program = "test";
    const render::StudioProgramContract* contract = render::StudioContract_For(&material);
    ASSERT_NE(contract, nullptr);
    EXPECT_TRUE(contract->supports_outline);
    EXPECT_TRUE(contract->supports_occlusion);
    EXPECT_FLOAT_EQ(render::Manifest_GetParam(&material, "uOccludedAmount")->scalar, 0.5f);
    EXPECT_FLOAT_EQ(render::Manifest_GetParam(&material, "uOutlineWidth")->scalar, 2);
    EXPECT_FLOAT_EQ(render::StudioContract_PickingAlpha(&material, 0.75f), 0.75f);

    goldsrc::client_static_t client{};
    client.state = goldsrc::ca_active;
    cls = &client;
    render::ServerMaterials_ApplyEffectPolicy(material, false);

    ASSERT_NE(material.FindParam("uOccludedAmount"), nullptr);
    ASSERT_NE(material.FindParam("uOutlineWidth"), nullptr);
    EXPECT_FLOAT_EQ(material.FindParam("uOccludedAmount")->scalar, 0);
    EXPECT_FLOAT_EQ(material.FindParam("uOutlineWidth")->scalar, 0);
    EXPECT_EQ(material.params.size(), 2u);
    render::ServerMaterials_ApplyEffectPolicy(material, false);
    EXPECT_EQ(material.params.size(), 2u);

    material.params.clear();
    render::ServerMaterials_ApplyEffectPolicy(material, true);
    EXPECT_TRUE(material.params.empty());
    client.state = goldsrc::ca_disconnected;
    render::ServerMaterials_ApplyEffectPolicy(material, false);
    EXPECT_TRUE(material.params.empty());
}

TEST_F(MaterialSyncTest, CheckedManifestBoundaryRejectsUnknownAndMismatchedSemantics)
{
    InstallStudioContract();
    ncl_material::Material material;
    material.program = "test";
    ASSERT_NE(render::StudioContract_For(&material), nullptr);
    g_Manifest.render_contract = "unknown";
    ResetMaterialOwners();
    EXPECT_EQ(render::Manifest_Get("test"), nullptr);
    EXPECT_EQ(render::StudioContract_For(&material), nullptr);

    InstallStudioContract();
    g_Manifest.parameters.clear();
    g_Manifest.groups.clear();
    ResetMaterialOwners();
    EXPECT_EQ(render::Manifest_Get("test"), nullptr);
    EXPECT_EQ(render::StudioContract_For(&material), nullptr);
}

TEST_F(MaterialSyncTest, OrdinaryParametersDoNotEnrollAMaterialOrAuthorizeEffects)
{
    ncl_material::Material material;
    material.program = "test";
    material.params.push_back({"uOccludedAmount", {}});
    material.params.back().value.scalar = 1;
    goldsrc::client_static_t client{};
    client.state = goldsrc::ca_active;
    cls = &client;
    EXPECT_EQ(render::StudioContract_For(&material), nullptr);
    render::ServerMaterials_ApplyEffectPolicy(material, false);
    EXPECT_EQ(material.params.size(), 1u);
    EXPECT_FLOAT_EQ(material.FindParam("uOccludedAmount")->scalar, 1);
    EXPECT_FLOAT_EQ(render::StudioContract_PickingAlpha(&material, 0.75f), 0.75f);
}

TEST_F(MaterialSyncTest, LegacyProgramNeverReceivesAuthoredContract)
{
    InstallStudioContract(0.5f, 2);
    g_Manifest.program = ncl_material::kLegacyProgram;
    ASSERT_NE(render::Manifest_Get(g_Manifest.program), nullptr);
    ncl_material::Material material;
    material.program = ncl_material::kLegacyProgram;
    EXPECT_EQ(render::StudioContract_For(&material), nullptr);
    EXPECT_FLOAT_EQ(render::StudioContract_PickingAlpha(&material, 0.5f), 0.5f);
    render::StudioContract_DisableRestrictedEffects(material);
    EXPECT_TRUE(material.params.empty());
}

TEST_F(MaterialSyncTest, DirectFloatEditorPreservesNonGridValuesAcrossTheGroup)
{
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    const char* textures[] = {"first.bmp", "second.bmp"};
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginGroup("models/shared.mdl", textures, 2));
    for (float value : {0.1234567f, -0.1234567f, 0.0000001f})
    {
        ASSERT_TRUE(editor->SetScalar("uWear", value));
        for (const char* texture : textures)
        {
            const ncl_material::Material* material = render::MaterialEdit_Override("models/shared.mdl", texture);
            ASSERT_NE(material, nullptr);
            EXPECT_EQ(material->FindParam("uWear")->scalar, value);
        }
    }
}

TEST_F(MaterialSyncTest, DirectFloatEditorRejectsFractionalChoicesBeforeClamping)
{
    ncl_material::ParamSchema& schema = g_Manifest.parameters.front();
    schema.widget = ncl_material::ParamWidget::Choice;
    schema.choices = {"Off", "On", "Auto"};
    schema.has_default = true;
    schema.has_range = true;
    schema.min_value = 0.0f;
    schema.max_value = 2.0f;
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    const char* textures[] = {"first.bmp", "second.bmp"};
    ASSERT_TRUE(render::MaterialEdit_Get()->BeginGroup("models/shared.mdl", textures, 2));
    ASSERT_TRUE(editor->SetScalar("uWear", 1.0f));
    const std::string original = editor->GetPresetStub();
    for (float value : {-0.5f, 0.1234567f, 2.5f})
    {
        EXPECT_FALSE(editor->SetScalar("uWear", value));
        EXPECT_STREQ(editor->GetPresetStub(), original.c_str());
    }
}

TEST_F(MaterialSyncTest, DirectFloatNetworkValuesSurviveScopedUpdatesMigrationAndReentry)
{
    const float initial = 0.1234567f;
    const float updated = -0.0000001f;
    g_Strings.Register(5, "first.bmp/uWear");
    sync_->OnCreate(10, Fields(7, 2, initial));
    auto expect_bits = [](int index, float expected) {
        const ncl_material::Material* material = render::ServerMaterials_BindingFor({index, "first.bmp"});
        ASSERT_NE(material, nullptr);
        ASSERT_NE(material->FindParam("uWear"), nullptr);
        EXPECT_EQ(std::bit_cast<uint32_t>(material->FindParam("uWear")->scalar), std::bit_cast<uint32_t>(expected));
    };
    expect_bits(7, initial);

    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.removed = {4};
    params.added = {{5, updated}};
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    expect_bits(7, updated);
    sync_->OnUpdate(10, kEntityIndex, {{kEntityIndex, uint32_t{8}}});
    EXPECT_EQ(render::ServerMaterials_BindingFor({7, "first.bmp"}), nullptr);
    expect_bits(8, updated);

    sync_->OnDestroy(10);
    NclEntityFields reentry = Fields(8, 2, initial);
    reentry[kParams] = ncl_entity::ArrayStringIdFloatPairsDelta{{{5, updated}}};
    sync_->OnCreate(10, reentry);
    expect_bits(8, updated);
}

TEST_F(MaterialSyncTest, DirectFloatSnapshotsCopyValuesAndRestoreNonGridDefaults)
{
    g_Documents["float.mat"] = R"({"version":1,"materials":[
        {"name":"float","texture":"first.bmp","material":{"program":"test","params":{"uWear":0.1234567}}}
    ]})";
    const render::ServerMaterialBinding bindings[] = {{"float.mat", "float"}};
    std::vector<render::ServerMaterialParam> params = {{"uWear", -0.0000001f}};
    ASSERT_TRUE(render::ServerMaterials_ReplaceOwner({7, 10}, bindings, params));
    params.front().value = 0.75f;
    const ncl_material::Material* material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ASSERT_NE(material, nullptr);
    EXPECT_EQ(std::bit_cast<uint32_t>(material->FindParam("uWear")->scalar), std::bit_cast<uint32_t>(-0.0000001f));
    ASSERT_TRUE(render::ServerMaterials_ReplaceOwnerParams({7, 10}, {}));
    EXPECT_EQ(std::bit_cast<uint32_t>(material->FindParam("uWear")->scalar), std::bit_cast<uint32_t>(0.1234567f));
}

TEST_F(MaterialSyncTest, DirectFloatNonfiniteWritesAndSnapshotsPreserveState)
{
    sync_->OnCreate(10, Fields(7, 2, 0.1234567f));
    for (float invalid :
         {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
    {
        EXPECT_FALSE(render::ServerMaterials_SetParam(render::ServerMaterialBindingOwner{7, 10}, "uWear", invalid));
        EXPECT_FALSE(render::ServerMaterials_SetParam(7, "first.bmp/uWear", invalid));
        const render::ServerMaterialParam params[] = {{"uWear", invalid}};
        EXPECT_FALSE(render::ServerMaterials_ReplaceOwnerParams({7, 10}, params));
        const render::ServerMaterialBinding bindings[] = {{"test.mat", "second"}};
        EXPECT_FALSE(render::ServerMaterials_ReplaceOwner({7, 10}, bindings, params));
        sync_->OnCreate(10, Fields(8, 3, invalid));
        ncl_entity::ArrayStringIdFloatPairsDelta delta;
        delta.clear = true;
        delta.added = {{4, invalid}};
        sync_->OnUpdate(10, kParams, {{kParams, delta}});
        ExpectWear(7, "first.bmp", 0.1234567f);
        EXPECT_EQ(render::ServerMaterials_BindingFor({8, "second.bmp"}), nullptr);
    }
    ncl_entity::ArrayStringIdFloatPairsDelta valid;
    valid.added = {{4, -0.0000001f}};
    sync_->OnUpdate(10, kParams, {{kParams, valid}});
    ExpectWear(7, "first.bmp", -0.0000001f);
}

TEST_F(MaterialSyncTest, DirectFloatRejectedChoiceDoesNotPoisonFollowingOrdinaryUpdate)
{
    ncl_material::ParamSchema mode;
    mode.name = "uMode";
    mode.instance = true;
    mode.widget = ncl_material::ParamWidget::Choice;
    mode.choices = {"Off", "On", "Auto"};
    mode.has_default = true;
    mode.has_range = true;
    mode.min_value = 0.0f;
    mode.max_value = 2.0f;
    g_Manifest.parameters.push_back(mode);
    g_Strings.Register(5, "first.bmp/uMode");
    sync_->OnCreate(10, Fields(7, 2, 0.1234567f));
    ncl_entity::ArrayStringIdFloatPairsDelta params;
    params.added = {{5, 1.0f}};
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    const ncl_material::Material* material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("uMode"), nullptr);

    for (float invalid : {-0.5f, 0.1234567f, 2.5f})
    {
        EXPECT_FALSE(render::ServerMaterials_SetParam(render::ServerMaterialBindingOwner{7, 10}, "first.bmp/uMode", invalid));
        const render::ServerMaterialParam snapshot[] = {{"uWear", 0.75f}, {"uMode", invalid}};
        EXPECT_FALSE(render::ServerMaterials_ReplaceOwnerParams({7, 10}, snapshot));
        params.added = {{5, invalid}};
        sync_->OnUpdate(10, kParams, {{kParams, params}});
        EXPECT_EQ(material->FindParam("uMode")->scalar, 1.0f);
        params.added = {{4, -0.0000001f}};
        sync_->OnUpdate(10, kParams, {{kParams, params}});
        ExpectWear(7, "first.bmp", -0.0000001f);
        EXPECT_EQ(material->FindParam("uMode")->scalar, 1.0f);
    }
    params.added = {{5, 0.5f}};
    sync_->OnUpdate(10, kParams | kEntityIndex, {{kParams, params}, {kEntityIndex, uint32_t{8}}});
    ASSERT_NE(render::ServerMaterials_BindingFor({7, "first.bmp"}), nullptr);
    ASSERT_EQ(render::ServerMaterials_BindingFor({8, "first.bmp"}), nullptr);
    params.added = {{4, 0.1234567f}};
    sync_->OnUpdate(10, kParams, {{kParams, params}});
    ExpectWear(7, "first.bmp", 0.1234567f);

    for (const std::pair<float, float>& endpoints : {std::pair{-100.0f, 0.0f}, {100.0f, 2.0f}})
    {
        params.added = {{5, endpoints.first}};
        sync_->OnUpdate(10, kParams, {{kParams, params}});
        EXPECT_EQ(material->FindParam("uMode")->scalar, endpoints.second);
    }
}

TEST_F(MaterialSyncTest, DirectFloatConsoleRequiresCompleteFiniteNumber)
{
    render::ServerMaterials_SetBinding(7, "test.mat", "first");
    ASSERT_TRUE(render::ServerMaterials_SetParam(7, "uWear", 0.1234567f));
    render::ServerMaterials_RegisterCommands();
    for (const char* invalid : {"", "bad", "0.5tail", "nan", "inf", "-inf", "1e100"})
    {
        g_Arguments = {"r_mat_param", "7", "uWear", invalid};
        g_Commands.at("r_mat_param")();
        ExpectWear(7, "first.bmp", 0.1234567f);
    }
    g_Arguments = {"r_mat_param", "7", "first.bmp/uWear", "-0.0000001"};
    g_Commands.at("r_mat_param")();
    const ncl_material::Material* material = render::ServerMaterials_BindingFor({7, "first.bmp"});
    ASSERT_NE(material, nullptr);
    EXPECT_EQ(std::bit_cast<uint32_t>(material->FindParam("uWear")->scalar), std::bit_cast<uint32_t>(-0.0000001f));
}
