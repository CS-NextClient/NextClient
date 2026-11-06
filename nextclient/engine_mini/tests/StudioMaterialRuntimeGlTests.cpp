#include <gtest/gtest.h>

#include "PerformanceTestCore.h"
#include "PerformanceRuntimeTestSupport.h"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <utility>
#include <string_view>
#include <vector>

#include <glad/glad.h>
#include <ncl_gl/program.h>
#include <ncl_gl/loader.h>
#ifdef _WIN32
#include <Windows.h>
#endif

#include "RadarGlContext.h"
#include "StudioTextureUploadFixture.h"
#include "engine.h"
#include "PreviewTestOwners.h"
#include "common/cvar.h"
#include "common/memory/ScratchArena.h"
#include "common/memory/g_scratch_allocator.h"
#include "graphics/gl_local.h"
#include "graphics/preview/ScenePreview.h"
#include "graphics/render/material/material_textures.h"
#include "graphics/render/material/material_manifests.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"
#include "graphics/render/render_device.h"
#include "graphics/render/shader_manager.h"
#include "graphics/render/StudioMaterialRuntime.h"
#include "graphics/render/render_resources.h"
#include "StudioMaterialProviders.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/RendererLifecycle.h"
#include "graphics/render/render_context.h"
#include "performance/performance_integration.h"
#include "performance/PerformanceMonitor.h"
#include "performance/PerformanceRuntime.h"
#include "chain/HandlersChain.h"
#include "performance/GpuQueryBackend.h"
#include "graphics/texture_loader.h"
#ifdef _WIN32
#include "graphics/text/text_render.h"
#endif

nitroapi::NitroApiInterface* StudioTest_Engine();

namespace
{
    PFNGLGETFLOATVPROC g_MapperOriginalGetFloat{};
    PFNGLBEGINPROC g_MapperOriginalBegin{};
    uint32_t g_MapperMatrixReads{};
    uint32_t g_MapperOutlines{};

    void APIENTRY ObserveMapperFloat(GLenum name, GLfloat* value)
    {
        if (name == GL_MODELVIEW_MATRIX || name == GL_PROJECTION_MATRIX)
        {
            ++g_MapperMatrixReads;
        }
        g_MapperOriginalGetFloat(name, value);
    }

    void APIENTRY ObserveMapperBegin(GLenum mode)
    {
        if (mode == GL_LINES)
        {
            ++g_MapperOutlines;
        }
        g_MapperOriginalBegin(mode);
    }

    class MapperGlProbe
    {
    public:
        MapperGlProbe()
        {
            g_MapperMatrixReads = 0;
            g_MapperOutlines = 0;
            Rebind();
        }
        ~MapperGlProbe()
        {
            glad_glGetFloatv = g_MapperOriginalGetFloat;
            glad_glBegin = g_MapperOriginalBegin;
        }
        static void Rebind()
        {
            g_MapperOriginalGetFloat = glad_glGetFloatv;
            g_MapperOriginalBegin = glad_glBegin;
            glad_glGetFloatv = ObserveMapperFloat;
            glad_glBegin = ObserveMapperBegin;
        }
    };

    struct MapperRuntimeSession
    {
        nitroapi::EngineData engine;
        nitroapi::ClientData client;
        performance_test::MapperInputs inputs{engine};
        performance::PerformanceRuntime runtime;

        MapperRuntimeSession(performance_test::Core& core, render::RendererResources& resources, render::RendererLifecycle& lifecycle) :
            runtime(
                engine,
                client,
                nullptr,
                nullptr,
                core.monitor,
                core.mapper,
                core.clock,
                resources.device,
                lifecycle,
#ifdef _WIN32
                TextRender_Get(),
#else
                nullptr,
#endif
                8684
            )
        {
            inputs.Enable(runtime, 2);
        }
    };

    constexpr const char* kRuntimeImagePath = "fixtures/runtime-color.png";
    constexpr std::array<unsigned char, 4> kRuntimeImagePixel = {32, 96, 192, 255};
    unsigned int g_RuntimeImageLoads{};
    bool g_RuntimeImageScratchRestored{};
    constexpr const char* kRuntimeVertex = R"(
#version 330 compatibility
layout(std140) uniform Bones { vec4 bones[384]; };
void main() { gl_Position = vec4(bones[0].x, 0, 0, 1); }
)";
    constexpr const char* kRuntimeFragment = R"(
