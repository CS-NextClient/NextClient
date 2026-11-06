#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include "PerformanceTestCore.h"

#include <string>

#include <glad/glad.h>

#include "RadarGlContext.h"
#include "ShaderBindingTrace.h"
#include "PreviewTestOwners.h"
#include "StudioMaterialProviders.h"
#include "StudioTestPose.h"
#include "graphics/render/shader_includes.h"

namespace
{
    class PickSource final : public StudioMaterialSourceFixture
    {
    public:
        unsigned int builds{};
        std::string failed_fragment;

        bool LoadSource(const char* name, std::string& out) override
        {
            std::string path(name);
            std::string program = path.substr(0, path.rfind('.'));
            return manifests.contains(program) ? StudioMaterialSourceFixture::LoadSource(name, out)
                                               : render::GetShaderBackend().LoadSource(name, out);
        }

        bool Build(const render::ProgramDesc& desc, ncl_gl::Program& program) override
        {
            ++builds;
            if (failed_fragment == desc.fs_name)
            {
                return false;
            }

            const auto loader = [](std::string_view name, std::string& text) {
                return render::GetShaderBackend().LoadSource(std::string(name).c_str(), text);
            };
            ncl_utils::ResultT<std::string> vertex = render::ShaderIncludes_Expand(desc.vs_name, loader);
            ncl_utils::ResultT<std::string> fragment = render::ShaderIncludes_Expand(desc.fs_name, loader);
            if (vertex.has_error() || fragment.has_error())
            {
                return false;
            }

            std::string error;
            ncl_gl::LoadProgram(
                program,
                vertex->c_str(),
                fragment->c_str(),
                desc.uniform_names,
                desc.uniform_count,
                desc.attrib_names,
                desc.attrib_count,
                &error
            );
            EXPECT_TRUE(program.ok) << error;
            return program.ok;
        }

        void DeleteProgram(unsigned int program) override
        {
            glDeleteProgram(program);
        }
    };

    class PreviewInteractionGlTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        goldsrc::cvar_t developer_{};
        goldsrc::cvar_t* previous_developer_{};
        GLuint framebuffer_{};
        GLuint output_{};
        GLuint vao_{};
        GLuint vertices_{};
        GLuint bones_{};
        GLuint indices_{};
        GLuint texture_{};

    protected:
        render::StudioDrawPacket packet_{.bones = CapturePose()};

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            ASSERT_TRUE(GLAD_GL_VERSION_3_3);
            previous_developer_ = developer;
            developer = &developer_;
            glGenFramebuffers(1, &framebuffer_);
            glGenTextures(1, &output_);
            glBindTexture(GL_TEXTURE_2D, output_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_, 0);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            glGenVertexArrays(1, &vao_);
            glBindVertexArray(vao_);
            glGenBuffers(1, &vertices_);
            glBindBuffer(GL_ARRAY_BUFFER, vertices_);
            const float positions[] = {-.8f, -.8f, 0, 0, .8f, 0, .8f, -.8f, 0};
            glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
            glGenBuffers(1, &bones_);
            glBindBuffer(GL_ARRAY_BUFFER, bones_);
            const unsigned char bone_indices[9]{};
            glBufferData(GL_ARRAY_BUFFER, sizeof(bone_indices), bone_indices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(3);
            glVertexAttribIPointer(3, 3, GL_UNSIGNED_BYTE, 0, nullptr);
            glGenBuffers(1, &indices_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_);
            const unsigned int indices[] = {0, 1, 2};
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
            glGenTextures(1, &texture_);
            glBindTexture(GL_TEXTURE_2D, texture_);
            const unsigned char white[] = {255, 255, 255, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            packet_.vao = vao_;
            packet_.blend = 1;
            packet_.bone_count = 1;
            packet_.bones = CapturePose({{0, 1.0f}, {5, 1.0f}, {10, 1.0f}}, packet_.bones);
            packet_.raster.depth_range[1] = 1;
            for (int index = 0; index < 4; index++)
            {
                packet_.modelview[index * 5] = packet_.projection[index * 5] = 1;
            }
            render::StudioQueuedMesh mesh{};
            mesh.texture = texture_;
            mesh.skin = 7;
            mesh.index_count = 3;
            packet_.meshes.push_back(mesh);
        }

        void TearDown() override
        {
            glDeleteFramebuffers(1, &framebuffer_);
            glDeleteTextures(1, &output_);
            glDeleteTextures(1, &texture_);
            glDeleteVertexArrays(1, &vao_);
            glDeleteBuffers(1, &vertices_);
            glDeleteBuffers(1, &bones_);
            glDeleteBuffers(1, &indices_);
            developer = previous_developer_;
        }

        void Capture(render::preview::PreviewInteraction& interaction, int model_index = 2)
        {
            interaction.BeginFrame();
            interaction.BeginModel(model_index, PreviewModel{});
            PrepareCoveragePacket(render::preview::PreviewInteractionTestAccess::Coverage(interaction), packet_);
            interaction.Capture(packet_);
        }
        PreviewHit Pick(render::preview::PreviewInteraction& interaction)
        {
            return interaction.Render({0, 0, 32, 32}, {0, 0, 32, 32}, 16, 16);
        }
    };
} // namespace

