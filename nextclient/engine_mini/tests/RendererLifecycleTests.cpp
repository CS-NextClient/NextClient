#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "RadarGlContext.h"
#include "RendererOwnerTestSupport.h"
#include "PreviewTestOwners.h"
#include "StudioModelTestSupport.h"
#include "graphics/render/render_context.h"
#include "graphics/texture_loader.h"

namespace
{
    render::StudioInterceptState* g_ActiveIntercept{};
    unsigned int g_StockDraws{};
    std::function<void()> g_TextureCallback;
    unsigned int g_NativeGenerations{};
    void NativeTextureCallback(const char*, int, int, char*)
    {
        g_TextureCallback();
    }
    void APIENTRY CountNativeGenerations(GLsizei, GLuint*)
    {
        ++g_NativeGenerations;
    }
    void StockDraw()
    {
        ++g_StockDraws;
    }
    void InterceptDraw()
    {
        g_ActiveIntercept->StudioDrawPoints();
    }

    class RendererLifecycleTest : public testing::Test
    {
    protected:
        RadarGlContext context_;
        goldsrc::cvar_t developer_{};
        goldsrc::cvar_t* previous_developer_{};
        std::unique_ptr<renderer_test::Owners> owners_;
        studio_test::ModelBlock block_;
        goldsrc::model_t model_{};

