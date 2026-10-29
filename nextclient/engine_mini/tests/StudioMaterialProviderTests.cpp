#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include "StudioMaterialProviders.h"
#include "RadarGlContext.h"

class StudioMaterialProviderTest : public testing::Test
{
private:
    RadarGlContext context_;

protected:
    void SetUp() override
    {
        std::string error;
        ASSERT_TRUE(context_.Initialize(error)) << error;
    }
};

TEST_F(StudioMaterialProviderTest, IndependentInstancesCacheAndRetryTheirOwnProgramGeneration)
{
    StudioProgramSourceFixture first_programs;
    StudioProgramSourceFixture second_programs;
    StudioMaterialSourceFixture first_materials;
    StudioMaterialSourceFixture second_materials;
    ncl_gl::Program first_program;
    ncl_gl::Program second_program;
    first_program.prog = 11;
    second_program.prog = 22;
    first_programs.program = &first_program;
    second_programs.program = &second_program;
    performance_test::Core first_performance;
    render::StudioMaterialRuntime first(first_programs.manager, first_materials.catalog, first_materials.cache, first_performance.monitor);
    performance_test::Core second_performance;
    render::StudioMaterialRuntime second(
        second_programs.manager, second_materials.catalog, second_materials.cache, second_performance.monitor
    );
    EXPECT_EQ(first_programs.resolutions, 0u);
    EXPECT_TRUE(first_materials.manifest_queries.empty());
    ASSERT_NE(first.ProgramFor(nullptr), nullptr);
    EXPECT_EQ(first.ProgramFor(nullptr)->prog, 11u);
    EXPECT_EQ(second.ProgramFor(nullptr)->prog, 22u);

    ncl_material::Material material;
    material.program = "fixture";
    const ncl_gl::Program* borrowed = first.ProgramFor(&material);
    ASSERT_NE(borrowed, nullptr);
    EXPECT_EQ(second.ProgramFor(&material)->prog, 22u);
    first_programs.program = nullptr;
    EXPECT_EQ(first.ProgramFor(&material), borrowed);
    EXPECT_EQ(first_programs.manager.Get(first_programs.descriptors[1]), borrowed);
    EXPECT_EQ(first_programs.resolutions, 2u);
    EXPECT_EQ(first_programs.source_loads, 0u);
    first_programs.manager.OnDeviceLost();
    EXPECT_EQ(first.ProgramFor(&material), nullptr);
    first_programs.program = &second_program;
    EXPECT_EQ(first.ProgramFor(&material), nullptr);
    EXPECT_EQ(first_programs.resolutions, 3u);
    first_programs.manager.OnDeviceLost();
    ASSERT_NE(first.ProgramFor(&material), nullptr);
    EXPECT_EQ(first.ProgramFor(&material)->prog, 22u);
    EXPECT_EQ(first.ProgramFor(nullptr)->prog, 22u);
    EXPECT_EQ(second.ProgramFor(&material)->prog, 22u);
    EXPECT_EQ(second_programs.resolutions, 2u);
    EXPECT_STREQ(first_programs.descriptors[1].vs_name, "fixture.vert");
    EXPECT_STREQ(first_programs.descriptors[1].fs_name, "fixture.frag");
    first.InvalidatePrograms();
    EXPECT_EQ(first.ProgramFor(&material)->prog, 22u);
    EXPECT_EQ(first_programs.resolutions, 5u);
}

TEST_F(StudioMaterialProviderTest, WarmupFiltersSuppliedEnumerationAndRetainsDescriptorStrings)
{
    StudioProgramSourceFixture programs;
    StudioMaterialSourceFixture materials;
    ncl_gl::Program linked;
    programs.program = &linked;
    programs.names = {"accepted", "rejected"};
    materials.manifests.emplace("accepted", ncl_material::ProgramManifest{});
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(programs.manager, materials.catalog, materials.cache, runtime_performance.monitor);
    StudioProgramSourceFixture other_programs;
    StudioMaterialSourceFixture other_materials;
    other_programs.program = &linked;
    other_programs.names = programs.names;
    other_materials.manifests.emplace("rejected", ncl_material::ProgramManifest{});
    performance_test::Core other_performance;
    render::StudioMaterialRuntime other(other_programs.manager, other_materials.catalog, other_materials.cache, other_performance.monitor);
    runtime.WarmPrograms();
    other.WarmPrograms();

    ASSERT_EQ(programs.descriptors.size(), 2u);
    ASSERT_EQ(other_programs.descriptors.size(), 2u);
    EXPECT_STREQ(other_programs.descriptors[1].vs_name, "rejected.vert");
    EXPECT_STREQ(programs.descriptors[0].vs_name, render::kStudioBuiltinProgram.vs_name);
    EXPECT_STREQ(programs.descriptors[1].vs_name, "accepted.vert");
    render::ProgramDesc accepted = programs.descriptors[1];
    runtime.WarmPrograms();
    EXPECT_EQ(programs.resolutions, 2u);
    EXPECT_EQ(programs.compiled, 0);
    EXPECT_EQ(programs.ready, 2);
    EXPECT_EQ(other_programs.resolutions, 2u);

    for (int index = 0; index < 128; ++index)
    {
        ncl_material::Material material;
        material.program = "growth_" + std::to_string(index);
        runtime.ProgramFor(&material);
    }
    EXPECT_STREQ(accepted.vs_name, "accepted.vert");
    EXPECT_STREQ(accepted.fs_name, "accepted.frag");
    EXPECT_TRUE(materials.texture_queries.empty());
}