TEST_F(PreviewInteractionGlTest, IndependentOwnersUseTheirSuppliedProgramsAndDeviceLoss)
{
    PickSource first;
    PickSource second;
    performance_test::Core first_coverage_performance;
    render::MaterialCoverageRuntime first_coverage(first.catalog, first.cache, first_coverage_performance.monitor);
    performance_test::Core second_coverage_performance;
    render::MaterialCoverageRuntime second_coverage(second.catalog, second.cache, second_coverage_performance.monitor);
    render::preview::PreviewInteraction first_pick(first.device, first.shaders, first_coverage);
    render::preview::PreviewInteraction second_pick(second.device, second.shaders, second_coverage);
    Capture(first_pick);
    Capture(second_pick, 9);
    EXPECT_EQ(Pick(first_pick).model_index, 2);
    EXPECT_EQ(Pick(second_pick).model_index, 9);
    EXPECT_EQ(first.builds, 2u);
    EXPECT_EQ(second.builds, 2u);
    first.device.OnContextDestroyed();
    EXPECT_EQ(render::preview::PreviewInteractionTestAccess::Count(first_pick), 0u);
    EXPECT_EQ(render::preview::PreviewInteractionTestAccess::Count(second_pick), 1u);
    EXPECT_EQ(Pick(second_pick).model_index, 9);
    EXPECT_EQ(second.builds, 2u);
    second_pick.Clear();
    second.shaders.Clear();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PreviewInteractionGlTest, FixedProgramFailuresRetryOnlyAfterTheirShaderGenerationChanges)
{
    PickSource source;
    performance_test::Core coverage_performance;
    render::MaterialCoverageRuntime coverage(source.catalog, source.cache, coverage_performance.monitor);
    render::preview::PreviewInteraction interaction(source.device, source.shaders, coverage);
    for (const char* fragment : {"preview_pick.frag", "preview_outline.frag"})
    {
        SCOPED_TRACE(fragment);
        source.shaders.Clear();
        source.failed_fragment = fragment;
        Capture(interaction);
        unsigned int before = source.builds;
        EXPECT_EQ(Pick(interaction).model_index, -1);
        EXPECT_EQ(source.builds, before + 2);
        source.failed_fragment.clear();
        EXPECT_EQ(Pick(interaction).model_index, -1);
        EXPECT_EQ(source.builds, before + 2);
        source.shaders.Clear();
        EXPECT_EQ(Pick(interaction).texture_index, 7);
        EXPECT_EQ(source.builds, before + 4);
        EXPECT_EQ(Pick(interaction).texture_index, 7);
        EXPECT_EQ(source.builds, before + 4);
    }
    interaction.Clear();
    source.shaders.Clear();
}

TEST_F(PreviewInteractionGlTest, RepeatedPickingRetainsIdentityScratchAcrossFramesAndClear)
{
    PickSource source;
    performance_test::Core coverage_performance;
    render::MaterialCoverageRuntime coverage(source.catalog, source.cache, coverage_performance.monitor);
    render::preview::PreviewInteraction interaction(source.device, source.shaders, coverage);
    Capture(interaction);
    EXPECT_EQ(Pick(interaction).texture_index, 7);
    size_t capacity = render::preview::PreviewInteractionTestAccess::IdentityCapacity(interaction);
    ASSERT_GT(capacity, 0u);
    for (int frame = 0; frame < 4; frame++)
    {
        Capture(interaction, frame);
        EXPECT_EQ(Pick(interaction).model_index, frame);
        EXPECT_EQ(render::preview::PreviewInteractionTestAccess::IdentityCapacity(interaction), capacity);
        EXPECT_EQ(source.builds, 2u);
    }
    interaction.Clear();
    EXPECT_EQ(render::preview::PreviewInteractionTestAccess::IdentityCapacity(interaction), capacity);
    EXPECT_EQ(Pick(interaction).model_index, -1);
    source.shaders.Clear();
}