        void SetUp() override
        {
            previous_developer_ = developer;
            developer = &developer_;
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            owners_ = std::make_unique<renderer_test::Owners>();
            owners_->state.lifecycle.OnContextCreated();
            owners_->external.events.clear();
            std::strcpy(model_.name, "models/lifecycle_test.mdl");
            ASSERT_TRUE(owners_->state.models.PrepareMetadata(&model_, &block_.header, &block_.header, &model_));
        }
        void TearDown() override
        {
            owners_->external.on_event = {};
            owners_->backend.on_delete_texture = {};
            owners_->state.lifecycle.ReleaseLiveContext();
            owners_.reset();
            developer = previous_developer_;
        }
        GLuint RetainTexture()
        {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            const unsigned char pixel[4] = {1, 2, 3, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            owners_->backend.neutral = texture;
            EXPECT_EQ(owners_->resources.textures.Neutral(), texture);
            return texture;
        }
    };
} // namespace

TEST_F(RendererLifecycleTest, ConstructionAndIndependentOwnersHaveNoLifecycleEffects)
{
    renderer_test::Owners second;
    EXPECT_TRUE(second.external.events.empty());
    EXPECT_FALSE(second.resources.device.is_context_alive());
    EXPECT_EQ(second.state.models.ModelCount(), 0);
    second.state.lifecycle.OnClientStateClearing();
    EXPECT_EQ(owners_->state.models.ModelCount(), 1);
    EXPECT_TRUE(owners_->external.events.empty());
}

TEST_F(RendererLifecycleTest, MapResetDiscardsAndCancelsBeforeExternalReentry)
{
    goldsrc::cvar_t shader{};
    goldsrc::cvar_t materials{};
    shader.string = const_cast<char*>("2.250");
    materials.string = const_cast<char*>("03");
    render::StudioDrawPacket packet;
    packet.meshes.emplace_back();
    render::StudioDrawQueue& queue = render::StudioRendererTestAccess::Queue(owners_->state.studio);
    queue.BeginOpaquePass(true);
    queue.Submit(packet);
    ASSERT_EQ(render::StudioRendererTestAccess::QueuedCount(owners_->state.studio), 1u);
    render::preview::PreviewInteraction& picking = render::preview::ScenePreviewTestAccess::Interaction(owners_->state.preview);
    picking.Capture(packet);
    ASSERT_EQ(render::preview::PreviewInteractionTestAccess::Count(picking), 1u);
    owners_->state.capture.Begin("map", shader, &materials);
    owners_->capture.on_set = [&] {
        EXPECT_EQ(render::StudioRendererTestAccess::QueuedCount(owners_->state.studio), 0u);
        EXPECT_EQ(render::preview::PreviewInteractionTestAccess::Count(picking), 0u);
    };
    owners_->external.on_event = [&](std::string_view event) {
        if (event == "map")
        {
            EXPECT_FALSE(owners_->state.capture.is_pending());
            EXPECT_STREQ(shader.string, "2.250");
            EXPECT_STREQ(materials.string, "03");
            EXPECT_EQ(render::StudioRendererTestAccess::QueuedCount(owners_->state.studio), 0u);
            EXPECT_EQ(render::preview::PreviewInteractionTestAccess::Count(picking), 0u);
            owners_->state.lifecycle.OnClientStateClearing();
        }
    };
    owners_->state.lifecycle.OnMapReset();
    EXPECT_EQ(owners_->state.models.ModelCount(), 0);
    EXPECT_TRUE(owners_->resources.device.IsContextCurrent());
    EXPECT_EQ(owners_->external.events, (std::vector<std::string>{"map"}));
    owners_->capture.on_set = {};
}

TEST_F(RendererLifecycleTest, DisconnectAndClientClearPreserveTheOwningContext)
{
    const uint32_t generation = owners_->resources.device.get_generation();
    owners_->state.lifecycle.OnDisconnectBegin();
    EXPECT_EQ(owners_->state.models.ModelCount(), 1);
    owners_->state.lifecycle.OnClientStateClearing();
    EXPECT_EQ(owners_->state.models.ModelCount(), 0);
    EXPECT_EQ(owners_->resources.device.get_generation(), generation);
    EXPECT_TRUE(owners_->resources.device.IsContextCurrent());
    EXPECT_TRUE(owners_->external.events.empty());
}

TEST_F(RendererLifecycleTest, LiveReleaseDeletesOnceBeforeDeviceInvalidationAndNestedRelease)
{
    GLuint texture = RetainTexture();
    unsigned int deletions = 0;
    owners_->backend.on_delete_texture = [&](unsigned int actual) {
        EXPECT_EQ(actual, texture);
        EXPECT_TRUE(owners_->resources.device.IsContextCurrent());
        ++deletions;
        owners_->state.lifecycle.ReleaseLiveContext();
    };
    owners_->state.lifecycle.ReleaseLiveContext();
    owners_->state.lifecycle.ReleaseLiveContext();
    EXPECT_EQ(deletions, 1u);
    EXPECT_FALSE(glIsTexture(texture));
    EXPECT_FALSE(owners_->resources.device.is_context_alive());
    EXPECT_EQ(std::count(owners_->external.events.begin(), owners_->external.events.end(), "release"), 1);
}

TEST_F(RendererLifecycleTest, LossMarksDeviceUnavailableBeforeResourceNotificationsAndKeepsMetadata)
{
    struct Observer : render::IDeviceResource
    {
        render::RenderDevice& device;
        bool notified{};
        explicit Observer(render::RenderDevice& input) :
            device(input)
        {}
        void OnDeviceLost() override
        {
            EXPECT_FALSE(device.is_context_alive());
            notified = true;
        }
    } observer(owners_->resources.device);
    GLuint texture = RetainTexture();
    owners_->resources.device.Register(&observer);
    owners_->state.lifecycle.OnContextLost();
    owners_->resources.device.Unregister(&observer);
    EXPECT_TRUE(observer.notified);
    EXPECT_EQ(owners_->state.models.ModelCount(), 1);
    EXPECT_TRUE(glIsTexture(texture));
    owners_->state.lifecycle.OnContextLost();
    EXPECT_EQ(std::count(owners_->external.events.begin(), owners_->external.events.end(), "abandon"), 1);
    glDeleteTextures(1, &texture);
}

TEST_F(RendererLifecycleTest, SameContextPreservesResourcesAndStandaloneReleaseAllowsReadoption)
{
    GLuint texture = RetainTexture();
    const uint32_t generation = owners_->resources.device.get_generation();
    owners_->state.lifecycle.OnContextCreated();
    EXPECT_EQ(owners_->resources.device.get_generation(), generation);
    EXPECT_TRUE(glIsTexture(texture));
    EXPECT_TRUE(owners_->external.events.empty());
    owners_->state.lifecycle.ReleaseLiveContext();
    owners_->state.lifecycle.OnContextCreated();
    EXPECT_TRUE(owners_->resources.device.IsContextCurrent());
    EXPECT_GT(owners_->resources.device.get_generation(), generation);
}

TEST_F(RendererLifecycleTest, ReplacementContextAbandonsWithoutDeletingAndCanBeAdopted)
{
    GLuint texture = RetainTexture();
    RadarGlContext replacement;
    std::string error;
    ASSERT_TRUE(replacement.Initialize(error)) << error;
    owners_->state.lifecycle.OnContextCreated();
    EXPECT_EQ(owners_->state.models.ModelCount(), 1);
    EXPECT_TRUE(owners_->resources.device.IsContext(replacement.native_context()));
    EXPECT_EQ(std::count(owners_->external.events.begin(), owners_->external.events.end(), "abandon"), 1);
    owners_->state.lifecycle.ReleaseLiveContext();
    ASSERT_TRUE(context_.MakeCurrent());
    EXPECT_TRUE(glIsTexture(texture));
    glDeleteTextures(1, &texture);
}

TEST_F(RendererLifecycleTest, TerminalShutdownStopsInterceptionBeforeCaptureRestoreReentry)
{
    goldsrc::cvar_t shader{};
    shader.string = const_cast<char*>("04.0");
    render::RenderConfig config{};
    config.studio_shader = &shader;
    owners_->state.studio_engine.config = &config;
    goldsrc::engine_studio_api_t api{};
    api.StudioDrawPoints = StockDraw;
    render::StudioInterceptCallbacks callbacks;
    callbacks.studio.StudioDrawPoints = InterceptDraw;
    g_ActiveIntercept = &owners_->state.intercept;
    g_StockDraws = 0;
    owners_->state.intercept.PatchApi(&api, nullptr, callbacks);
    void (*copied_draw)() = api.StudioDrawPoints;
    copied_draw();
    ASSERT_EQ(g_StockDraws, 1u);
    ASSERT_STREQ(owners_->state.studio.get_last_rejection(), "cvar is off");
    owners_->state.capture.Begin("terminal", shader, nullptr);
    shader.value = 1;
    bool nested = false;
    owners_->capture.on_set = [&] {
        if (!nested)
        {
            nested = true;
            copied_draw();
            EXPECT_EQ(g_StockDraws, 2u);
            EXPECT_STREQ(owners_->state.studio.get_last_rejection(), "cvar is off");
            owners_->state.lifecycle.OnClientStateClearing();
            owners_->state.lifecycle.OnContextCreated();
        }
    };
    owners_->state.lifecycle.BeginShutdown();
    EXPECT_TRUE(nested);
    EXPECT_STREQ(shader.string, "04.0");
    EXPECT_EQ(api.StudioDrawPoints, StockDraw);
    g_ActiveIntercept = nullptr;
    owners_->capture.on_set = {};
    owners_->state.lifecycle.ReleaseLiveContext();
    const uint32_t generation = owners_->resources.device.get_generation();
    owners_->state.lifecycle.OnContextCreated();
    EXPECT_EQ(owners_->resources.device.get_generation(), generation);
    EXPECT_FALSE(owners_->resources.device.is_context_alive());
}

TEST_F(RendererLifecycleTest, FatalShutdownCompletesAfterUnavailableDeviceAndOnlyOnce)
{
    bool observed = false;
    owners_->external.on_event = [&](std::string_view event) {
        if (event == "shutdown")
        {
            observed = true;
            EXPECT_FALSE(owners_->resources.device.is_context_alive());
            EXPECT_EQ(owners_->state.models.ModelCount(), 0);
            owners_->state.lifecycle.OnFatalShutdown();
        }
    };
    owners_->state.lifecycle.OnFatalShutdown();
    owners_->state.lifecycle.OnFatalShutdown();
    EXPECT_TRUE(observed);
    EXPECT_EQ(std::count(owners_->external.events.begin(), owners_->external.events.end(), "shutdown"), 1);
}

TEST_F(RendererLifecycleTest, TestQuadUsesItsSuppliedShaderOwnerAndReleasesBeforeDeviceLoss)
{
    renderer_test::Owners second;
    second.state.lifecycle.OnContextCreated();
    owners_->state.test_quad.Draw(0.5f);
    const GLuint first_vao = render::TestQuadTestAccess::Vao(owners_->state.test_quad);
    ASSERT_NE(first_vao, 0u);
    EXPECT_TRUE(glIsVertexArray(first_vao));
    EXPECT_EQ(owners_->backend.builds, 1u);
    EXPECT_EQ(second.backend.builds, 0u);
    second.state.test_quad.Draw(1.0f);
    EXPECT_EQ(second.backend.builds, 1u);
    const GLuint second_vao = render::TestQuadTestAccess::Vao(second.state.test_quad);
    owners_->state.lifecycle.ReleaseLiveContext();
    EXPECT_FALSE(glIsVertexArray(first_vao));
    EXPECT_TRUE(glIsVertexArray(second_vao));
    second.state.lifecycle.ReleaseLiveContext();
    EXPECT_FALSE(glIsVertexArray(second_vao));
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(RendererLifecycleTest, NativeTextureCallbackCancellationStopsBeforeGlGeneration)
{
    uint64_t epoch = 1;
    const auto previous_callback = g_modfuncs.m_pfnTextureLoad;
    const auto previous_generate = qglGenTextures;
    g_modfuncs.m_pfnTextureLoad = NativeTextureCallback;
    qglGenTextures = CountNativeGenerations;
    g_NativeGenerations = 0;
    unsigned int callbacks = 0;
    g_TextureCallback = [&] {
        ++callbacks;
        ++epoch;
    };
    tex::TextureLoadGuard guard;
    guard.native_context = context_.native_context();
    guard.preparation_epoch = &epoch;
    guard.expected_preparation_epoch = epoch;
    tex::Texture texture{};
    tex::TexIdentifierStr identifier;
    identifier.assign("guarded");
    const uint8_t pixel[4] = {1, 2, 3, 255};
    const bool loaded = tex::LoadTexture(texture, identifier, TextureFormat::RGBA, pixel, 1, 1, false, nullptr, GL_LINEAR, guard);
    qglGenTextures = previous_generate;
    g_modfuncs.m_pfnTextureLoad = previous_callback;
    g_TextureCallback = {};
    EXPECT_FALSE(loaded);
    EXPECT_EQ(callbacks, 1u);
    EXPECT_EQ(g_NativeGenerations, 0u);
    EXPECT_EQ(texture.texnum, 0);
}

TEST_F(RendererLifecycleTest, NativeTextureCallbackLossStopsBeforeGlGeneration)
{
    const auto previous_callback = g_modfuncs.m_pfnTextureLoad;
    const auto previous_generate = qglGenTextures;
    g_modfuncs.m_pfnTextureLoad = NativeTextureCallback;
    qglGenTextures = CountNativeGenerations;
    g_NativeGenerations = 0;
    g_TextureCallback = [&] { owners_->state.lifecycle.OnContextLost(); };
    tex::TextureLoadGuard guard;
    guard.native_context = context_.native_context();
    guard.device = &owners_->resources.device;
    guard.expected_device_generation = owners_->resources.device.get_generation();
    tex::Texture texture{};
    tex::TexIdentifierStr identifier;
    identifier.assign("loss");
    const uint8_t pixel[4] = {1, 2, 3, 255};
    const bool loaded = tex::LoadTexture(texture, identifier, TextureFormat::RGBA, pixel, 1, 1, false, nullptr, GL_LINEAR, guard);
    qglGenTextures = previous_generate;
    g_modfuncs.m_pfnTextureLoad = previous_callback;
    g_TextureCallback = {};
    EXPECT_FALSE(loaded);
    EXPECT_EQ(g_NativeGenerations, 0u);
    EXPECT_EQ(texture.texnum, 0);
    EXPECT_FALSE(owners_->resources.device.is_context_alive());
}

TEST_F(RendererLifecycleTest, CanceledCacheLoadKeepsItsOwnedPathAndNeverDeletesTheReplacementContextName)
{
    RadarGlContext replacement;
    std::string path = "fixtures/owned_path.png";
    GLuint sentinel = 0;
    unsigned int loads = 0;
    unsigned int deletions = 0;
    owners_->backend.on_delete_texture = [&](unsigned int) { ++deletions; };
    owners_->backend.on_load = [&](const char* owned_path, tex::TextureLoadGuard) {
        ++loads;
        path.assign(1024, 'x');
        EXPECT_STREQ(owned_path, "fixtures/owned_path.png");
        std::string error;
        EXPECT_TRUE(replacement.Initialize(error)) << error;
        owners_->resources.device.OnContextCreated();
        glGenTextures(1, &sentinel);
        glBindTexture(GL_TEXTURE_2D, sentinel);
        return sentinel;
    };
    bool canceled = false;
    EXPECT_EQ(owners_->resources.textures.Get(path.c_str(), {}, &canceled), 0u);
    EXPECT_TRUE(canceled);
    EXPECT_EQ(loads, 1u);
    EXPECT_EQ(deletions, 0u);
    EXPECT_TRUE(glIsTexture(sentinel));
    owners_->backend.on_load = [&](const char*, tex::TextureLoadGuard) {
        ++loads;
        return GLuint{0};
    };
    EXPECT_EQ(owners_->resources.textures.Get("fixtures/owned_path.png"), 0u);
    EXPECT_EQ(loads, 2u);
    glDeleteTextures(1, &sentinel);
    owners_->state.lifecycle.ReleaseLiveContext();
    EXPECT_TRUE(context_.MakeCurrent());
}