TEST_F(StudioMaterialProviderTest, DescriptorIdentityOwnsSourcesDefinesAndSeparateOrderedTables)
{
    StudioProgramSourceFixture fixture;
    ncl_gl::Program linked;
    fixture.program = &linked;
    const char* names[] = {"first", "second"};
    render::ProgramDesc desc{"source.vert", "source.frag", "DEFINE", names, 2, nullptr, 0};
    const ncl_gl::Program* first = fixture.manager.Get(desc);
    ASSERT_NE(first, nullptr);
    render::ProgramDesc attributes = desc;
    attributes.uniform_count = 0;
    attributes.attrib_names = names;
    attributes.attrib_count = 2;
    EXPECT_NE(fixture.manager.Get(attributes), first);
    const char* reversed[] = {"second", "first"};
    desc.uniform_names = reversed;
    EXPECT_NE(fixture.manager.Get(desc), first);
    desc.uniform_names = names;
    desc.defines = "OTHER";
    EXPECT_NE(fixture.manager.Get(desc), first);
    desc.defines = "DEFINE";
    desc.fs_name = "other.frag";
    EXPECT_NE(fixture.manager.Get(desc), first);
    desc.fs_name = "source.frag";
    EXPECT_EQ(fixture.manager.Get(desc), first);
    EXPECT_EQ(fixture.resolutions, 5u);

    desc.defines = nullptr;
    const ncl_gl::Program* no_defines = fixture.manager.Get(desc);
    desc.defines = "";
    EXPECT_EQ(fixture.manager.Get(desc), no_defines);
    EXPECT_EQ(fixture.resolutions, 6u);
}

TEST_F(StudioMaterialProviderTest, TextureOwnersCacheFailuresAndSeparateLiveReleaseFromLoss)
{
    StudioMaterialSourceFixture first;
    StudioMaterialSourceFixture second;
    first.textures = {{"image.png", 71}};
    first.neutral = 72;
    second.textures = {{"image.png", 81}};
    EXPECT_EQ(first.cache.Get("image.png"), 71u);
    EXPECT_EQ(second.cache.Get("image.png"), 81u);
    EXPECT_EQ(first.cache.Get("missing.png"), 0u);
    EXPECT_EQ(first.cache.Get("../invalid.png"), 0u);
    EXPECT_EQ(first.cache.Neutral(), 72u);
    EXPECT_EQ(first.cache.Get("image.png"), 71u);
    EXPECT_EQ(first.cache.Get("missing.png"), 0u);
    EXPECT_EQ(first.cache.Get("../invalid.png"), 0u);
    EXPECT_EQ(first.cache.Neutral(), 72u);
    EXPECT_EQ(first.texture_queries.size(), 2u);
    EXPECT_EQ(first.rejected, 1u);
    EXPECT_EQ(first.neutral_queries, 1u);
    first.cache.Clear();
    EXPECT_EQ(first.deletions, (std::vector<GLuint>{71, 72}));
    EXPECT_EQ(first.cache.Get("image.png"), 71u);
    first.device.OnContextDestroyed();
    EXPECT_EQ(first.deletions.size(), 2u);
    EXPECT_EQ(first.cache.Get("image.png"), 0u);
    EXPECT_EQ(second.cache.Get("image.png"), 81u);
    first.device.OnContextCreated();
    EXPECT_EQ(first.cache.Get("image.png"), 71u);
}