#version 330 compatibility
uniform sampler2D uTexture;
uniform sampler2D uPattern;
uniform sampler2D uUnderlay;
uniform sampler2D uWearMap;
uniform vec4 uBaseTint;
uniform float uWear;
uniform float uLightGamma[256];
uniform int gammaIndex;
void main()
{
    gl_FragColor = (texture(uTexture, vec2(0.5)) + texture(uPattern, vec2(0.5))
        + texture(uUnderlay, vec2(0.5))) * uBaseTint * uWear * uLightGamma[gammaIndex];
}
)";

    void BindLifecycleTexture(void*, unsigned int texture)
    {
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    PFNGLGETUNIFORMLOCATIONPROC g_GetUniformLocation{};
    PFNGLGETUNIFORMBLOCKINDEXPROC g_GetUniformBlockIndex{};
    std::map<std::string, unsigned int> g_UniformLocationQueries;
    std::map<std::string, unsigned int> g_UniformBlockQueries;

    GLint APIENTRY RecordUniformLocation(GLuint program, const GLchar* name)
    {
        ++g_UniformLocationQueries[name];
        return g_GetUniformLocation(program, name);
    }

    GLuint APIENTRY RecordUniformBlockIndex(GLuint program, const GLchar* name)
    {
        ++g_UniformBlockQueries[name];
        return g_GetUniformBlockIndex(program, name);
    }

    class RuntimeLookupTrace
    {
    public:
        RuntimeLookupTrace()
        {
            g_GetUniformLocation = glad_glGetUniformLocation;
            g_GetUniformBlockIndex = glad_glGetUniformBlockIndex;
            g_UniformLocationQueries.clear();
            g_UniformBlockQueries.clear();
            glad_glGetUniformLocation = RecordUniformLocation;
            glad_glGetUniformBlockIndex = RecordUniformBlockIndex;
        }

        ~RuntimeLookupTrace()
        {
            glad_glGetUniformLocation = g_GetUniformLocation;
            glad_glGetUniformBlockIndex = g_GetUniformBlockIndex;
        }
    };

    PFNGLDELETEBUFFERSPROC g_DeleteBuffers{};
    PFNGLDELETETEXTURESPROC g_DeleteTextures{};
    PFNGLDELETEPROGRAMPROC g_DeleteProgram{};
    std::vector<GLuint> g_DeletedBuffers;
    std::vector<GLuint> g_DeletedTextures;
    std::vector<GLuint> g_DeletedPrograms;

    void APIENTRY RecordBufferDeletes(GLsizei count, const GLuint* names)
    {
        g_DeletedBuffers.insert(g_DeletedBuffers.end(), names, names + count);
        g_DeleteBuffers(count, names);
    }

    void APIENTRY RecordTextureDeletes(GLsizei count, const GLuint* names)
    {
        g_DeletedTextures.insert(g_DeletedTextures.end(), names, names + count);
        g_DeleteTextures(count, names);
    }

    void APIENTRY RecordProgramDelete(GLuint name)
    {
        g_DeletedPrograms.push_back(name);
        g_DeleteProgram(name);
    }

    class LifecycleRuntimeQueries : public performance::GpuQueryBackendInterface
    {
    public:
        uint32_t deletions{};
        uint32_t writes{};
        bool IsTimestampSupported() const override
        {
            return true;
        }
        int counter_bits() const override
        {
            return 64;
        }
        void Allocate(uint32_t count, uint32_t* names) override
        {
            for (uint32_t index = 0; index < count; ++index)
            {
                names[index] = index + 1;
            }
        }
        void Delete(uint32_t, const uint32_t*) override
        {
            ++deletions;
        }
        void Write(uint32_t) override
        {
            ++writes;
        }
        bool IsReady(uint32_t) override
        {
            return false;
        }
        uint64_t Read(uint32_t) override
        {
            ADD_FAILURE() << "A retired runtime query must not be read";
            return 0;
        }
    };

    class LifecycleDeleteTrace
    {
    public:
        LifecycleDeleteTrace()
        {
            g_DeleteBuffers = glad_glDeleteBuffers;
            g_DeleteTextures = glad_glDeleteTextures;
            g_DeleteProgram = glad_glDeleteProgram;
            g_DeletedBuffers.clear();
            g_DeletedTextures.clear();
            g_DeletedPrograms.clear();
            glad_glDeleteBuffers = RecordBufferDeletes;
            glad_glDeleteTextures = RecordTextureDeletes;
            glad_glDeleteProgram = RecordProgramDelete;
        }

        ~LifecycleDeleteTrace()
        {
            glad_glDeleteBuffers = g_DeleteBuffers;
            glad_glDeleteTextures = g_DeleteTextures;
            glad_glDeleteProgram = g_DeleteProgram;
        }
    };

    class StudioMaterialRuntimeGlTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        goldsrc::cvar_t developer_{};
        int current_texture_ = -1;
        bool ready_{};

    protected:
        render::RendererResources& resources_ = render::GetResources();
        performance_test::Core runtime_performance_;
        render::StudioMaterialRuntime runtime_{resources_.shaders, resources_.manifests, resources_.textures, runtime_performance_.monitor};
        ncl_gl::Program program_{};
        render::StudioMaterialBindings bindings_;

        const render::StudioMaterialBindings& Bindings(
            render::StudioMaterialRuntime& runtime,
            const ncl_material::Material* material = nullptr
        )
        {
            runtime.PrepareBindings(bindings_, program_, material);
            return bindings_;
        }

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            ASSERT_TRUE(GLAD_GL_VERSION_3_3);
            developer = &developer_;
            g_NitroApi = StudioTest_Engine();
            p_currenttexture = &current_texture_;
            qglBindTexture = glad_glBindTexture;
            qglColorTableEXT = nullptr;
            g_RuntimeImageLoads = 0;
            g_RuntimeImageScratchRestored = false;
            ready_ = true;
            render::Device_OnContextCreated();
            program_.prog = ncl_gl::LinkProgram(kRuntimeVertex, kRuntimeFragment, nullptr, 0, &error);
            ASSERT_NE(program_.prog, 0u) << error;
            glUseProgram(program_.prog);
        }

        void TearDown() override
        {
            if (!ready_)
            {
                return;
            }

            ASSERT_TRUE(context_.MakeCurrent());
            glUseProgram(0);
            glDeleteProgram(program_.prog);
            render::MaterialTextures_Clear();
            EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
            render::Device_OnContextDestroyed();
            p_currenttexture = nullptr;
            g_NitroApi = nullptr;
            developer = nullptr;
        }

        GLint Sampler(const char* name)
        {
            GLint value = -1;
            glGetUniformiv(program_.prog, glGetUniformLocation(program_.prog, name), &value);
            return value;
        }

        float Scalar(const char* name)
        {
            float value = -1;
            glGetUniformfv(program_.prog, glGetUniformLocation(program_.prog, name), &value);
            return value;
        }
    };
} // namespace

TEST_F(StudioMaterialRuntimeGlTest, InjectedMaterialsSelectDefaultsAndNeutralImagesBeforeBinding)
{
    class MaterialSource final : public StudioMaterialSourceFixture
    {
    public:
        std::array<GLuint, 2> original{};
        unsigned int checked_loads{};

        GLuint Load(const char* path, tex::TextureLoadGuard) override
        {
            CheckBindings();

            return StudioMaterialSourceFixture::Load(path, {});
        }

        GLuint CreateNeutral() override
        {
            CheckBindings();

            return StudioMaterialSourceFixture::CreateNeutral();
        }

    private:
        void CheckBindings()
        {
            GLint active = 0;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &active);

            for (int slot = 0; slot < 2; ++slot)
            {
                glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
                GLint binding = 0;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
                EXPECT_EQ(binding, static_cast<GLint>(original[slot]));
            }

            glActiveTexture(active);
            ++checked_loads;
        }
    };

    StudioProgramSourceFixture programs;
    MaterialSource materials;
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1, "program": "injected", "parameters": {
            "uPattern": {"type": "texture"},
            "uUnderlay": {"type": "texture", "default": "default.png"}
        }
    })");
    ASSERT_FALSE(manifest.has_error());
    materials.manifests.emplace("injected", std::move(*manifest));
    GLuint textures[5]{};
    glGenTextures(5, textures);
    materials.original = {textures[0], textures[1]};
    materials.textures = {{"selected.png", textures[2]}, {"default.png", textures[3]}};
    materials.neutral = textures[4];

    for (int slot = 0; slot < 2; ++slot)
    {
        glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
        glBindTexture(GL_TEXTURE_2D, materials.original[slot]);
    }

    glActiveTexture(GL_TEXTURE0);
    ncl_material::Material material;
    material.program = "injected";
    material.params.push_back({"uPattern", {}});
    material.params.back().value.type = ncl_material::ParamType::TextureRef;
    material.params.back().value.texture_ref = "selected.png";
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(programs.manager, materials.catalog, materials.cache, runtime_performance.monitor);
    render::StudioMaterialState state;
    EXPECT_EQ(UploadPreparedMaterial(runtime, Bindings(runtime, &material), material, state), 2);
    EXPECT_EQ(materials.checked_loads, 2u);
    EXPECT_EQ(materials.texture_queries, (std::vector<std::string>{"selected.png", "default.png"}));
    EXPECT_EQ(materials.neutral_queries, 0u);

    for (int slot = 0; slot < 2; ++slot)
    {
        glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
        GLint binding = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        EXPECT_EQ(binding, static_cast<GLint>(textures[2 + slot]));
    }

    state.Restore();
    materials.texture_queries.clear();
    materials.cache.Forget();
    materials.textures.clear();
    material.params.clear();
    EXPECT_EQ(UploadPreparedMaterial(runtime, Bindings(runtime, &material), material, state), 2);
    EXPECT_EQ(materials.checked_loads, 4u);
    EXPECT_EQ(materials.texture_queries, (std::vector<std::string>{"default.png"}));
    EXPECT_EQ(materials.neutral_queries, 1u);

    for (int slot = 0; slot < 2; ++slot)
    {
        glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
        GLint binding = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        EXPECT_EQ(binding, static_cast<GLint>(materials.neutral));
    }

    state.Restore();
    glDeleteTextures(5, textures);
}

