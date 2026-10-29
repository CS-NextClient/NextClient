#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "RadarGlContext.h"
#include "StudioModelTestSupport.h"
#include "engine.h"
#include "PreviewTestOwners.h"
#include "StudioMaterialProviders.h"
#include "RendererOwnerTestSupport.h"
#include "common/model.h"
#include "graphics/gl_local.h"
#include "graphics/preview/preview_model.h"
#include "graphics/preview/ScenePreview.h"
#include "graphics/render/material/material_textures.h"
#include "graphics/render/render_device.h"
#include "graphics/render/shader_manager.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/model/StudioGeometry.h"
#include "graphics/render/model/studio_model_cache.h"
#include "graphics/render/StudioPreviewContext.h"
#include "graphics/render/RendererState.h"

namespace
{
    struct GeometryBlock
    {
        goldsrc::studiohdr_t header{};
        goldsrc::mstudiobodyparts_t body{};
        goldsrc::mstudiomodel_t submodel{};
        goldsrc::mstudiomesh_t mesh{};
        goldsrc::mstudioseqdesc_t sequence{};
        goldsrc::vec3_t positions[3] = {{-.5f, -.5f, 0}, {0, .5f, 0}, {.5f, -.5f, 0}};
        goldsrc::vec3_t normals[1] = {{0, 0, 1}};
        uint8_t bones[3]{};
        short commands[14] = {3, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0};
        goldsrc::mstudiotexture_t texture{};
        short skin{};

        GeometryBlock()
        {
            header.version = 10;
            header.length = sizeof(*this);
            header.texturedataindex = sizeof(*this);
            header.numbones = 1;
            header.numseq = 1;
            header.seqindex = offsetof(GeometryBlock, sequence);
            header.numbodyparts = 1;
            header.bodypartindex = offsetof(GeometryBlock, body);
            header.numtextures = 1;
            header.textureindex = offsetof(GeometryBlock, texture);
            header.numskinref = 1;
            header.numskinfamilies = 1;
            header.skinindex = offsetof(GeometryBlock, skin);
            body.base = 1;
            body.nummodels = 1;
            body.modelindex = offsetof(GeometryBlock, submodel);
            std::strcpy(submodel.name, "body");
            submodel.nummesh = 1;
            submodel.meshindex = offsetof(GeometryBlock, mesh);
            submodel.numverts = 3;
            submodel.vertindex = offsetof(GeometryBlock, positions);
            submodel.vertinfoindex = offsetof(GeometryBlock, bones);
            submodel.numnorms = 1;
            submodel.normindex = offsetof(GeometryBlock, normals);
            submodel.norminfoindex = offsetof(GeometryBlock, bones);
            mesh.numtris = 1;
            mesh.triindex = offsetof(GeometryBlock, commands);
            std::strcpy(texture.name, "base.bmp");
            texture.width = texture.height = 1;
        }
    };

    struct GeometryNames
    {
        GLuint vao{};
        GLuint vertices{};
        GLuint indices{};
    };