TEST_F(StudioMaterialProviderTest, CatalogChecksSourcesContractsAndRegistersLocalizationOnce)
{
    StudioProgramSourceFixture sources;
    StudioMaterialSourceFixture diagnostics;
    sources.sources = {
        {"checked.json",
         R"({"version":1,"program":"checked","strings":"resource/checked_%language%.txt","parameters":{"amount":{"type":"scalar","default":0.5}}})"},
        {"checked.vert", "vertex"},
        {"checked.frag", "fragment"},
        {"mismatch.json", R"({"version":1,"program":"other","parameters":{}})"},
        {"unpaired.json", R"({"version":1,"program":"unpaired","parameters":{}})"},
        {"unpaired.vert", "vertex"},
        {"contract.json", R"({"version":1,"program":"contract","render_contract":"unknown","parameters":{}})"},
        {"contract.vert", "vertex"},
        {"contract.frag", "fragment"}
    };
    render::MaterialManifestCatalog catalog(sources.manager, diagnostics);
    const ncl_material::ProgramManifest* checked = catalog.Get("checked");
    ASSERT_NE(checked, nullptr);
    EXPECT_EQ(diagnostics.localization, (std::vector<std::string>{"resource/checked_%language%.txt"}));
    unsigned int loads = sources.source_loads;
    EXPECT_EQ(catalog.Get("checked"), checked);
    ncl_material::Material material;
    material.program = "checked";
    ASSERT_NE(catalog.GetParam(&material, "amount"), nullptr);
    EXPECT_FLOAT_EQ(catalog.GetParam(&material, "amount")->scalar, 0.5f);
    EXPECT_EQ(sources.source_loads, loads);
    EXPECT_EQ(catalog.Get("mismatch"), nullptr);
    EXPECT_EQ(catalog.Get("unpaired"), nullptr);
    EXPECT_EQ(catalog.Get("contract"), nullptr);
    loads = sources.source_loads;
    EXPECT_EQ(catalog.Get("mismatch"), nullptr);
    EXPECT_EQ(catalog.Get("unpaired"), nullptr);
    EXPECT_EQ(catalog.Get("contract"), nullptr);
    EXPECT_EQ(sources.source_loads, loads);
    EXPECT_EQ(diagnostics.errors.size(), 3u);
    EXPECT_EQ(diagnostics.localization.size(), 1u);
}

TEST_F(StudioMaterialProviderTest, ShaderOwnerReleasesLiveNamesAndAbandonsLostNames)
{
    StudioProgramSourceFixture fixture;
    ncl_gl::Program linked;
    linked.prog = 93;
    fixture.program = &linked;
    ASSERT_NE(fixture.manager.Get(render::kStudioBuiltinProgram), nullptr);
    uint32_t generation = fixture.manager.get_generation();
    fixture.manager.Clear();
    EXPECT_NE(fixture.manager.get_generation(), generation);
    EXPECT_EQ(fixture.deletions, (std::vector<unsigned int>{93}));
    ASSERT_NE(fixture.manager.Get(render::kStudioBuiltinProgram), nullptr);
    fixture.device.OnContextDestroyed();
    EXPECT_EQ(fixture.deletions.size(), 1u);
    fixture.manager.Clear();
    EXPECT_EQ(fixture.deletions.size(), 1u);
    fixture.device.OnContextCreated();
    ASSERT_NE(fixture.manager.Get(render::kStudioBuiltinProgram), nullptr);
    EXPECT_EQ(fixture.resolutions, 3u);
}

TEST_F(StudioMaterialProviderTest, DeviceSnapshotAllowsReentrantUnregisterAndOwnersDetachOnDestruction)
{
    render::RenderDevice device;
    struct Resource : render::IDeviceResource
    {
        render::RenderDevice& device;
        render::IDeviceResource* remove{};
        int calls{};
        explicit Resource(render::RenderDevice& owner) :
            device(owner)
        {}
        void OnDeviceLost() override
        {
            ++calls;
            EXPECT_FALSE(device.is_context_alive());
            if (remove != nullptr)
            {
                device.Unregister(remove);
            }
        }
    };
    Resource first(device);
    Resource second(device);
    first.remove = &second;
    device.Register(&first);
    device.Register(&first);
    device.Register(&second);
    device.OnContextCreated();
    device.OnContextDestroyed();
    EXPECT_EQ(first.calls, 1);
    EXPECT_EQ(second.calls, 0);
    device.Unregister(&first);

    StudioProgramSourceFixture programs;
    StudioMaterialSourceFixture images;
    ncl_gl::Program linked;
    linked.prog = 101;
    programs.program = &linked;
    images.textures = {{"owned.png", 102}};
    {
        render::ShaderManager manager(device, programs);
        render::MaterialTextureCache textures(device, images);
        device.OnContextCreated();
        ASSERT_NE(manager.Get(render::kStudioBuiltinProgram), nullptr);
        EXPECT_EQ(textures.Get("owned.png"), 102u);
    }
    device.OnContextDestroyed();
    EXPECT_TRUE(programs.deletions.empty());
    EXPECT_TRUE(images.deletions.empty());
}