TEST_F(StudioMaterialRuntimeGlTest, SuccessiveMaterialsResetEveryActiveSamplerAndRestoreBindings)
{
    ncl_material::Material material;
    material.program = "weapon_finish";
    material.params.push_back({"uPattern", {}});
    material.params.back().value.type = ncl_material::ParamType::TextureRef;
    material.params.back().value.texture_ref = kRuntimeImagePath;
    material.params.push_back({"uWearMap", material.params.back().value});
    material.params.back().value.texture_ref = "../refused.png";
    render::StudioMaterialState state;
    GLuint original[2]{};
    glGenTextures(2, original);

    for (int slot = 0; slot < 2; ++slot)
    {
        glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
        glBindTexture(GL_TEXTURE_2D, original[slot]);
    }

    ASSERT_EQ(glGetUniformLocation(program_.prog, "uWearMap"), -1);
    glActiveTexture(GL_TEXTURE0);
    EXPECT_EQ(UploadPreparedMaterial(runtime_, Bindings(runtime_, &material), material, state), 2);
    GLuint selected = render::MaterialTexture_Get(kRuntimeImagePath);
    ASSERT_NE(selected, 0u);
    EXPECT_NE(selected, render::MaterialTexture_Neutral());
    EXPECT_EQ(g_RuntimeImageLoads, 1u);
    EXPECT_TRUE(g_RuntimeImageScratchRestored);
    glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit);
    GLint bound = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    EXPECT_EQ(bound, static_cast<GLint>(selected));
    std::array<unsigned char, 4> selected_pixel{};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, selected_pixel.data());
    EXPECT_EQ(selected_pixel, kRuntimeImagePixel);
    glActiveTexture(GL_TEXTURE0);
    material.params.clear();
    glUniform1i(glGetUniformLocation(program_.prog, "uPattern"), 0);
    glUniform1i(glGetUniformLocation(program_.prog, "uUnderlay"), 0);
    EXPECT_EQ(UploadPreparedMaterial(runtime_, Bindings(runtime_, &material), material, state), 2);
    EXPECT_EQ(Sampler("uPattern"), render::kFirstMaterialUnit);
    EXPECT_EQ(Sampler("uUnderlay"), render::kFirstMaterialUnit + 1);

    for (int slot = 0; slot < 2; ++slot)
    {
        glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
        GLint bound = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        EXPECT_EQ(bound, static_cast<GLint>(render::MaterialTexture_Neutral()));
        std::array<unsigned char, 4> pixel{};
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        EXPECT_EQ(pixel, (std::array<unsigned char, 4>{255, 255, 255, 255}));
    }

    state.Restore();

    for (int slot = 0; slot < 2; ++slot)
    {
        glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
        GLint bound = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        EXPECT_EQ(bound, static_cast<GLint>(original[slot]));
    }

    glDeleteTextures(2, original);
}

TEST_F(StudioMaterialRuntimeGlTest, RepeatedMaterialUploadsCacheInactiveNamesBeyondTheDescriptorCapacity)
{
    const ncl_material::ProgramManifest* manifest = render::Manifest_Get("weapon_finish");
    ASSERT_NE(manifest, nullptr);
    ASSERT_GT(manifest->parameters.size(), static_cast<size_t>(ncl_gl::kMaxUniforms));
    ncl_material::Material material;
    material.program = "weapon_finish";
    material.params.push_back({"uWearMap", {}});
    material.params.back().value.type = ncl_material::ParamType::TextureRef;
    material.params.back().value.texture_ref = kRuntimeImagePath;
    material.params.push_back({"uDissolveMap", material.params.back().value});
    material.params.push_back({"uDissolve", {}});
    material.params.back().value.scalar = 0.5f;
    render::StudioMaterialState state;

    {
        RuntimeLookupTrace trace;
        const render::StudioMaterialBindings& bindings = Bindings(runtime_, &material);
        const render::MaterialCoverageBindings coverage = render::MaterialCoverageBindings::Prepare(program_);
        EXPECT_EQ(UploadPreparedMaterial(runtime_, bindings, material, state), 2);
        EXPECT_EQ(UploadPreparedMaterial(runtime_, bindings, material, state), 2);
        EXPECT_EQ(g_RuntimeImageLoads, 0u);
        UploadPreparedCoverage(render::GetRendererState().coverage, coverage, &material, {0, 0, 16, 16}, 0, state);
        EXPECT_EQ(g_RuntimeImageLoads, 1u);
        UploadPreparedCoverage(render::GetRendererState().coverage, coverage, &material, {0, 0, 16, 16}, 1, state);
        EXPECT_EQ(UploadPreparedMaterial(runtime_, bindings, material, state), 2);
        EXPECT_EQ(g_RuntimeImageLoads, 1u);
        EXPECT_EQ(g_UniformLocationQueries["uEffectViewport"], 1u);
        EXPECT_EQ(g_UniformLocationQueries["uEffectTime"], 1u);

        for (const ncl_material::ParamSchema& schema : manifest->parameters)
        {
            EXPECT_EQ(g_UniformLocationQueries[schema.name], 1u) << schema.name;
        }
    }

    EXPECT_EQ(g_RuntimeImageLoads, 1u);
    EXPECT_EQ(Sampler("uPattern"), render::kFirstMaterialUnit);
    EXPECT_EQ(Sampler("uUnderlay"), render::kFirstMaterialUnit + 1);
    state.Restore();
}

TEST_F(StudioMaterialRuntimeGlTest, GammaRevisionsRetainUniformAndMissingBlockLookups)
{
    std::array<float, render::kStudioLightGammaEntries> gamma;
    gamma.fill(0.25f);

    {
        RuntimeLookupTrace trace;
        runtime_.SetLightGamma(gamma);
        runtime_.ConfigureProgram(Bindings(runtime_));
        gamma.fill(0.75f);
        runtime_.SetLightGamma(gamma);
        runtime_.ConfigureProgram(Bindings(runtime_));
        EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
        EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
        EXPECT_EQ(g_UniformLocationQueries["uTexture"], 1u);
        EXPECT_EQ(g_UniformLocationQueries["uLightGamma"], 1u);
        EXPECT_EQ(g_UniformBlockQueries["Bones"], 1u);
        EXPECT_EQ(g_UniformBlockQueries["MissingBlock"], 1u);
    }

    EXPECT_FLOAT_EQ(Scalar("uLightGamma[17]"), 0.75f);
    EXPECT_EQ(Sampler("uTexture"), render::kStudioBaseTextureUnit);
}