    GeometryNames ReadNames(const render::StudioGpuModel& model)
    {
        GLint previous = 0;
        GLint vertices = 0;
        GLint indices = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous);
        glBindVertexArray(model.vao);
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vertices);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &indices);
        glBindVertexArray(static_cast<GLuint>(previous));
        return {model.vao, static_cast<GLuint>(vertices), static_cast<GLuint>(indices)};
    }

    render::StudioDrawPacket PickPacket(const GeometryNames& names, GLuint texture)
    {
        render::StudioDrawPacket packet{};
        packet.vao = names.vao;
        packet.bone_count = 1;
        packet.bones[0] = packet.bones[5] = packet.bones[10] = 1;
        packet.modelview = packet.projection = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        packet.raster.depth_range[0] = 0;
        packet.raster.depth_range[1] = 1;
        packet.blend = 1;
        render::StudioQueuedMesh mesh{};
        mesh.texture = texture;
        mesh.index_count = 3;
        mesh.texel_scale[0] = mesh.texel_scale[1] = 1;
        packet.meshes.push_back(mesh);
        return packet;
    }

    PFNGLGENFRAMEBUFFERSPROC g_GenFramebuffers{};
    PFNGLGENBUFFERSPROC g_GenBuffers{};
    PFNGLGENVERTEXARRAYSPROC g_GenArrays{};
    std::vector<GLuint> g_Framebuffers;
    std::vector<GLuint> g_Buffers;
    std::vector<GLuint> g_Arrays;

    void APIENTRY RecordFramebuffers(GLsizei count, GLuint* names)
    {
        g_GenFramebuffers(count, names);
        g_Framebuffers.insert(g_Framebuffers.end(), names, names + count);
    }

    void APIENTRY RecordBuffers(GLsizei count, GLuint* names)
    {
        g_GenBuffers(count, names);
        g_Buffers.insert(g_Buffers.end(), names, names + count);
    }

    void APIENTRY RecordArrays(GLsizei count, GLuint* names)
    {
        g_GenArrays(count, names);
        g_Arrays.insert(g_Arrays.end(), names, names + count);
    }

    class AllocationTrace
    {
    public:
        AllocationTrace()
        {
            g_Framebuffers.clear();
            g_Buffers.clear();
            g_Arrays.clear();
            g_GenFramebuffers = glad_glGenFramebuffers;
            g_GenBuffers = glad_glGenBuffers;
            g_GenArrays = glad_glGenVertexArrays;
            glad_glGenFramebuffers = RecordFramebuffers;
            glad_glGenBuffers = RecordBuffers;
            glad_glGenVertexArrays = RecordArrays;
        }

        ~AllocationTrace()
        {
            glad_glGenFramebuffers = g_GenFramebuffers;
            glad_glGenBuffers = g_GenBuffers;
            glad_glGenVertexArrays = g_GenArrays;
        }
    };

    int g_DeleteCalls{};

    void APIENTRY CountDeletes(GLsizei, const GLuint*)
    {
        g_DeleteCalls++;
    }

    class DeleteTrace
    {
    private:
        PFNGLDELETEBUFFERSPROC buffers_ = glad_glDeleteBuffers;
        PFNGLDELETEVERTEXARRAYSPROC arrays_ = glad_glDeleteVertexArrays;
        PFNGLDELETETEXTURESPROC textures_ = glad_glDeleteTextures;
        PFNGLDELETEFRAMEBUFFERSPROC framebuffers_ = glad_glDeleteFramebuffers;
        PFNGLDELETERENDERBUFFERSPROC renderbuffers_ = glad_glDeleteRenderbuffers;

    public:
        DeleteTrace()
        {
            g_DeleteCalls = 0;
            glad_glDeleteBuffers = CountDeletes;
            glad_glDeleteVertexArrays = CountDeletes;
            glad_glDeleteTextures = CountDeletes;
            glad_glDeleteFramebuffers = CountDeletes;
            glad_glDeleteRenderbuffers = CountDeletes;
        }

        ~DeleteTrace()
        {
            glad_glDeleteBuffers = buffers_;
            glad_glDeleteVertexArrays = arrays_;
            glad_glDeleteTextures = textures_;
            glad_glDeleteFramebuffers = framebuffers_;
            glad_glDeleteRenderbuffers = renderbuffers_;
        }
    };

    std::array<GLuint, 2> ReadRenderbuffers(GLuint framebuffer)
    {
        GLint previous = 0;
        GLint color = 0;
        GLint depth = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &color);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depth);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previous));
        return {static_cast<GLuint>(color), static_cast<GLuint>(depth)};
    }

    int ScreenInfo(goldsrc::SCREENINFO* info)
    {
        info->iWidth = info->iHeight = 32;
        return 1;
    }

    constexpr char kSceneText[] = "v 8 -1 -1\nv 8 0 1\nv 8 1 -1\nf 1 2 3\n";
    int g_SceneReads{};
    int g_SceneDraws{};

    uint8_t* LoadSceneFile(const char*, int, int* length)
    {
        g_SceneReads++;
        *length = sizeof(kSceneText) - 1;
        return reinterpret_cast<uint8_t*>(const_cast<char*>(kSceneText));
    }

    void FreeSceneFile(void*) {}

    const render::StudioDrawPacket* g_PreviewPacket{};
    int g_ModelResolutions{};

    goldsrc::model_t* ResolvePreviewModel(const char* path, goldsrc::qboolean crash, goldsrc::qboolean track_crc)
    {
        g_ModelResolutions++;
        return Mod_ForName(path, crash, track_crc);
    }

    int CapturePreviewPacket(int)
    {
        render::StudioDrawPacket packet = *g_PreviewPacket;
        PrepareCoveragePacket(render::GetRendererState().coverage, packet);
        render::GetRendererState().studio.CapturePreviewPacket(*p_currententity, packet);
        return 1;
    }

    char* Print(const char*, ...)
    {
        return nullptr;
    }

    class ConsoleHook : public studio_test::Hook<char*, const char*>
    {
    public:
        uint32_t GetOriginalFunctionAddress() const override
        {
            return reinterpret_cast<uint32_t>(Print);
        }
    };

    class PreviewEngine : public studio_test::ModelEngine
    {
    private:
        studio_test::Chain<double> clock_chain_;
        studio_test::Chain<char*, const char*> console_chain_;
        studio_test::Hook<double> clock_;
        ConsoleHook console_;

    public:
        PreviewEngine()
        {
            clock_.callback = []() { return 0.0; };
            nitroapi::EngineData* data = GetEngineData();
            data->Sys_FloatTime.SetChain(&clock_chain_);
            data->Sys_FloatTime.SetHook(&clock_);
            data->Con_Printf.SetChain(&console_chain_);
            data->Con_Printf.SetHook(&console_);
        }
    };

    class PreviewBackendScope
    {
    private:
        nitroapi::NitroApiInterface* saved_api_ = g_NitroApi;
        goldsrc::r_studio_interface_t* saved_studio_ = pStudioAPI;
        goldsrc::cl_entity_t** saved_entity_ = p_currententity;
        float* saved_blend_ = p_r_blend;
        PreviewEngine backend_;
        goldsrc::r_studio_interface_t studio_{};
        goldsrc::cl_entity_t* entity_{};
        float blend_ = 1;

    public:
        PreviewBackendScope(const goldsrc::model_t& model, goldsrc::studiohdr_t* header, const render::StudioDrawPacket& packet)
        {
            studio_.StudioDrawModel = CapturePreviewPacket;
            g_NitroApi = &backend_;
            pStudioAPI = &studio_;
            p_currententity = &entity_;
            p_r_blend = &blend_;
            mod_known[0] = model;
            mod_known[0].type = goldsrc::mod_studio;
            mod_known[0].cache.data = header;
            mod_numknown = 1;
            g_PreviewPacket = &packet;
            preview_test::BindEngine(render::GetRendererState());
        }

        ~PreviewBackendScope()
        {
            g_PreviewPacket = nullptr;
            mod_known[0] = {};
            mod_numknown = 0;
            g_NitroApi = saved_api_;
            pStudioAPI = saved_studio_;
            p_currententity = saved_entity_;
            p_r_blend = saved_blend_;
            preview_test::BindEngine(render::GetRendererState());
        }
    };

    void APIENTRY RecordSceneDraw(GLenum target, GLenum name, GLfloat value)
    {
        g_SceneDraws++;
        glad_glTexEnvf(target, name, value);
    }

    class SceneAuthoringCloseGlTest : public testing::Test
    {
    private:
        PreviewEngine backend_;
        nitroapi::NitroApiInterface* saved_api_{};
        goldsrc::cl_enginefunc_t saved_engine_{};
        goldsrc::cvar_t* saved_developer_{};
        goldsrc::cvar_t developer_{};
        GLuint framebuffer_{};
        GLuint images_[2]{};

    protected:
        RadarGlContext context_;
        GeometryBlock block_;
        goldsrc::model_t model_{};

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            ASSERT_TRUE(GLAD_GL_VERSION_3_3);
            saved_api_ = g_NitroApi;
            g_NitroApi = &backend_;
            saved_engine_ = gEngfuncs;
            saved_developer_ = developer;
            developer = &developer_;
            gEngfuncs.pfnGetScreenInfo = ScreenInfo;
            gEngfuncs.COM_LoadFile = LoadSceneFile;
            gEngfuncs.COM_FreeFile = FreeSceneFile;
            preview_test::BindEngine(render::GetRendererState());
            qglGetIntegerv = glad_glGetIntegerv;
            qglMatrixMode = glad_glMatrixMode;
            qglPushMatrix = glad_glPushMatrix;
            qglPopMatrix = glad_glPopMatrix;
            qglIsEnabled = glad_glIsEnabled;
            qglGetBooleanv = glad_glGetBooleanv;
            qglScissor = glad_glScissor;
            qglViewport = glad_glViewport;
            qglEnable = glad_glEnable;
            qglDisable = glad_glDisable;
            qglDepthMask = glad_glDepthMask;
            qglDepthFunc = glad_glDepthFunc;
            qglClearDepth = glad_glClearDepth;
            qglColorMask = glad_glColorMask;
            qglClearColor = glad_glClearColor;
            qglClear = glad_glClear;
            qglColor4f = glad_glColor4f;
            qglCullFace = glad_glCullFace;
            qglFrontFace = glad_glFrontFace;
            qglLoadIdentity = glad_glLoadIdentity;
            qglFrustum = glad_glFrustum;
            qglRotatef = glad_glRotatef;
            qglTranslatef = glad_glTranslatef;
            qglTexEnvf = RecordSceneDraw;
            g_SceneReads = g_SceneDraws = 0;
            render::Device_OnContextCreated();
            render::Studio_DiscardPending();
            render::Models_Clear();
            std::strcpy(model_.name, "models/close_test.mdl");

            glGenFramebuffers(1, &framebuffer_);
            glGenRenderbuffers(2, images_);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            for (int index = 0; index < 2; index++)
            {
                glBindRenderbuffer(GL_RENDERBUFFER, images_[index]);
                glRenderbufferStorage(GL_RENDERBUFFER, index ? GL_DEPTH_COMPONENT24 : GL_RGBA8, 32, 32);
                glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER, index ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, images_[index]
                );
            }
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            glViewport(0, 0, 32, 32);
        }

        void TearDown() override
        {
            ASSERT_TRUE(context_.MakeCurrent());
            render::Studio_DiscardPending();
            render::Models_Clear();
            render::MaterialTextures_Clear();
            render::Shaders_Clear();
            render::Device_OnContextDestroyed();
            glDeleteFramebuffers(1, &framebuffer_);
            glDeleteRenderbuffers(2, images_);
            gEngfuncs = saved_engine_;
            g_NitroApi = saved_api_;
            developer = saved_developer_;
        }

        const render::StudioGpuModel* PrepareGeometry()
        {
            if (!render::Models_PrepareMetadata(&model_, &block_.header, &block_.header, &model_))
            {
                return nullptr;
            }

            return render::Models_PrepareGeometry(&model_, &block_.header);
        }
    };
} // namespace