TEST_F(PreviewInteractionGlTest, IndependentCoverageOwnersUseTheirOwnAcceptedManifest)
{
    PickSource first;
    PickSource second;
    std::string json;
    ASSERT_TRUE(render::GetShaderBackend().LoadSource("studio.json", json));
    ncl_utils::ResultT<ncl_material::ProgramManifest> parsed = ncl_material::ParseProgramManifest(json);
    ASSERT_FALSE(parsed.has_error());
    first.manifests["studio"] = *parsed;
    second.manifests["studio"] = *parsed;
    second.manifests["studio"].render_contract.clear();
    performance_test::Core first_coverage_performance;
    render::MaterialCoverageRuntime first_coverage(first.catalog, first.cache, first_coverage_performance.monitor);
    performance_test::Core second_coverage_performance;
    render::MaterialCoverageRuntime second_coverage(second.catalog, second.cache, second_coverage_performance.monitor);
    render::preview::PreviewInteraction first_pick(first.device, first.shaders, first_coverage);
    render::preview::PreviewInteraction second_pick(second.device, second.shaders, second_coverage);
    ncl_material::Material material;
    material.program = "studio";
    material.params.push_back({"uDissolve", {}});
    material.params.back().value.scalar = 1;
    packet_.meshes.front().material = material;
    Capture(first_pick);
    Capture(second_pick);

    EXPECT_EQ(Pick(first_pick).model_index, -1);
    EXPECT_EQ(Pick(second_pick).texture_index, 7);
    EXPECT_EQ(first.manifest_queries, (std::vector<std::string>{"studio"}));
    EXPECT_EQ(second.manifest_queries, first.manifest_queries);
    first_pick.Clear();
    second_pick.Clear();
    first.shaders.Clear();
    second.shaders.Clear();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PreviewInteractionGlTest, PreparedCoverageAndBonesRefreshAfterReloadAndStayWarmAcrossMeshes)
{
    PickSource source;
    performance_test::Core coverage_performance;
    render::MaterialCoverageRuntime coverage(source.catalog, source.cache, coverage_performance.monitor);
    render::preview::PreviewInteraction interaction(source.device, source.shaders, coverage);
    packet_.meshes.resize(3, packet_.meshes.front());
    Capture(interaction);
    ShaderBindingTrace trace;
    unsigned int cold_uniforms = 0;
    unsigned int cold_blocks = 0;

    for (int generation = 0; generation < 2; ++generation)
    {
        for (int draw = 0; draw < 3; ++draw)
        {
            EXPECT_EQ(Pick(interaction).texture_index, 7);

            if (draw == 0)
            {
                EXPECT_GT(trace.get_uniforms(), cold_uniforms);
                EXPECT_GT(trace.get_blocks(), cold_blocks);
                cold_uniforms = trace.get_uniforms();
                cold_blocks = trace.get_blocks();
            }
            else
            {
                EXPECT_EQ(trace.get_uniforms(), cold_uniforms);
                EXPECT_EQ(trace.get_blocks(), cold_blocks);
            }
        }

        source.shaders.Clear();
    }

    interaction.Clear();
}

TEST_F(PreviewInteractionGlTest, FrameRetirementReusesOwnedMaterialStorageWithinItsSeparateBudget)
{
    PickSource source;
    performance_test::Core coverage_performance;
    render::MaterialCoverageRuntime coverage(source.catalog, source.cache, coverage_performance.monitor);
    render::preview::PreviewInteraction interaction(source.device, source.shaders, coverage);
    using Access = render::preview::PreviewInteractionTestAccess;
    ncl_material::Material material;
    material.program = "studio_legacy";
    ncl_material::Param param;
    param.name.assign(80, 'n');
    param.value.texture_ref.assign(160, 't');
    material.params.push_back(param);
    ncl_material::OverlaySlot overlay;
    overlay.name.assign(80, 'o');
    material.overlays.push_back(overlay);
    packet_.meshes[0].material = material;
    Capture(interaction);
    const char* names = Access::Packet(interaction, 0).meshes[0].material->params[0].name.data();
    const char* refs = Access::Packet(interaction, 0).meshes[0].material->params[0].value.texture_ref.data();
    interaction.BeginFrame();
    EXPECT_EQ(Access::Count(interaction), 0u);
    EXPECT_GT(Access::RetainedBytes(interaction), 0u);
    EXPECT_LE(Access::RetainedBytes(interaction), 1024u * 1024u);
    EXPECT_EQ(Pick(interaction).model_index, -1);
    PrepareCoveragePacket(render::preview::PreviewInteractionTestAccess::Coverage(interaction), packet_);
    interaction.Capture(packet_);
    ASSERT_EQ(Access::Count(interaction), 1u);
    EXPECT_EQ(Access::Packet(interaction, 0).meshes[0].material->params[0].name.data(), names);
    EXPECT_EQ(Access::Packet(interaction, 0).meshes[0].material->params[0].value.texture_ref.data(), refs);
    packet_.meshes[0].material->params[0].value.texture_ref = "source changed";
    EXPECT_EQ(Access::Packet(interaction, 0).meshes[0].material->params[0].value.texture_ref, std::string(160, 't'));
    interaction.BeginFrame();
    packet_.meshes[0].material.reset();
    PrepareCoveragePacket(render::preview::PreviewInteractionTestAccess::Coverage(interaction), packet_);
    interaction.Capture(packet_);
    EXPECT_FALSE(Access::Packet(interaction, 0).meshes[0].material);
    EXPECT_EQ(Pick(interaction).texture_index, 7);
    interaction.BeginFrame();
    interaction.Clear();
    EXPECT_EQ(Access::RetainedBytes(interaction), 0u);
    EXPECT_EQ(Access::Count(interaction), 0u);
    source.shaders.Clear();
}