TEST_F(StudioMaterialRuntimeGlTest, PreparedSchemaBindingsRefreshForReusedProgramIdentityAndGeneration)
{
    ncl_material::Material material;
    material.program = "weapon_finish";
    material.params.push_back({"uWearMap", {}});
    material.params.back().value.type = ncl_material::ParamType::TextureRef;
    material.params.back().value.texture_ref = kRuntimeImagePath;
    render::StudioMaterialState state;
    RuntimeLookupTrace trace;
    const render::StudioMaterialBindings& initial = Bindings(runtime_, &material);
    ASSERT_NE(initial.manifest, nullptr);
    EXPECT_EQ(initial.parameters.size(), initial.manifest->parameters.size());
    const uint32_t initial_generation = initial.generation;
    EXPECT_EQ(UploadPreparedMaterial(runtime_, initial, material, state), 2);
    state.Restore();
    EXPECT_EQ(program_.UniformLocation("uWearMap"), -1);
    EXPECT_NE(program_.UniformBlockIndex("Bones"), GL_INVALID_INDEX);
    EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(g_UniformLocationQueries["uWearMap"], 1u);
    EXPECT_EQ(g_RuntimeImageLoads, 0u);
    const GLuint program_name = program_.prog;
    GLuint attached[2]{};
    GLsizei attached_count = 0;
    glGetAttachedShaders(program_name, 2, &attached_count, attached);

    for (GLsizei index = 0; index < attached_count; ++index)
    {
        glDetachShader(program_name, attached[index]);
    }

    std::string fragment = kRuntimeFragment;
    const std::string old_expression = "+ texture(uUnderlay, vec2(0.5)))";
    const size_t expression = fragment.find(old_expression);
    ASSERT_NE(expression, std::string::npos);
    fragment.replace(expression, old_expression.size(), "+ texture(uUnderlay, vec2(0.5)) + texture(uWearMap, vec2(0.5)))");
    GLuint vertex_shader = ncl_gl::CompileShader(GL_VERTEX_SHADER, kRuntimeVertex);
    GLuint fragment_shader = ncl_gl::CompileShader(GL_FRAGMENT_SHADER, fragment.c_str());
    ASSERT_NE(vertex_shader, 0u);
    ASSERT_NE(fragment_shader, 0u);
    glAttachShader(program_name, vertex_shader);
    glAttachShader(program_name, fragment_shader);
    glLinkProgram(program_name);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    GLint linked = GL_FALSE;
    glGetProgramiv(program_name, GL_LINK_STATUS, &linked);
    ASSERT_EQ(linked, GL_TRUE);
    const char* names[] = {"uTexture", "uWearMap", "uMissing"};
    program_.ResolveLocations(names, 3);
    render::Shaders_Clear();
    glUseProgram(program_name);
    const render::StudioMaterialBindings& refreshed = Bindings(runtime_, &material);
    runtime_.ConfigureProgram(refreshed);
    EXPECT_EQ(&refreshed, &initial);
    EXPECT_NE(refreshed.generation, initial_generation);
    ASSERT_EQ(program_.prog, program_name);
    ASSERT_GE(program_.loc[1], 0);
    EXPECT_EQ(program_.loc[2], -1);
    EXPECT_EQ(program_.loc[3], -1);
    EXPECT_EQ(UploadPreparedMaterial(runtime_, Bindings(runtime_, &material), material, state), 3);
    EXPECT_EQ(UploadPreparedMaterial(runtime_, Bindings(runtime_, &material), material, state), 3);
    EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(g_UniformLocationQueries["uWearMap"], 2u);
    EXPECT_EQ(g_UniformBlockQueries["Bones"], 2u);
    EXPECT_EQ(g_UniformBlockQueries["MissingBlock"], 2u);
    EXPECT_EQ(g_RuntimeImageLoads, 1u);
    EXPECT_EQ(Sampler("uWearMap"), render::kFirstMaterialUnit + 2);
    glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + 2);
    std::array<unsigned char, 4> pixel{};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, kRuntimeImagePixel);
    state.Restore();
}

TEST_F(StudioMaterialRuntimeGlTest, SourceReloadReplacesTheProgramOwnedLookupTables)
{
    RuntimeLookupTrace trace;
    EXPECT_EQ(program_.UniformLocation("uWearMap"), -1);
    EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    glUseProgram(0);
    glDeleteProgram(program_.prog);
    const char* names[] = {"uWearMap", "uTexture"};
    ncl_gl::LoadProgram(program_, kRuntimeVertex, kRuntimeFragment, names, 2);
    ASSERT_TRUE(program_.ok);
    EXPECT_EQ(program_.loc[0], -1);
    EXPECT_GE(program_.loc[1], 0);
    EXPECT_EQ(program_.UniformLocation("uTexture"), program_.loc[1]);
    EXPECT_EQ(g_UniformLocationQueries["uTexture"], 1u);
    EXPECT_EQ(program_.UniformLocation("uWearMap"), -1);
    EXPECT_EQ(program_.UniformLocation("uWearMap"), -1);
    EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(g_UniformLocationQueries["uWearMap"], 2u);
    EXPECT_EQ(g_UniformBlockQueries["MissingBlock"], 2u);
}