TEST_F(SceneAuthoringCloseGlTest, SuppliedSceneOwnersCloseOnlyTheirGeometryAndImages)
{
    preview_test::Owners first;
    preview_test::Owners second;
    first.resources.device.OnContextCreated();
    second.resources.device.OnContextCreated();
    ASSERT_TRUE(first.state.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    ASSERT_TRUE(second.state.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
    const render::StudioGpuModel* first_model = first.state.models.PrepareGeometry(&model_, &block_.header);
    const render::StudioGpuModel* second_model = second.state.models.PrepareGeometry(&model_, &block_.header);
    ASSERT_NE(first_model, nullptr);
    ASSERT_NE(second_model, nullptr);
    GeometryNames first_names = ReadNames(*first_model);
    GeometryNames second_names = ReadNames(*second_model);
    GLuint first_image = first.resources.textures.Neutral();
    GLuint second_image = second.resources.textures.Neutral();
    ASSERT_NE(first_image, 0u);
    ASSERT_NE(second_image, 0u);
    ASSERT_NE(first_image, second_image);

    first.state.preview.CloseAuthoring();

    EXPECT_FALSE(glIsVertexArray(first_names.vao));
    EXPECT_FALSE(glIsBuffer(first_names.vertices));
    EXPECT_FALSE(glIsBuffer(first_names.indices));
    EXPECT_FALSE(glIsTexture(first_image));
    EXPECT_TRUE(glIsVertexArray(second_names.vao));
    EXPECT_TRUE(glIsBuffer(second_names.vertices));
    EXPECT_TRUE(glIsBuffer(second_names.indices));
    EXPECT_TRUE(glIsTexture(second_image));
    EXPECT_NE(first.state.models.Metadata(model_.name), nullptr);
    EXPECT_NE(second.state.models.Metadata(model_.name), nullptr);
    second.state.preview.CloseAuthoring();
    first.resources.device.OnContextDestroyed();
    second.resources.device.OnContextDestroyed();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(SceneAuthoringCloseGlTest, ModelRendererUsesOneResolutionPerDrawAndObservesReloadedHeaders)
{
    render::StudioDrawPacket packet{};
    PreviewBackendScope engine(model_, &block_.header, packet);
    preview_test::Owners owner;
    preview_test::BindEngine(owner.state);
    owner.state.preview_engine.load_model = ResolvePreviewModel;
    PreviewModel model{};
    model.path = model_.name;
    PreviewDrawParams params{};
    params.camera.fov = 90;
    g_ModelResolutions = 0;

    EXPECT_EQ(owner.state.preview_renderer.Draw(model, params, 32, 32), 1);
    EXPECT_EQ(g_ModelResolutions, 1);

    GeometryBlock replacement;
    replacement.sequence.numframes = 9;
    mod_known[0].cache.data = &replacement.header;
    EXPECT_EQ(owner.state.preview_renderer.Draw(model, params, 32, 32), 1);
    EXPECT_EQ(g_ModelResolutions, 2);
    const ncl_studio::StudioMetadata* metadata = owner.state.models.Metadata(model_.name);
    ASSERT_NE(metadata, nullptr);
    ASSERT_EQ(metadata->sequences.size(), 1u);
    EXPECT_EQ(metadata->sequences[0].frame_count, 9);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(SceneAuthoringCloseGlTest, IndependentWavefrontOwnersKeepDistinctNativeTextureNamesAcrossReload)
{
    render::RenderDevice first_device;
    render::RenderDevice second_device;
    render::preview::PreviewAssetBackend assets(LoadSceneFile, FreeSceneFile);
    render::preview::WavefrontScene first(first_device, assets);
    render::preview::WavefrontScene second(second_device, assets);
    ASSERT_TRUE(first.Load("preview.obj", nullptr, 0, 1));
    ASSERT_TRUE(second.Load("preview.obj", nullptr, 0, 1));
    std::string first_name = render::preview::WavefrontSceneTestAccess::Identifier(first);
    std::string second_name = render::preview::WavefrontSceneTestAccess::Identifier(second);
    EXPECT_NE(first_name, second_name);
    ASSERT_TRUE(first.Reload());
    EXPECT_EQ(render::preview::WavefrontSceneTestAccess::Identifier(first), first_name);
    EXPECT_EQ(render::preview::WavefrontSceneTestAccess::Identifier(second), second_name);
}

TEST_F(SceneAuthoringCloseGlTest, CloseDeletesTargetGeometryAndImagesAndRecreatesThemLazily)
{
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    const render::StudioGpuModel* geometry = PrepareGeometry();
    ASSERT_NE(geometry, nullptr);
    GeometryNames names = ReadNames(*geometry);
    const ncl_studio::StudioMetadata* metadata = render::Models_Metadata(model_.name);
    ASSERT_EQ(render::Models_ModelCount(), 1);
    const char* model_name = render::Models_ModelName(0);
    GLuint neutral = render::MaterialTexture_Neutral();
    ASSERT_NE(neutral, 0u);
    const render::ProgramDesc descriptor{"preview_outline.vert", "preview_outline.frag", nullptr, nullptr, 0, nullptr, 0};
    const ncl_gl::Program* program = render::Shaders_Get(descriptor);
    ASSERT_NE(program, nullptr);
    const GLuint program_name = program->prog;
    const uint32_t shader_generation = render::Shaders_Generation();
    glUseProgram(program_name);
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.clear = true;
    AllocationTrace trace;
    ASSERT_EQ(preview.Draw(params, {0, 0, 16, 16}, {0, 0, 16, 16}), PreviewDrawStatus::Ready);
    ASSERT_EQ(g_Framebuffers.size(), 1u);
    const GLuint target = g_Framebuffers[0];
    const std::array<GLuint, 2> target_images = ReadRenderbuffers(target);

    preview.CloseAuthoring();

    EXPECT_FALSE(glIsVertexArray(names.vao));
    EXPECT_FALSE(glIsBuffer(names.vertices));
    EXPECT_FALSE(glIsBuffer(names.indices));
    EXPECT_FALSE(glIsTexture(neutral));
    EXPECT_FALSE(glIsFramebuffer(target));
    EXPECT_FALSE(glIsRenderbuffer(target_images[0]));
    EXPECT_FALSE(glIsRenderbuffer(target_images[1]));
    EXPECT_EQ(render::Models_Metadata(model_.name), metadata);
    EXPECT_EQ(render::Models_ModelName(0), model_name);
    EXPECT_EQ(render::Models_TextureCount(model_.name), 1);
    EXPECT_EQ(render::Models_BodyCount(model_.name), 1);
    EXPECT_TRUE(glIsProgram(program_name));
    EXPECT_EQ(render::Shaders_Generation(), shader_generation);
    GLint binding = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &binding);
    EXPECT_EQ(static_cast<GLuint>(binding), program_name);
    preview.CloseAuthoring();
    ASSERT_NE(PrepareGeometry(), nullptr);
    EXPECT_TRUE(glIsTexture(render::MaterialTexture_Neutral()));
    ASSERT_EQ(preview.Draw(params, {0, 0, 16, 16}, {0, 0, 16, 16}), PreviewDrawStatus::Ready);
    EXPECT_EQ(g_Framebuffers.size(), 2u);
    EXPECT_TRUE(glIsFramebuffer(g_Framebuffers.back()));
    preview.CloseAuthoring();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(SceneAuthoringCloseGlTest, CloseDropsBindingsOfDeletedGeometryWithoutRestoringItsNames)
{
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    const render::StudioGpuModel* geometry = PrepareGeometry();
    ASSERT_NE(geometry, nullptr);
    const GeometryNames names = ReadNames(*geometry);
    const GLuint neutral = render::MaterialTexture_Neutral();
    glBindVertexArray(names.vao);
    glBindBuffer(GL_ARRAY_BUFFER, names.vertices);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, neutral);

    preview.CloseAuthoring();

    GLint binding = -1;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &binding);
    EXPECT_EQ(binding, 0);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
    EXPECT_EQ(binding, 0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    EXPECT_EQ(binding, 0);
    EXPECT_FALSE(glIsVertexArray(names.vao));
    EXPECT_FALSE(glIsBuffer(names.vertices));
    EXPECT_FALSE(glIsTexture(neutral));
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(SceneAuthoringCloseGlTest, CloseDeletesPickingResourcesAndPreservesModelAdmission)
{
    ASSERT_EQ(mod_numknown, 0);
    const render::StudioGpuModel* geometry = PrepareGeometry();
    ASSERT_NE(geometry, nullptr);
    const GeometryNames names = ReadNames(*geometry);
    const render::StudioDrawPacket packet = PickPacket(names, render::MaterialTexture_Neutral());
    PreviewBackendScope backend(model_, &block_.header, packet);
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    PreviewModel model{};
    model.path = model_.name;
    model.camera_space = true;
    model.fov = 90;
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.models = &model;
    params.model_count = 1;
    params.clear = true;
    params.interactive = true;
    params.cursor_x = params.cursor_y = 8;
    AllocationTrace trace;
    const PreviewRect view{0, 0, 16, 16};
    PreviewHit hit{};
    const int loaded_entries = PreviewModel_LoadedEntries();
    ASSERT_EQ(preview.Draw(params, view, view, &hit), PreviewDrawStatus::Ready);
    EXPECT_EQ(hit.model_index, 0);
    ASSERT_EQ(g_Framebuffers.size(), 2u);
    ASSERT_EQ(g_Buffers.size(), 1u);
    ASSERT_EQ(g_Arrays.size(), 1u);
    const GLuint picking = g_Framebuffers[1];
    const GLuint bones = g_Buffers[0];
    const GLuint vao = g_Arrays[0];
    const std::array<GLuint, 2> picking_images = ReadRenderbuffers(picking);

    preview.CloseAuthoring();

    EXPECT_FALSE(glIsFramebuffer(picking));
    EXPECT_FALSE(glIsTexture(picking_images[0]));
    EXPECT_FALSE(glIsRenderbuffer(picking_images[1]));
    EXPECT_FALSE(glIsBuffer(bones));
    EXPECT_FALSE(glIsVertexArray(vao));
    preview.CloseAuthoring();
    EXPECT_EQ(PreviewModel_LoadedEntries(), loaded_entries);
    params.models = nullptr;
    params.model_count = 0;
    ASSERT_EQ(preview.Draw(params, view, view, &hit), PreviewDrawStatus::Ready);
    EXPECT_EQ(hit.model_index, -1);
    EXPECT_EQ(g_Framebuffers.size(), 3u);
    preview.CloseAuthoring();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(SceneAuthoringCloseGlTest, ClosePreservesIndependentSceneHandleAndItsReload)
{
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    const char* wads[] = {nullptr};
    const PreviewSceneHandle scene = preview.LoadScene("corridor.obj", wads, 0);
    ASSERT_NE(scene, 0);
    ASSERT_EQ(g_SceneReads, 1);
    preview.CloseAuthoring();
    preview.CloseAuthoring();
    PreviewDrawParams params{};
    params.scene = scene;
    params.camera.fov = 90;
    params.clear = true;
    EXPECT_EQ(preview.Draw(params, {0, 0, 16, 16}, {0, 0, 16, 16}), PreviewDrawStatus::Ready);
    EXPECT_EQ(g_SceneDraws, 1);
    preview.ReloadAll();
    EXPECT_EQ(g_SceneReads, 2);
    preview.FreeScene(scene);
    EXPECT_EQ(preview.Draw(params, {0, 0, 16, 16}, {0, 0, 16, 16}), PreviewDrawStatus::Ready);
    EXPECT_EQ(g_SceneDraws, 1);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(SceneAuthoringCloseGlTest, CloseWithoutCurrentContextForgetsNamesWithoutDeleting)
{
    ASSERT_EQ(mod_numknown, 0);
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    const render::StudioGpuModel* geometry = PrepareGeometry();
    ASSERT_NE(geometry, nullptr);
    const GeometryNames names = ReadNames(*geometry);
    GLuint neutral = render::MaterialTexture_Neutral();
    const render::StudioDrawPacket packet = PickPacket(names, neutral);
    PreviewBackendScope backend(model_, &block_.header, packet);
    PreviewModel model{};
    model.path = model_.name;
    model.camera_space = true;
    model.fov = 90;
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.models = &model;
    params.model_count = 1;
    params.interactive = true;
    params.cursor_x = params.cursor_y = 8;
    params.clear = true;
    AllocationTrace trace;
    ASSERT_EQ(preview.Draw(params, {0, 0, 16, 16}, {0, 0, 16, 16}), PreviewDrawStatus::Ready);
    ASSERT_EQ(g_Framebuffers.size(), 2u);
    const GLuint target = g_Framebuffers[0];
    const std::array<GLuint, 2> target_images = ReadRenderbuffers(target);
    const GLuint picking = g_Framebuffers[1];
    const std::array<GLuint, 2> picking_images = ReadRenderbuffers(picking);
    ASSERT_EQ(g_Buffers.size(), 1u);
    ASSERT_EQ(g_Arrays.size(), 1u);
    const GLuint picking_bones = g_Buffers[0];
    const GLuint picking_vao = g_Arrays[0];
    ASSERT_TRUE(context_.ClearCurrent());

    {
        DeleteTrace trace;
        preview.CloseAuthoring();
        preview.CloseAuthoring();
        EXPECT_EQ(g_DeleteCalls, 0);
    }

    ASSERT_TRUE(context_.MakeCurrent());
    EXPECT_TRUE(glIsVertexArray(names.vao));
    EXPECT_TRUE(glIsBuffer(names.vertices));
    EXPECT_TRUE(glIsBuffer(names.indices));
    EXPECT_TRUE(glIsTexture(neutral));
    EXPECT_TRUE(glIsFramebuffer(target));
    EXPECT_TRUE(glIsRenderbuffer(target_images[0]));
    EXPECT_TRUE(glIsRenderbuffer(target_images[1]));
    EXPECT_TRUE(glIsFramebuffer(picking));
    EXPECT_TRUE(glIsTexture(picking_images[0]));
    EXPECT_TRUE(glIsRenderbuffer(picking_images[1]));
    EXPECT_TRUE(glIsBuffer(picking_bones));
    EXPECT_TRUE(glIsVertexArray(picking_vao));
    ASSERT_NE(PrepareGeometry(), nullptr);
    EXPECT_NE(render::MaterialTexture_Neutral(), neutral);
    params.models = nullptr;
    params.model_count = 0;
    ASSERT_EQ(preview.Draw(params, {0, 0, 16, 16}, {0, 0, 16, 16}), PreviewDrawStatus::Ready);
    EXPECT_EQ(g_Framebuffers.size(), 3u);
    EXPECT_NE(g_Framebuffers.back(), target);
    preview.CloseAuthoring();
    glDeleteVertexArrays(1, &names.vao);
    const GLuint buffers[] = {names.vertices, names.indices};
    glDeleteBuffers(2, buffers);
    glDeleteTextures(1, &neutral);
    glDeleteFramebuffers(1, &target);
    glDeleteRenderbuffers(2, target_images.data());
    glDeleteFramebuffers(1, &picking);
    glDeleteTextures(1, &picking_images[0]);
    glDeleteRenderbuffers(1, &picking_images[1]);
    glDeleteBuffers(1, &picking_bones);
    glDeleteVertexArrays(1, &picking_vao);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(SceneAuthoringCloseGlTest, CloseRejectsAReplacementContextAndPreservesItsMatchingNames)
{
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    const render::StudioGpuModel* geometry = PrepareGeometry();
    ASSERT_NE(geometry, nullptr);
    const GeometryNames names = ReadNames(*geometry);
    GLuint neutral = render::MaterialTexture_Neutral();
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.clear = true;
    GLuint target = 0;
    std::array<GLuint, 2> target_images{};

    {
        AllocationTrace trace;
        ASSERT_EQ(preview.Draw(params, {0, 0, 16, 16}, {0, 0, 16, 16}), PreviewDrawStatus::Ready);
        ASSERT_EQ(g_Framebuffers.size(), 1u);
        target = g_Framebuffers[0];
        target_images = ReadRenderbuffers(target);
    }

    {
        RadarGlContext replacement;
        std::string error;
        ASSERT_TRUE(replacement.Initialize(error)) << error;
        ASSERT_FALSE(render::Device_IsContextCurrent());
        glBindTexture(GL_TEXTURE_2D, neutral);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, names.vertices);
        glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, names.indices);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);
        std::vector<GLuint> framebuffers(target);
        glGenFramebuffers(static_cast<GLsizei>(framebuffers.size()), framebuffers.data());
        ASSERT_NE(std::find(framebuffers.begin(), framebuffers.end(), target), framebuffers.end());
        glBindFramebuffer(GL_FRAMEBUFFER, target);

        std::vector<GLuint> renderbuffers(std::max(target_images[0], target_images[1]));
        glGenRenderbuffers(static_cast<GLsizei>(renderbuffers.size()), renderbuffers.data());
        for (GLuint image : target_images)
        {
            ASSERT_NE(std::find(renderbuffers.begin(), renderbuffers.end(), image), renderbuffers.end());
            glBindRenderbuffer(GL_RENDERBUFFER, image);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
        }
        std::vector<GLuint> arrays(names.vao);
        glGenVertexArrays(static_cast<GLsizei>(arrays.size()), arrays.data());
        ASSERT_NE(std::find(arrays.begin(), arrays.end(), names.vao), arrays.end());
        glBindVertexArray(names.vao);

        ASSERT_TRUE(glIsTexture(neutral));
        ASSERT_TRUE(glIsBuffer(names.vertices));
        ASSERT_TRUE(glIsBuffer(names.indices));
        ASSERT_TRUE(glIsVertexArray(names.vao));
        ASSERT_TRUE(glIsFramebuffer(target));
        ASSERT_TRUE(glIsRenderbuffer(target_images[0]));
        ASSERT_TRUE(glIsRenderbuffer(target_images[1]));
        ASSERT_EQ(glGetError(), GLenum(GL_NO_ERROR));

        {
            DeleteTrace trace;
            preview.CloseAuthoring();
            preview.CloseAuthoring();
            EXPECT_EQ(g_DeleteCalls, 0);
        }

        EXPECT_TRUE(glIsTexture(neutral));
        EXPECT_TRUE(glIsBuffer(names.vertices));
        EXPECT_TRUE(glIsBuffer(names.indices));
        EXPECT_TRUE(glIsVertexArray(names.vao));
        EXPECT_TRUE(glIsFramebuffer(target));
        EXPECT_TRUE(glIsRenderbuffer(target_images[0]));
        EXPECT_TRUE(glIsRenderbuffer(target_images[1]));
        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    }

    ASSERT_TRUE(context_.MakeCurrent());
    EXPECT_TRUE(glIsVertexArray(names.vao));
    EXPECT_TRUE(glIsBuffer(names.vertices));
    EXPECT_TRUE(glIsBuffer(names.indices));
    EXPECT_TRUE(glIsTexture(neutral));
    EXPECT_TRUE(glIsFramebuffer(target));
    ASSERT_NE(PrepareGeometry(), nullptr);
    EXPECT_NE(render::MaterialTexture_Neutral(), neutral);
    preview.CloseAuthoring();
    glDeleteVertexArrays(1, &names.vao);
    const GLuint buffers[] = {names.vertices, names.indices};
    glDeleteBuffers(2, buffers);
    glDeleteTextures(1, &neutral);
    glDeleteFramebuffers(1, &target);
    glDeleteRenderbuffers(2, target_images.data());
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

namespace
{
    renderer_test::Owners* g_ContinuationOwner{};
    unsigned int g_ContinuationModels{};
    unsigned int g_ContinuationGlCalls{};
    bool g_AfterContinuationLoss{};
    decltype(glad_glMatrixMode) g_ContinuationMatrix{};
    decltype(glad_glBindFramebuffer) g_ContinuationFramebuffer{};
    decltype(qglFrontFace) g_ContinuationFrontFace{};

    void APIENTRY ObserveContinuationMatrix(GLenum mode)
    {
        if (g_AfterContinuationLoss)
        {
            ++g_ContinuationGlCalls;
        }
        g_ContinuationMatrix(mode);
    }
    void APIENTRY ObserveContinuationFramebuffer(GLenum target, GLuint framebuffer)
    {
        if (g_AfterContinuationLoss)
        {
            ++g_ContinuationGlCalls;
        }
        g_ContinuationFramebuffer(target, framebuffer);
    }
    void APIENTRY ObserveContinuationFrontFace(GLenum mode)
    {
        if (g_AfterContinuationLoss)
        {
            ++g_ContinuationGlCalls;
        }
        g_ContinuationFrontFace(mode);
    }
    int LoadImageDuringPreview(int)
    {
        ++g_ContinuationModels;
        render::StudioRenderer& renderer = g_ContinuationOwner->state.studio;
        tex::TextureLoadGuard guard;
        guard.preparation_epoch = &renderer.get_cancellation_source();
        guard.expected_preparation_epoch = renderer.get_cancellation_epoch();
        bool canceled = false;
        g_ContinuationOwner->resources.textures.Get("fixtures/continuation.png", guard, &canceled);
        EXPECT_TRUE(canceled);
        return 1;
    }
} // namespace

TEST_F(SceneAuthoringCloseGlTest, PreviewLossFromImagePreparationStopsNativeAndSceneContinuations)
{
    render::StudioDrawPacket packet;
    PreviewBackendScope backend(model_, &block_.header, packet);
    renderer_test::Owners owners;
    preview_test::BindEngine(owners.state);
    owners.state.lifecycle.OnContextCreated();
    g_ContinuationOwner = &owners;
    const auto previous_draw = owners.state.preview_engine.studio->StudioDrawModel;
    owners.state.preview_engine.studio->StudioDrawModel = LoadImageDuringPreview;
    g_ContinuationModels = g_ContinuationGlCalls = 0;
    g_AfterContinuationLoss = false;
    owners.backend.on_load = [&](const char*, tex::TextureLoadGuard guard) {
        const bool phase = render::StudioRendererTestAccess::QueuedCount(owners.state.studio) == 0;
        EXPECT_TRUE(phase);
        if (!phase)
        {
            return GLuint{0};
        }
        owners.state.lifecycle.OnContextLost();
        EXPECT_FALSE(guard.is_epoch_current());
        g_AfterContinuationLoss = true;
        return GLuint{0};
    };
    g_ContinuationMatrix = glad_glMatrixMode;
    g_ContinuationFramebuffer = glad_glBindFramebuffer;
    g_ContinuationFrontFace = qglFrontFace;
    glad_glMatrixMode = ObserveContinuationMatrix;
    glad_glBindFramebuffer = ObserveContinuationFramebuffer;
    qglFrontFace = ObserveContinuationFrontFace;
    PreviewModel models[2]{};
    models[0].path = models[1].path = model_.name;
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.camera.fov = 90;
    params.models = models;
    params.model_count = 2;
    params.interactive = true;
    const PreviewDrawStatus result = owners.state.preview.Draw(params, {0, 0, 32, 32}, {0, 0, 32, 32});
    g_AfterContinuationLoss = false;
    glad_glMatrixMode = g_ContinuationMatrix;
    glad_glBindFramebuffer = g_ContinuationFramebuffer;
    qglFrontFace = g_ContinuationFrontFace;
    owners.state.preview_engine.studio->StudioDrawModel = previous_draw;
    g_ContinuationOwner = nullptr;
    EXPECT_EQ(result, PreviewDrawStatus::ModelDrawFailed);
    EXPECT_EQ(g_ContinuationModels, 1u);
    EXPECT_EQ(g_ContinuationGlCalls, 0u);
    EXPECT_EQ(owners.backend.neutral_queries, 0u);
    EXPECT_FALSE(owners.resources.device.is_context_alive());
    EXPECT_EQ(owners.state.effects.get_view_count(), 0u);
}

TEST_F(SceneAuthoringCloseGlTest, PreviewSessionResetStopsFollowingModelsWithoutLosingContext)
{
    render::StudioDrawPacket packet;
    PreviewBackendScope backend(model_, &block_.header, packet);
    renderer_test::Owners owners;
    preview_test::BindEngine(owners.state);
    owners.state.lifecycle.OnContextCreated();
    g_ContinuationOwner = &owners;
    const auto previous_draw = owners.state.preview_engine.studio->StudioDrawModel;
    owners.state.preview_engine.studio->StudioDrawModel = LoadImageDuringPreview;
    g_ContinuationModels = 0;
    unsigned int loads = 0;
    owners.backend.on_load = [&](const char*, tex::TextureLoadGuard guard) {
        ++loads;
        EXPECT_EQ(render::StudioRendererTestAccess::QueuedCount(owners.state.studio), 0u);
        owners.state.lifecycle.OnClientStateClearing();
        EXPECT_FALSE(guard.is_epoch_current());
        return GLuint{0};
    };
    PreviewModel models[2]{};
    models[0].path = models[1].path = model_.name;
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.camera.fov = 90;
    params.models = models;
    params.model_count = 2;
    params.interactive = true;
    EXPECT_EQ(owners.state.preview.Draw(params, {0, 0, 32, 32}, {0, 0, 32, 32}), PreviewDrawStatus::ModelDrawFailed);
    EXPECT_FALSE(render::preview::ScenePreviewTestAccess::IsTargetActive(owners.state.preview));
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR)) << "canceled draw";
    EXPECT_EQ(g_ContinuationModels, 1u);
    EXPECT_EQ(loads, 1u);
    EXPECT_EQ(owners.state.effects.get_view_count(), 0u);
    EXPECT_TRUE(owners.resources.device.IsContextCurrent());
    EXPECT_EQ(owners.backend.neutral_queries, 0u);
    owners.backend.on_load = [&](const char*, tex::TextureLoadGuard) {
        ++loads;
        return GLuint{0};
    };
    owners.resources.textures.Get("fixtures/continuation.png");
    EXPECT_EQ(loads, 2u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR)) << "retry image";
    owners.state.preview_engine.studio->StudioDrawModel = previous_draw;
    EXPECT_EQ(owners.state.preview.Draw(params, {0, 0, 32, 32}, {0, 0, 32, 32}), PreviewDrawStatus::Ready);
    EXPECT_EQ(owners.state.effects.get_view_count(), 0u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR)) << "successful draw";
    g_ContinuationOwner = nullptr;
    owners.state.lifecycle.ReleaseLiveContext();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(SceneAuthoringCloseGlTest, PendingColdLoadLossStopsScenePreludeAndPreviewContextBeforeGl)
{
    render::StudioDrawPacket packet;
    PreviewBackendScope backend(model_, &block_.header, packet);
    for (bool scene_entry : {true, false})
    {
        SCOPED_TRACE(scene_entry);
        renderer_test::Owners owners;
        preview_test::BindEngine(owners.state);
        owners.state.lifecycle.OnContextCreated();
        PreviewModelInfo info{};
        render::preview::PreparedPreviewModel prepared;
        ASSERT_EQ(owners.state.preview_models.PrepareModel(model_.name, &info, &prepared), PreviewModelStatus::Ready);
        ncl_material::ProgramManifest manifest;
        ncl_material::ParamSchema schema;
        schema.name = "uImage";
        schema.type = ncl_material::ParamType::TextureRef;
        manifest.parameters.push_back(schema);
        render::StudioMaterialBindings bindings;
        bindings.manifest = &manifest;
        bindings.parameters.push_back(0);
        render::StudioRendererTestAccess::SetPendingImage(owners.state.studio, bindings);
        ASSERT_TRUE(render::StudioRendererTestAccess::IsPending(owners.state.studio));
        unsigned int loads = 0;
        owners.backend.on_load = [&](const char*, tex::TextureLoadGuard) {
            ++loads;
            EXPECT_FALSE(render::StudioRendererTestAccess::IsPending(owners.state.studio));
            owners.state.lifecycle.OnContextLost();
            g_AfterContinuationLoss = true;
            return GLuint{0};
        };
        g_ContinuationMatrix = glad_glMatrixMode;
        g_ContinuationFramebuffer = glad_glBindFramebuffer;
        g_ContinuationFrontFace = qglFrontFace;
        g_ContinuationGlCalls = 0;
        glad_glMatrixMode = ObserveContinuationMatrix;
        glad_glBindFramebuffer = ObserveContinuationFramebuffer;
        qglFrontFace = ObserveContinuationFrontFace;
        PreviewDrawParams params{};
        params.mode = PreviewRenderMode::MaterialAuthoring;
        PreviewModel model{};
        model.path = model_.name;
        if (scene_entry)
        {
            EXPECT_EQ(owners.state.preview.Draw(params, {0, 0, 32, 32}, {0, 0, 32, 32}), PreviewDrawStatus::RendererUnavailable);
        }
        else
        {
            EXPECT_EQ(owners.state.preview_renderer.Draw(model, params, 32, 32), -1);
        }
        g_AfterContinuationLoss = false;
        glad_glMatrixMode = g_ContinuationMatrix;
        glad_glBindFramebuffer = g_ContinuationFramebuffer;
        qglFrontFace = g_ContinuationFrontFace;
        EXPECT_EQ(loads, 1u);
        EXPECT_EQ(g_ContinuationGlCalls, 0u);
        EXPECT_EQ(owners.state.effects.get_view_count(), 0u);
    }
}

namespace
{
    goldsrc::model_t* LoseContextAfterPreviewResolution(const char* path, goldsrc::qboolean crash, goldsrc::qboolean track_crc)
    {
        goldsrc::model_t* model = Mod_ForName(path, crash, track_crc);
        g_ContinuationOwner->state.lifecycle.OnContextLost();
        g_AfterContinuationLoss = true;
        return model;
    }
} // namespace

TEST_F(SceneAuthoringCloseGlTest, ModelPreparationLossStopsBeforeCameraAndNativeDraw)
{
    render::StudioDrawPacket packet;
    PreviewBackendScope backend(model_, &block_.header, packet);
    renderer_test::Owners owners;
    preview_test::BindEngine(owners.state);
    owners.state.lifecycle.OnContextCreated();
    g_ContinuationOwner = &owners;
    owners.state.preview_engine.load_model = LoseContextAfterPreviewResolution;
    const auto previous_draw = owners.state.preview_engine.studio->StudioDrawModel;
    owners.state.preview_engine.studio->StudioDrawModel = LoadImageDuringPreview;
    g_ContinuationModels = g_ContinuationGlCalls = 0;
    g_ContinuationMatrix = glad_glMatrixMode;
    g_ContinuationFrontFace = qglFrontFace;
    glad_glMatrixMode = ObserveContinuationMatrix;
    qglFrontFace = ObserveContinuationFrontFace;
    PreviewModel model{};
    model.path = model_.name;
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    const int result = owners.state.preview_renderer.Draw(model, params, 32, 32);
    g_AfterContinuationLoss = false;
    glad_glMatrixMode = g_ContinuationMatrix;
    qglFrontFace = g_ContinuationFrontFace;
    owners.state.preview_engine.studio->StudioDrawModel = previous_draw;
    g_ContinuationOwner = nullptr;
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_ContinuationModels, 0u);
    EXPECT_EQ(g_ContinuationGlCalls, 0u);
}

namespace
{
    struct PreviewSlotResetTrace
    {
        decltype(qglFrontFace) original{};
        unsigned int front_faces{};
        bool canceled{};
    };
    PreviewSlotResetTrace* g_SlotResetTrace{};

    void APIENTRY ObserveSlotResetFrontFace(GLenum mode)
    {
        if (g_SlotResetTrace->canceled)
        {
            ++g_SlotResetTrace->front_faces;
        }
        g_SlotResetTrace->original(mode);
    }
} // namespace

TEST_F(SceneAuthoringCloseGlTest, LiveSessionCancellationPreservesReplacementValuesInTheSameEngineSlots)
{
    render::StudioDrawPacket packet;
    PreviewBackendScope backend(model_, &block_.header, packet);
    renderer_test::Owners owners;
    preview_test::BindEngine(owners.state);
    owners.state.lifecycle.OnContextCreated();
    g_ContinuationOwner = &owners;
    const auto previous_draw = owners.state.preview_engine.studio->StudioDrawModel;
    owners.state.preview_engine.studio->StudioDrawModel = LoadImageDuringPreview;
    goldsrc::cl_entity_t original_entity{};
    goldsrc::cl_entity_t replacement_entity{};
    goldsrc::cl_entity_t** const entity_slot = owners.state.preview_engine.current_entity;
    float* const blend_slot = owners.state.preview_engine.blend;
    *entity_slot = &original_entity;
    *blend_slot = .625f;
    PreviewSlotResetTrace trace{qglFrontFace};
    g_SlotResetTrace = &trace;
    qglFrontFace = ObserveSlotResetFrontFace;
    unsigned int loads = 0;
    owners.backend.on_load = [&](const char*, tex::TextureLoadGuard guard) {
        ++loads;
        owners.state.lifecycle.OnClientStateClearing();
        EXPECT_EQ(owners.state.preview_engine.current_entity, entity_slot);
        EXPECT_EQ(owners.state.preview_engine.blend, blend_slot);
        *entity_slot = &replacement_entity;
        *blend_slot = .125f;
        EXPECT_FALSE(guard.is_epoch_current());
        EXPECT_TRUE(owners.resources.device.IsContextCurrent());
        trace.canceled = true;
        return GLuint{0};
    };
    PreviewModel model{};
    model.path = model_.name;
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.camera.fov = 90;
    const int result = owners.state.preview_renderer.Draw(model, params, 32, 32);
    qglFrontFace = trace.original;
    g_SlotResetTrace = nullptr;
    owners.state.preview_engine.studio->StudioDrawModel = previous_draw;
    g_ContinuationOwner = nullptr;

    EXPECT_EQ(result, -1);
    EXPECT_EQ(loads, 1u);
    EXPECT_EQ(*entity_slot, &replacement_entity);
    EXPECT_FLOAT_EQ(*blend_slot, .125f);
    EXPECT_EQ(trace.front_faces, 0u);
    EXPECT_TRUE(owners.resources.device.IsContextCurrent());
    owners.state.lifecycle.ReleaseLiveContext();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