TEST_F(PreviewInteractionGlTest, NestedOversizeCaptureStorageIsRejectedAndLossReleasesRetainedPayload)
{
    PickSource source;
    performance_test::Core coverage_performance;
    render::MaterialCoverageRuntime coverage(source.catalog, source.cache, coverage_performance.monitor);
    render::preview::PreviewInteraction interaction(source.device, source.shaders, coverage);
    using Access = render::preview::PreviewInteractionTestAccess;
    packet_.meshes[0].material.emplace();
    packet_.meshes[0].material->program.assign(2 * 1024 * 1024, 'p');
    PrepareCoveragePacket(render::preview::PreviewInteractionTestAccess::Coverage(interaction), packet_);
    interaction.Capture(packet_);
    interaction.BeginFrame();
    EXPECT_EQ(Access::RetainedBytes(interaction), 0u);
    EXPECT_EQ(Access::Count(interaction), 0u);
    packet_.meshes[0].material.reset();
    PrepareCoveragePacket(render::preview::PreviewInteractionTestAccess::Coverage(interaction), packet_);
    interaction.Capture(packet_);
    interaction.BeginFrame();
    EXPECT_GT(Access::RetainedBytes(interaction), 0u);
    interaction.OnDeviceLost();
    EXPECT_EQ(Access::RetainedBytes(interaction), 0u);
    EXPECT_EQ(Access::Count(interaction), 0u);
}

TEST_F(PreviewInteractionGlTest, PreparedCoverageSurvivesOptimizedColorSamplerAndRejectsRetiredTextureRevision)
{
    PickSource source;
    std::string json;
    ASSERT_TRUE(render::GetShaderBackend().LoadSource("studio.json", json));
    ncl_utils::ResultT<ncl_material::ProgramManifest> parsed = ncl_material::ParseProgramManifest(json);
    ASSERT_FALSE(parsed.has_error());
    source.manifests["studio"] = *parsed;
    source.textures["fixtures/dissolve.png"] = packet_.meshes[0].texture;
    performance_test::Core coverage_performance;
    render::MaterialCoverageRuntime coverage(source.catalog, source.cache, coverage_performance.monitor);
    render::preview::PreviewInteraction interaction(source.device, source.shaders, coverage);
    ncl_material::Material material;
    material.program = "studio";
    material.params.push_back({"uDissolve", {}});
    material.params.back().value.scalar = 0.5f;
    ncl_material::Param map;
    map.name = "uDissolveMap";
    map.value.type = ncl_material::ParamType::TextureRef;
    map.value.texture_ref = "fixtures/dissolve.png";
    material.params.push_back(map);
    render::StudioMaterialBindings bindings;
    bindings.manifest = source.catalog.Get("studio");
    ASSERT_NE(bindings.manifest, nullptr);
    bindings.parameters.resize(bindings.manifest->parameters.size(), -1);
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(source.shaders, source.catalog, source.cache, runtime_performance.monitor);
    render::StudioMeshImages images;
    ASSERT_TRUE(runtime.PrepareImages(bindings, material, images));
    EXPECT_EQ(images.material_count, 0);
    EXPECT_TRUE(source.texture_queries.empty());
    ASSERT_TRUE(coverage.PrepareImages(&material, bindings.manifest, images));
    EXPECT_NE(images.coverage_dissolve_map, 0u);
    packet_.meshes[0].material = material;
    packet_.meshes[0].images = images;
    packet_.texture_revision = source.cache.get_revision();
    interaction.BeginModel(2, PreviewModel{});
    interaction.Capture(packet_);
    const size_t loads = source.texture_queries.size();
    const size_t manifests = source.manifest_queries.size();
    EXPECT_EQ(Pick(interaction).texture_index, 7);
    EXPECT_EQ(Pick(interaction).texture_index, 7);
    EXPECT_EQ(source.texture_queries.size(), loads);
    EXPECT_EQ(source.manifest_queries.size(), manifests);
    source.cache.Clear();
    EXPECT_EQ(Pick(interaction).model_index, -1);
    PrepareCoveragePacket(coverage, packet_);
    interaction.BeginFrame();
    interaction.Capture(packet_);
    EXPECT_EQ(Pick(interaction).texture_index, 7);
    EXPECT_EQ(source.texture_queries.size(), loads + 1);
    interaction.Clear();
    source.shaders.Clear();
}