TEST_F(StudioMaterialRuntimeGlTest, BinaryReloadReplacesTheProgramOwnedLookupTables)
{
    if (!ncl_gl::ProgramBinarySupported())
    {
        GTEST_SKIP() << "The GL driver does not support program binaries";
    }

    GLenum format = 0;
    std::vector<uint8_t> binary;
    ASSERT_TRUE(ncl_gl::GetProgramBinary(program_.prog, format, binary));
    RuntimeLookupTrace trace;
    EXPECT_EQ(program_.UniformLocation("uWearMap"), -1);
    EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    glUseProgram(0);
    glDeleteProgram(program_.prog);
    const char* names[] = {"uWearMap", "uTexture"};
    ncl_gl::LoadProgramBinary(program_, binary.data(), binary.size(), format, names, 2);
    ASSERT_TRUE(program_.ok);
    EXPECT_EQ(program_.loc[0], -1);
    EXPECT_GE(program_.loc[1], 0);
    EXPECT_EQ(program_.UniformLocation("uTexture"), program_.loc[1]);
    EXPECT_EQ(g_UniformLocationQueries["uTexture"], 1u);
    EXPECT_EQ(program_.UniformLocation("uWearMap"), -1);
    EXPECT_EQ(program_.UniformLocation("uWearMap"), -1);
    EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(program_.UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(g_UniformLocationQueries["uWearMap"], 2u);
    EXPECT_EQ(g_UniformBlockQueries["MissingBlock"], 2u);
}

TEST_F(StudioMaterialRuntimeGlTest, ShaderGenerationAndContextLossReplaceOwnedLookupTables)
{
    const ncl_gl::Program* first = runtime_.ProgramFor(nullptr);
    ASSERT_NE(first, nullptr);
    RuntimeLookupTrace trace;
    EXPECT_EQ(first->UniformLocation("uRuntimeMissing"), -1);
    EXPECT_EQ(first->UniformLocation("uRuntimeMissing"), -1);
    EXPECT_EQ(first->UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(first->UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(g_UniformLocationQueries["uRuntimeMissing"], 1u);
    EXPECT_EQ(g_UniformBlockQueries["MissingBlock"], 1u);
    render::Shaders_Clear();
    const ncl_gl::Program* second = runtime_.ProgramFor(nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->UniformLocation("uRuntimeMissing"), -1);
    EXPECT_EQ(second->UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(g_UniformLocationQueries["uRuntimeMissing"], 2u);
    EXPECT_EQ(g_UniformBlockQueries["MissingBlock"], 2u);
    const GLuint abandoned = second->prog;
    render::Device_OnContextDestroyed();
    render::Device_OnContextCreated();
    // Context creation reloads the GL function table.
    glad_glGetUniformLocation = RecordUniformLocation;
    glad_glGetUniformBlockIndex = RecordUniformBlockIndex;
    const ncl_gl::Program* third = runtime_.ProgramFor(nullptr);
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(third->UniformLocation("uRuntimeMissing"), -1);
    EXPECT_EQ(third->UniformLocation("uRuntimeMissing"), -1);
    EXPECT_EQ(third->UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(third->UniformBlockIndex("MissingBlock"), GL_INVALID_INDEX);
    EXPECT_EQ(g_UniformLocationQueries["uRuntimeMissing"], 3u);
    EXPECT_EQ(g_UniformBlockQueries["MissingBlock"], 3u);
    glDeleteProgram(abandoned);
}

TEST_F(StudioMaterialRuntimeGlTest, ScalarAndVectorDefaultsReplacePreviousMaterialValues)
{
    ncl_material::Material material;
    material.program = "weapon_finish";
    material.params.push_back({"uWear", {}});
    material.params.back().value.scalar = 0.75f;
    material.params.push_back({"uBaseTint", {}});
    material.params.back().value.type = ncl_material::ParamType::Vec4;
    material.params.back().value.vec4 = {0.1f, 0.2f, 0.3f, 0.4f};
    render::StudioMaterialState state;
    UploadPreparedMaterial(runtime_, Bindings(runtime_, &material), material, state);
    EXPECT_FLOAT_EQ(Scalar("uWear"), 0.75f);
    std::array<float, 4> tint{};
    glGetUniformfv(program_.prog, glGetUniformLocation(program_.prog, "uBaseTint"), tint.data());
    EXPECT_EQ(tint, material.params.back().value.vec4);
    material.params.clear();
    UploadPreparedMaterial(runtime_, Bindings(runtime_, &material), material, state);
    EXPECT_FLOAT_EQ(Scalar("uWear"), 0.0f);
    glGetUniformfv(program_.prog, glGetUniformLocation(program_.prog, "uBaseTint"), tint.data());
    EXPECT_EQ(tint, (std::array<float, 4>{1, 1, 1, 1}));
    state.Restore();
}

TEST_F(StudioMaterialRuntimeGlTest, GammaRevisionAndShaderGenerationReconfigureTheSameProgramName)
{
    std::array<float, render::kStudioLightGammaEntries> gamma;
    gamma.fill(0.25f);
    runtime_.SetLightGamma(gamma);
    runtime_.ConfigureProgram(Bindings(runtime_));
    EXPECT_FLOAT_EQ(Scalar("uLightGamma[17]"), 0.25f);
    gamma.fill(0.75f);
    runtime_.SetLightGamma(gamma);
    gamma.fill(0.0f);
    runtime_.ConfigureProgram(Bindings(runtime_));
    EXPECT_FLOAT_EQ(Scalar("uLightGamma[17]"), 0.75f);

    const ncl_gl::Program* owned = runtime_.ProgramFor(nullptr);
    ASSERT_NE(owned, nullptr);
    const uint32_t generation = render::Shaders_Generation();
    render::MaterialTextures_Clear();
    render::Shaders_Clear();
    ASSERT_NE(render::Shaders_Generation(), generation);
    ASSERT_TRUE(glIsProgram(program_.prog));
    glUseProgram(program_.prog);
    glUniform1f(glGetUniformLocation(program_.prog, "uLightGamma[17]"), 0.0f);
    glUniform1i(glGetUniformLocation(program_.prog, "uTexture"), 7);
    GLuint block = glGetUniformBlockIndex(program_.prog, "Bones");
    glUniformBlockBinding(program_.prog, block, 7);
    runtime_.ConfigureProgram(Bindings(runtime_));
    EXPECT_FLOAT_EQ(Scalar("uLightGamma[17]"), 0.75f);
    EXPECT_EQ(Sampler("uTexture"), render::kStudioBaseTextureUnit);
    GLint binding = -1;
    glGetActiveUniformBlockiv(program_.prog, block, GL_UNIFORM_BLOCK_BINDING, &binding);
    EXPECT_EQ(binding, render::kStudioBonesBinding);
    runtime_.InvalidatePrograms();
    EXPECT_TRUE(glIsProgram(program_.prog));
    const ncl_gl::Program* manager_program = runtime_.ProgramFor(nullptr);
    ASSERT_NE(manager_program, nullptr);
    runtime_.InvalidatePrograms();
    EXPECT_TRUE(glIsProgram(manager_program->prog));
}

studio_test::TextureUploadCallbacks studio_test::g_TextureUploadCallbacks;
std::function<void(tex::TextureLoadGuard)> studio_test::g_MaterialImageLoadCallback;

namespace tex
{
    bool LoadTextureFromFile(Texture&, const TexIdentifierStr&, const char*, bool, int)
    {
        ADD_FAILURE() << "Unexpected file-texture upload in the studio material fixture";
        return false;
    }

    bool LoadTexture(
        Texture& texture,
        const TexIdentifierStr& identifier,
        TextureFormat format,
        const uint8_t* data,
        int width,
        int height,
        bool mipmap,
        const uint8_t* palette,
        int filter,
        tex::TextureLoadGuard guard
    )
    {
        if (studio_test::g_TextureUploadCallbacks.load)
        {
            return studio_test::g_TextureUploadCallbacks.load(texture, identifier, format, data, width, height, mipmap, palette, filter);
        }

        ADD_FAILURE() << "Unexpected generic texture upload in the studio material fixture";
        return false;
    }

    void UnloadTexture(Texture& texture)
    {
        if (studio_test::g_TextureUploadCallbacks.release)
        {
            studio_test::g_TextureUploadCallbacks.release(texture);
            return;
        }

        ADD_FAILURE() << "Unexpected texture-manager release in the studio material fixture";
    }

    bool LoadTextureFromPath(
        Texture& texture,
        const TexIdentifierStr& identifier,
        const char* path,
        bool mipmap,
        int,
        TextureLoadGuard guard
    )
    {
        if (std::string_view(path) != kRuntimeImagePath)
        {
            return false;
        }

        if (studio_test::g_MaterialImageLoadCallback)
        {
            studio_test::g_MaterialImageLoadCallback(guard);
        }
        if (!guard.is_current())
        {
            return false;
        }
        ++g_RuntimeImageLoads;
        size_t before = g_ScratchAllocator.GetOffset();

        {
            ScratchArena arena(g_ScratchAllocator);
            unsigned char* pixels = arena.AllocateArray<unsigned char>(4096);
            std::fill_n(pixels, 4096, static_cast<unsigned char>(0xa5));
        }

        g_RuntimeImageScratchRestored = g_ScratchAllocator.GetOffset() == before;
        GLuint image = 0;
        glGenTextures(1, &image);
        GL_Bind(static_cast<int>(image));
        *p_currenttexture = -1;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, kRuntimeImagePixel.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        texture.identifier = identifier;
        texture.texnum = static_cast<int>(image);
        texture.width = texture.height = 1;
        texture.mipmap = mipmap;
        return true;
    }
} // namespace tex

TEST_F(StudioMaterialRuntimeGlTest, AuthoringCloseDeletesFileImageOnceAndReloadsItsPixels)
{
    ASSERT_TRUE(render::Device_IsContextCurrent());
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    glActiveTexture(GL_TEXTURE0);
    GLuint image = render::MaterialTexture_Get(kRuntimeImagePath);
    ASSERT_NE(image, 0u);
    ASSERT_TRUE(glIsTexture(image));
    ASSERT_EQ(g_RuntimeImageLoads, 1u);
    LifecycleDeleteTrace trace;

    preview.CloseAuthoring();
    EXPECT_FALSE(glIsTexture(image));
    EXPECT_EQ(std::count(g_DeletedTextures.begin(), g_DeletedTextures.end(), image), 1);
    preview.CloseAuthoring();
    EXPECT_FALSE(glIsTexture(image));
    EXPECT_EQ(std::count(g_DeletedTextures.begin(), g_DeletedTextures.end(), image), 1);
    EXPECT_EQ(g_RuntimeImageLoads, 1u);

    glActiveTexture(GL_TEXTURE0);
    GLuint reloaded = render::MaterialTexture_Get(kRuntimeImagePath);
    ASSERT_NE(reloaded, 0u);
    ASSERT_TRUE(glIsTexture(reloaded));
    EXPECT_EQ(g_RuntimeImageLoads, 2u);
    glBindTexture(GL_TEXTURE_2D, reloaded);
    std::array<unsigned char, 4> pixel{};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, kRuntimeImagePixel);
    EXPECT_EQ(render::MaterialTexture_Get(kRuntimeImagePath), reloaded);
    EXPECT_EQ(g_RuntimeImageLoads, 2u);
}

TEST_F(StudioMaterialRuntimeGlTest, LiveReleaseDeletesProgramsTexturesAndSubmissionBonesOnce)
{
    const ncl_gl::Program* program = runtime_.ProgramFor(nullptr);
    ASSERT_NE(program, nullptr);
    GLuint program_name = program->prog;
    GLuint texture = render::MaterialTexture_Neutral();
    GLuint image = render::MaterialTexture_Get(kRuntimeImagePath);
    ASSERT_NE(texture, 0u);
    ASSERT_NE(image, 0u);
    render::StudioGlSubmission submission(
        render::GetResources().device,
        runtime_,
        {BindLifecycleTexture, nullptr},
        runtime_performance_.monitor,
        runtime_performance_.mapper,
        runtime_performance_.studio_depth
    );
    ASSERT_TRUE(submission.EnsureBones());
    submission.Begin();
    GLint bone_buffer = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, render::kStudioBonesBinding, &bone_buffer);
    submission.End();
    ASSERT_NE(bone_buffer, 0);
    ASSERT_TRUE(render::Device_IsContextCurrent());
    LifecycleDeleteTrace trace;
    runtime_.InvalidatePrograms();
    submission.Release();
    render::Shaders_Clear();
    render::MaterialTextures_Clear();
    EXPECT_FALSE(glIsBuffer(static_cast<GLuint>(bone_buffer)));
    EXPECT_FALSE(glIsProgram(program_name));
    EXPECT_FALSE(glIsTexture(texture));
    EXPECT_FALSE(glIsTexture(image));
    submission.Release();
    render::Shaders_Clear();
    render::MaterialTextures_Clear();
    EXPECT_EQ(std::count(g_DeletedBuffers.begin(), g_DeletedBuffers.end(), static_cast<GLuint>(bone_buffer)), 1);
    EXPECT_EQ(std::count(g_DeletedPrograms.begin(), g_DeletedPrograms.end(), program_name), 1);
    EXPECT_EQ(std::count(g_DeletedTextures.begin(), g_DeletedTextures.end(), texture), 1);
    EXPECT_EQ(std::count(g_DeletedTextures.begin(), g_DeletedTextures.end(), image), 1);
    ASSERT_TRUE(submission.EnsureBones());
    ASSERT_NE(runtime_.ProgramFor(nullptr), nullptr);
    EXPECT_TRUE(glIsTexture(render::MaterialTexture_Neutral()));
    submission.Release();
}

TEST_F(StudioMaterialRuntimeGlTest, LossForgetsNamesAndRecreatesLazyResourcesWithoutDeletingOldNames)
{
    const ncl_gl::Program* program = runtime_.ProgramFor(nullptr);
    ASSERT_NE(program, nullptr);
    GLuint old_program = program->prog;
    GLuint old_texture = render::MaterialTexture_Neutral();
    render::StudioGlSubmission submission(
        render::GetResources().device,
        runtime_,
        {BindLifecycleTexture, nullptr},
        runtime_performance_.monitor,
        runtime_performance_.mapper,
        runtime_performance_.studio_depth
    );
    ASSERT_TRUE(submission.EnsureBones());
    submission.Begin();
    GLint old_bones = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, render::kStudioBonesBinding, &old_bones);
    submission.End();
    uint32_t generation = render::Shaders_Generation();
    LifecycleDeleteTrace trace;
    render::RendererLifecycle lifecycle = preview_test::MakeLifecycle(render::GetRendererState());
    lifecycle.OnContextLost();
    EXPECT_FALSE(render::Device_IsContextCurrent());
    EXPECT_TRUE(glIsProgram(old_program));
    EXPECT_TRUE(glIsTexture(old_texture));
    EXPECT_TRUE(glIsBuffer(static_cast<GLuint>(old_bones)));
    EXPECT_NE(render::Shaders_Generation(), generation);
    EXPECT_TRUE(g_DeletedBuffers.empty());
    EXPECT_TRUE(g_DeletedTextures.empty());
    EXPECT_TRUE(g_DeletedPrograms.empty());
    lifecycle.OnContextCreated();
    ASSERT_TRUE(render::Device_IsContextCurrent());
    EXPECT_NE(render::context::Current(), nullptr);
    ASSERT_TRUE(submission.EnsureBones());
    submission.Begin();
    GLint new_bones = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, render::kStudioBonesBinding, &new_bones);
    submission.End();
    EXPECT_NE(new_bones, old_bones);
    ASSERT_NE(runtime_.ProgramFor(nullptr), nullptr);
    EXPECT_NE(render::MaterialTexture_Neutral(), old_texture);
    submission.Release();
    glDeleteProgram(old_program);
    glDeleteTextures(1, &old_texture);
    GLuint abandoned_bones = static_cast<GLuint>(old_bones);
    glDeleteBuffers(1, &abandoned_bones);
}

TEST_F(StudioMaterialRuntimeGlTest, NativeIdentityRejectsAReplacementAndProtectsItsObjects)
{
    ASSERT_NE(render::context::Current(), nullptr);
    ASSERT_TRUE(render::Device_IsContextCurrent());
    uint32_t generation = render::Device_Generation();
    render::Device_OnContextCreated();
    EXPECT_EQ(render::Device_Generation(), generation);
    ASSERT_NE(runtime_.ProgramFor(nullptr), nullptr);
    ASSERT_NE(render::MaterialTexture_Neutral(), 0u);
    render::StudioGlSubmission old_submission(
        render::GetResources().device,
        runtime_,
        {},
        runtime_performance_.monitor,
        runtime_performance_.mapper,
        runtime_performance_.studio_depth
    );
    ASSERT_TRUE(old_submission.EnsureBones());
    RadarGlContext replacement;
    std::string error;
    ASSERT_TRUE(replacement.Initialize(error)) << error;
    EXPECT_FALSE(render::Device_IsContextCurrent());
    EXPECT_EQ(render::context::Current(), replacement.native_context());
    ASSERT_TRUE(replacement.ClearCurrent());
    EXPECT_EQ(render::context::Current(), nullptr);
    EXPECT_FALSE(render::Device_IsContextCurrent());
    ASSERT_TRUE(replacement.MakeCurrent());
    GLuint sentinel_texture = 0;
    glGenTextures(1, &sentinel_texture);
    glBindTexture(GL_TEXTURE_2D, sentinel_texture);
    const uint8_t pixel[] = {17, 43, 79, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    GLuint sentinel_buffer = 0;
    glGenBuffers(1, &sentinel_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, sentinel_buffer);
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);
    LifecycleDeleteTrace trace;
    render::RendererLifecycle lifecycle = preview_test::MakeLifecycle(render::GetRendererState());
    lifecycle.ReleaseLiveContext();
    EXPECT_FALSE(render::Device_IsContextAlive());
    EXPECT_TRUE(g_DeletedBuffers.empty());
    EXPECT_TRUE(g_DeletedTextures.empty());
    EXPECT_TRUE(g_DeletedPrograms.empty());
    lifecycle.OnContextCreated();
    EXPECT_TRUE(render::Device_IsContextCurrent());
    EXPECT_EQ(render::context::Current(), replacement.native_context());
    EXPECT_NE(render::Device_Generation(), generation);
    EXPECT_TRUE(glIsTexture(sentinel_texture));
    EXPECT_TRUE(glIsBuffer(sentinel_buffer));
    EXPECT_TRUE(g_DeletedBuffers.empty());
    EXPECT_TRUE(g_DeletedTextures.empty());
    EXPECT_TRUE(g_DeletedPrograms.empty());
    glDeleteTextures(1, &sentinel_texture);
    glDeleteBuffers(1, &sentinel_buffer);
    lifecycle.ReleaseLiveContext();
}
TEST_F(StudioMaterialRuntimeGlTest, PerformanceRuntimeSelectsReleaseOrAbandonByItsOwnNativeIdentity)
{
    LifecycleRuntimeQueries queries;
    performance::PerformanceMonitor& monitor = runtime_performance_.monitor;
    nitroapi::EngineData engine;
    nitroapi::ClientData client;
    performance::PerformanceRuntime runtime(
        engine,
        client,
        nullptr,
        nullptr,
        monitor,
        runtime_performance_.mapper,
        runtime_performance_.clock,
        resources_.device,
        render::GetRendererState().lifecycle,

#ifdef _WIN32
        TextRender_Get(),
#else
        nullptr,
#endif
        8684
    );
    int window = 0;
    monitor.SetGpuBackend(&queries);
    monitor.SetMode(1);
    monitor.SetMainWindow(&window);
    monitor.OnContextCreated(render::context::Current());
    ncl_perf::PerfScopeToken_t old = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    EXPECT_NE(old, 0u);
    runtime.ReleaseContext();
    EXPECT_EQ(queries.deletions, 1u);
    uint32_t writes = queries.writes;
    monitor.EndGpu(old);
    EXPECT_EQ(queries.writes, writes);
    runtime.ReleaseContext();
    EXPECT_EQ(queries.deletions, 1u);
    int other_context = 0;
    monitor.OnContextCreated(&other_context);
    old = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    EXPECT_NE(old, 0u);
    runtime.ReleaseContext();
    runtime.AbandonContext();
    EXPECT_EQ(queries.deletions, 1u);
    writes = queries.writes;
    monitor.EndGpu(old);
    monitor.PollGpu();
    EXPECT_EQ(queries.writes, writes);
    runtime.Shutdown();
    monitor.SetGpuBackend(nullptr);
    monitor.SetMode(0);
    monitor.SetMainWindow(nullptr);
    monitor.Reset();
}

TEST_F(StudioMaterialRuntimeGlTest, MapperSetupSkipsReadsAfterTrackedDeviceLoss)
{
    nitroapi::HandlersChain<void> chain;
    performance_test::RuntimeHook<void> hook;
    MapperRuntimeSession session(runtime_performance_, resources_, render::GetRendererState().lifecycle);
    session.engine.R_SetupGL.SetChain(&chain);
    session.engine.R_SetupGL.SetHook(&hook);
    hook.original = [&] {
        resources_.device.OnContextDestroyed();
        session.runtime.AbandonContext();
    };
    session.runtime.Bind();
    MapperGlProbe probe;
    hook.Invoke();
    EXPECT_EQ(g_MapperMatrixReads, 0u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    session.runtime.Shutdown();
    session.engine.R_SetupGL.SetChain(nullptr);
}

TEST_F(StudioMaterialRuntimeGlTest, MapperSetupSkipsReadsAfterNativeContextReplacement)
{
    nitroapi::HandlersChain<void> chain;
    performance_test::RuntimeHook<void> hook;
    RadarGlContext replacement;
    MapperRuntimeSession session(runtime_performance_, resources_, render::GetRendererState().lifecycle);
    session.engine.R_SetupGL.SetChain(&chain);
    session.engine.R_SetupGL.SetHook(&hook);
    hook.original = [&] {
        std::string error;
        ASSERT_TRUE(replacement.Initialize(error)) << error;
        MapperGlProbe::Rebind();
    };
    session.runtime.Bind();
    MapperGlProbe probe;
    hook.Invoke();
    EXPECT_EQ(g_MapperMatrixReads, 0u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    session.runtime.Shutdown();
    session.engine.R_SetupGL.SetChain(nullptr);
}

TEST_F(StudioMaterialRuntimeGlTest, MapperOutlineSkipsLostDeviceWhilePhysicalContextRemainsValid)
{
    nitroapi::HandlersChain<void> view_chain;
    performance_test::RuntimeHook<void> view_hook;
    nitroapi::HandlersChain<void> setup_chain;
    performance_test::RuntimeHook<void> setup_hook;
    MapperRuntimeSession session(runtime_performance_, resources_, render::GetRendererState().lifecycle);
    session.engine.R_RenderView.SetChain(&view_chain);
    session.engine.R_RenderView.SetHook(&view_hook);
    session.engine.R_SetupGL.SetChain(&setup_chain);
    session.engine.R_SetupGL.SetHook(&setup_hook);
    setup_hook.original = [] {};
    view_hook.original = [&] {
        setup_hook.Invoke();
        EXPECT_EQ(g_MapperMatrixReads, 2u);
        resources_.device.OnContextDestroyed();
        session.runtime.AbandonContext();
    };
    session.runtime.Bind();
    MapperGlProbe probe;
    view_hook.Invoke();
    EXPECT_EQ(g_MapperOutlines, 0u);
    EXPECT_FALSE(runtime_performance_.mapper.get_snapshot().view_measured);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    session.runtime.Shutdown();
    session.engine.R_RenderView.SetChain(nullptr);
    session.engine.R_SetupGL.SetChain(nullptr);
}

TEST_F(StudioMaterialRuntimeGlTest, MapperOutlineSkipsNativeContextReplacement)
{
    nitroapi::HandlersChain<void> view_chain;
    performance_test::RuntimeHook<void> view_hook;
    nitroapi::HandlersChain<void> setup_chain;
    performance_test::RuntimeHook<void> setup_hook;
    RadarGlContext replacement;
    MapperRuntimeSession session(runtime_performance_, resources_, render::GetRendererState().lifecycle);
    session.engine.R_RenderView.SetChain(&view_chain);
    session.engine.R_RenderView.SetHook(&view_hook);
    session.engine.R_SetupGL.SetChain(&setup_chain);
    session.engine.R_SetupGL.SetHook(&setup_hook);
    setup_hook.original = [] {};
    view_hook.original = [&] {
        setup_hook.Invoke();
        EXPECT_EQ(g_MapperMatrixReads, 2u);
        std::string error;
        ASSERT_TRUE(replacement.Initialize(error)) << error;
        MapperGlProbe::Rebind();
    };
    session.runtime.Bind();
    MapperGlProbe probe;
    view_hook.Invoke();
    EXPECT_EQ(g_MapperOutlines, 0u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    session.runtime.Shutdown();
    session.engine.R_RenderView.SetChain(nullptr);
    session.engine.R_SetupGL.SetChain(nullptr);
}

TEST_F(StudioMaterialRuntimeGlTest, MapperViewDrawsThePreparedOutlineInItsUnchangedCurrentContext)
{
    nitroapi::HandlersChain<void> view_chain;
    performance_test::RuntimeHook<void> view_hook;
    nitroapi::HandlersChain<void> setup_chain;
    performance_test::RuntimeHook<void> setup_hook;
    MapperRuntimeSession session(runtime_performance_, resources_, render::GetRendererState().lifecycle);
    session.engine.R_RenderView.SetChain(&view_chain);
    session.engine.R_RenderView.SetHook(&view_hook);
    session.engine.R_SetupGL.SetChain(&setup_chain);
    session.engine.R_SetupGL.SetHook(&setup_hook);
    setup_hook.original = [] {};
    view_hook.original = [&] {
        setup_hook.Invoke();
    };
    session.runtime.Bind();
    MapperGlProbe probe;
    view_hook.Invoke();
    EXPECT_EQ(g_MapperMatrixReads, 2u);
    EXPECT_EQ(g_MapperOutlines, 1u);
    EXPECT_TRUE(runtime_performance_.mapper.get_snapshot().view_measured);
    EXPECT_TRUE(runtime_performance_.mapper.get_snapshot().target_hit);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    session.runtime.Shutdown();
    session.engine.R_RenderView.SetChain(nullptr);
    session.engine.R_SetupGL.SetChain(nullptr);
}

TEST_F(StudioMaterialRuntimeGlTest, LifecycleCannotAdoptAfterARealLossCallbackBeginsShutdown)
{
    uint32_t generation = render::Device_Generation();
    RadarGlContext replacement;
    std::string error;
    ASSERT_TRUE(replacement.Initialize(error)) << error;
    render::RendererLifecycle lifecycle = preview_test::MakeLifecycle(render::GetRendererState());
    class ShutdownResource : public render::IDeviceResource
    {
    private:
        render::RendererLifecycle& lifecycle_;

    public:
        bool called{};
        explicit ShutdownResource(render::RendererLifecycle& lifecycle) :
            lifecycle_(lifecycle)
        {
            render::Device_Register(this);
        }
        ~ShutdownResource() override
        {
            render::Device_Unregister(this);
        }
        void OnDeviceLost() override
        {
            called = true;
            EXPECT_FALSE(render::Device_IsContextAlive());
            lifecycle_.BeginShutdown();
        }
    } resource(lifecycle);
    lifecycle.OnContextCreated();
    EXPECT_TRUE(resource.called);
    EXPECT_FALSE(render::Device_IsContextAlive());
    EXPECT_EQ(render::Device_Generation(), generation);
    EXPECT_EQ(render::context::Current(), replacement.native_context());
}

TEST_F(StudioMaterialRuntimeGlTest, SameNativeContextRetriesAfterEntryPointLoadFailure)
{
    RadarGlContext retry_context;
    std::string error;
    ASSERT_TRUE(retry_context.Initialize(error)) << error;
    render::Device_OnContextCreated();
    ASSERT_TRUE(render::Device_IsContextCurrent());
    uint32_t generation = render::Device_Generation();
    ASSERT_TRUE(retry_context.ClearCurrent());
    EXPECT_FALSE(ncl_gl::Reload());
    ASSERT_TRUE(retry_context.MakeCurrent());
    EXPECT_FALSE(render::Device_IsContextCurrent());
    render::Device_OnContextCreated();
    EXPECT_TRUE(render::Device_IsContextCurrent());
    EXPECT_EQ(render::Device_Generation(), generation);
    render::Device_OnContextDestroyed();
}

TEST_F(StudioMaterialRuntimeGlTest, GpuBuffersKeepInjectedDeviceLossAndReleaseIndependent)
{
    render::RenderDevice first;
    render::RenderDevice second;
    first.OnContextCreated();
    second.OnContextCreated();
    const float data[] = {1, 2, 3, 4};
    render::GpuBuffer first_buffer(first);
    render::GpuBuffer second_buffer(second);
    ASSERT_TRUE(first_buffer.Create(GL_ARRAY_BUFFER, data, sizeof(data)));
    ASSERT_TRUE(second_buffer.Create(GL_ARRAY_BUFFER, data, sizeof(data)));
    const GLuint first_name = first_buffer.get_name();
    const GLuint second_name = second_buffer.get_name();
    LifecycleDeleteTrace trace;

    first.OnContextDestroyed();
    EXPECT_EQ(first_buffer.get_name(), 0u);
    EXPECT_EQ(second_buffer.get_name(), second_name);
    EXPECT_TRUE(g_DeletedBuffers.empty());
    EXPECT_FALSE(first_buffer.Update(data, sizeof(data)));
    EXPECT_TRUE(second_buffer.Update(data, sizeof(data)));
    second_buffer.Destroy();
    ASSERT_EQ(g_DeletedBuffers.size(), 1u);
    EXPECT_EQ(g_DeletedBuffers.front(), second_name);

    glDeleteBuffers(1, &first_name);
    second.OnContextDestroyed();
}

TEST_F(StudioMaterialRuntimeGlTest, GpuBufferDestructionOnlyForgetsAndUnregisters)
{
    render::RenderDevice device;
    device.OnContextCreated();
    GLuint name = 0;
    LifecycleDeleteTrace trace;
    {
        render::GpuBuffer buffer(device);
        const float data[] = {1, 2, 3, 4};
        ASSERT_TRUE(buffer.Create(GL_ARRAY_BUFFER, data, sizeof(data)));
        name = buffer.get_name();
    }

    EXPECT_TRUE(g_DeletedBuffers.empty());
    EXPECT_EQ(glIsBuffer(name), GL_TRUE);
    device.OnContextDestroyed();
    EXPECT_TRUE(g_DeletedBuffers.empty());
    glDeleteBuffers(1, &name);
}

TEST_F(StudioMaterialRuntimeGlTest, ExternalPreparationKeepsPublishedMaterialBindingsUnchanged)
{
    StudioProgramSourceFixture programs;
    StudioMaterialSourceFixture materials;
    programs.program = &program_;
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1, "program": "fixture", "parameters": {
            "uWear": {"type": "scalar", "default": 0.25},
            "uWearMap": {"type": "texture"}
        }
    })");
    ASSERT_FALSE(manifest.has_error());
    materials.manifests.emplace("fixture", *manifest);
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(programs.manager, materials.catalog, materials.cache, runtime_performance.monitor);
    ncl_material::Material material;
    material.program = "fixture";
    const render::StudioMaterialBindings* published = nullptr;
    const ncl_gl::Program* owned = runtime.ProgramFor(&material, &published);
    ASSERT_NE(owned, nullptr);
    ASSERT_NE(published, nullptr);
    const render::StudioMaterialBindings snapshot = *published;
    ncl_gl::Program external;
    external.prog = ncl_gl::LinkProgram(kRuntimeVertex, kRuntimeFragment, nullptr, 0);
    ASSERT_NE(external.prog, 0u);
    ASSERT_NE(external.prog, owned->prog);
    render::StudioMaterialBindings separate;
    runtime.PrepareBindings(separate, external, &material);
    EXPECT_EQ(separate.program, &external);
    EXPECT_EQ(published->program, owned);
    EXPECT_EQ(published->manifest, snapshot.manifest);
    EXPECT_EQ(published->parameters, snapshot.parameters);
    EXPECT_EQ(published->generation, snapshot.generation);
    EXPECT_EQ(published->bones, snapshot.bones);
    EXPECT_EQ(published->texture, snapshot.texture);
    EXPECT_EQ(published->light_gamma, snapshot.light_gamma);
    render::StudioMaterialState state;
    runtime.ConfigureProgram(*published);
    EXPECT_EQ(UploadPreparedMaterial(runtime, *published, material, state), 0);
    EXPECT_FLOAT_EQ(Scalar("uWear"), 0.25f);
    glDeleteProgram(external.prog);
}
