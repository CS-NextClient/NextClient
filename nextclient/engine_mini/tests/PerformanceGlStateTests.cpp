#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glad/glad.h>
#include <next_engine_mini/ShaderRendererInterface.h>
#include <vgui/ILocalize.h>

#include "RadarGlContext.h"
#include "engine.h"
#include "performance/PerformancePanel.h"
#include "performance/PerformanceMapper.h"
#include "performance/PerformanceNetwork.h"
#include "performance/PerformanceMonitor.h"
#include "performance/performance_scopes.h"
#include "performance/GpuQueryBackend.h"
#include "graphics/text/text_render.h"
#include "graphics/render/render_device.h"
#include "graphics/render/render_context.h"
#include "console/console.h"

goldsrc::cl_enginefunc_t gEngfuncs{};

namespace
{
    std::optional<void*> g_DeviceTestContext;
}

namespace render
{
    void* context::Current()
    {
        return g_DeviceTestContext.has_value() ? *g_DeviceTestContext : RadarGlContext::Current();
    }
} // namespace render
vgui2::ILocalize* g_pLocalize{};
int* p_currenttexture{};

namespace
{
    class TestFontService : public TextRenderInterface
    {
    private:
        TextGlyph glyph_{};

    public:
        uint32_t glyph_reads{};
        std::string glyph_text;
        uint32_t generation = 1;
        bool fail_next{};

        void ReleaseGl()
        {
            glyph_text.clear();
            if (glyph_.texture)
            {
                glDeleteTextures(1, &glyph_.texture);
                glyph_ = {};
            }
        }
        TextFont_t FindOrCreateFont(const TextFontDesc&) override
        {
            return 1;
        }
        TextFont_t CreateEmptyFont() override
        {
            return 1;
        }
        bool AddGlyphSet(TextFont_t, const char*, int, int, int) override
        {
            return true;
        }
        bool AddFallbackFace(TextFont_t, const char*) override
        {
            return true;
        }
        const TextGlyph* GetGlyph(TextFont_t, uint32_t codepoint) override
        {
            ++glyph_reads;
            if (codepoint >= 32 && codepoint < 128)
            {
                glyph_text.push_back(char(codepoint));
            }
            if (fail_next)
            {
                fail_next = false;
                return nullptr;
            }
            if (!glyph_.texture)
            {
                glGenTextures(1, &glyph_.texture);
                glBindTexture(GL_TEXTURE_2D, glyph_.texture);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                const uint8_t pixel[] = {255, 255};
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 1, 1, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, pixel);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glyph_.u1 = glyph_.v1 = 1;
                glyph_.wide = glyph_.tall = 4;
                glyph_.advance = 8;
                glyph_.renderable = true;
                *p_currenttexture = int(glyph_.texture);
            }
            return &glyph_;
        }
        int GetLineHeight(TextFont_t) override
        {
            return 14;
        }
        int GetAscent(TextFont_t) override
        {
            return 12;
        }
        void GetCharABCwide(TextFont_t, uint32_t, int* a, int* b, int* c) override
        {
            *a = *c = 0;
            *b = 8;
        }
        int GetCharacterWidth(TextFont_t, uint32_t) override
        {
            return 8;
        }
        void GetTextSize(TextFont_t, const wchar_t*, int* wide, int* tall) override
        {
            *wide = 8;
            *tall = 14;
        }
        int GetGlyphAscent(TextFont_t, uint32_t) override
        {
            return 12;
        }
        uint32_t Generation() override
        {
            return generation;
        }
        bool GetFontDesc(TextFont_t, char*, int, int*, int*, int*) override
        {
            return false;
        }
    };

    TestFontService g_Font;

    performance::PerformancePanel MakePanel(TextRenderInterface& text, ncl_perf::PerfClockInterface& clock, int* current_texture)
    {
#ifdef _WIN32
        return performance::PerformancePanel(text, clock, current_texture);
#else
        (void)text;
        return performance::PerformancePanel(clock, current_texture);
#endif
    }

    class PanelLocalization final : public vgui2::ILocalize
    {
    private:
        wchar_t title_[32] = L"Late bound panel";

    public:
        uint32_t queries{};

        bool AddFile(IFileSystem*, const char*) override
        {
            return false;
        }
        void RemoveAll() override {}
        wchar_t* Find(const char* token) override
        {
            ++queries;
            return std::string_view(token) == "NextClient_Perf_Title" ? title_ : nullptr;
        }
        int ConvertANSIToUnicode(const char*, wchar_t*, int) override
        {
            return 0;
        }
        int ConvertUnicodeToANSI(const wchar_t*, char*, int) override
        {
            return 0;
        }
        vgui2::StringIndex_t FindIndex(const char*) override
        {
            return vgui2::INVALID_STRING_INDEX;
        }
        void ConstructString(wchar_t*, int, wchar_t*, int, ...) override {}
        const char* GetNameByIndex(vgui2::StringIndex_t) override
        {
            return nullptr;
        }
        wchar_t* GetValueByIndex(vgui2::StringIndex_t) override
        {
            return nullptr;
        }
        vgui2::StringIndex_t GetFirstStringIndex() override
        {
            return vgui2::INVALID_STRING_INDEX;
        }
        vgui2::StringIndex_t GetNextStringIndex(vgui2::StringIndex_t) override
        {
            return vgui2::INVALID_STRING_INDEX;
        }
        void AddString(const char*, wchar_t*, const char*) override {}
        void SetValueByIndex(vgui2::StringIndex_t, wchar_t*) override {}
        bool SaveToFile(IFileSystem*, const char*) override
        {
            return false;
        }
        int GetLocalizationFileCount() override
        {
            return 0;
        }
        const char* GetLocalizationFileName(int) override
        {
            return nullptr;
        }
        const char* GetFileNameByIndex(vgui2::StringIndex_t) override
        {
            return nullptr;
        }
        void ReloadLocalizationFiles(IFileSystem*) override {}
        void ConstructString(wchar_t*, int, const char*, KeyValues*) override {}
        void ConstructString(wchar_t*, int, vgui2::StringIndex_t, KeyValues*) override {}
    };

    void DrawWithLegacyApis(performance::PerformancePanel& panel, const performance::PerfSnapshot& snapshot)
    {
        PFNGLUSEPROGRAMPROC program = glad_glUseProgram;
        PFNGLACTIVETEXTUREPROC active = glad_glActiveTexture;
        PFNGLBINDFRAMEBUFFERPROC framebuffer = glad_glBindFramebuffer;
        PFNGLBLENDEQUATIONPROC blend = glad_glBlendEquation;
        int version_12 = GLAD_GL_VERSION_1_2;
        int version_13 = GLAD_GL_VERSION_1_3;
        glad_glUseProgram = nullptr;
        glad_glActiveTexture = nullptr;
        glad_glBindFramebuffer = nullptr;
        glad_glBlendEquation = nullptr;
        GLAD_GL_VERSION_1_2 = 0;
        GLAD_GL_VERSION_1_3 = 0;
#ifdef _MSC_VER
        __try
        {
            panel.Draw(snapshot, 640, 480, 1);
        }
        __finally
        {
#else
        panel.Draw(snapshot, 640, 480, 1);
        {
#endif
            glad_glUseProgram = program;
            glad_glActiveTexture = active;
            glad_glBindFramebuffer = framebuffer;
            glad_glBlendEquation = blend;
            GLAD_GL_VERSION_1_2 = version_12;
            GLAD_GL_VERSION_1_3 = version_13;
        }
    }

    struct GlSnapshot
    {
        std::vector<GLint> integers;
        std::vector<GLfloat> floats;
        int engine_texture{};
    };

    GlSnapshot Capture()
    {
        GlSnapshot result;
        const GLenum scalars[] = {
            GL_CURRENT_PROGRAM,
            GL_MATRIX_MODE,
            GL_ACTIVE_TEXTURE,
            GL_CLIENT_ACTIVE_TEXTURE,
            GL_READ_FRAMEBUFFER_BINDING,
            GL_DRAW_FRAMEBUFFER_BINDING,
            GL_READ_BUFFER,
            GL_DRAW_BUFFER,
            GL_BLEND_SRC_RGB,
            GL_BLEND_DST_RGB,
            GL_BLEND_SRC_ALPHA,
            GL_BLEND_DST_ALPHA,
            GL_BLEND_EQUATION_RGB,
            GL_BLEND_EQUATION_ALPHA,
            GL_UNPACK_ALIGNMENT,
            GL_ARRAY_BUFFER_BINDING,
            GL_ELEMENT_ARRAY_BUFFER_BINDING,
            GL_VERTEX_ARRAY_SIZE,
            GL_VERTEX_ARRAY_TYPE,
            GL_VERTEX_ARRAY_STRIDE
        };
        for (GLenum name : scalars)
        {
            GLint value = 0;
            glGetIntegerv(name, &value);
            result.integers.push_back(value);
        }
        for (GLenum name : {GL_VIEWPORT, GL_SCISSOR_BOX})
        {
            std::array<GLint, 4> values{};
            glGetIntegerv(name, values.data());
            result.integers.insert(result.integers.end(), values.begin(), values.end());
        }
        std::array<GLint, 2> polygon{};
        glGetIntegerv(GL_POLYGON_MODE, polygon.data());
        result.integers.insert(result.integers.end(), polygon.begin(), polygon.end());
        std::array<GLboolean, 4> color_mask{};
        glGetBooleanv(GL_COLOR_WRITEMASK, color_mask.data());
        for (GLboolean value : color_mask)
        {
            result.integers.push_back(value);
        }
        for (GLenum name :
             {GL_DEPTH_TEST,
              GL_ALPHA_TEST,
              GL_CULL_FACE,
              GL_LIGHTING,
              GL_FOG,
              GL_STENCIL_TEST,
              GL_SCISSOR_TEST,
              GL_BLEND,
              GL_CLIP_PLANE0,
              GL_CLIP_PLANE1,
              GL_CLIP_PLANE2,
              GL_CLIP_PLANE3,
              GL_CLIP_PLANE4,
              GL_CLIP_PLANE5,
              GL_VERTEX_ARRAY})
        {
            result.integers.push_back(glIsEnabled(name));
        }
        for (GLenum name : {GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX})
        {
            std::array<GLfloat, 16> matrix{};
            glGetFloatv(name, matrix.data());
            result.floats.insert(result.floats.end(), matrix.begin(), matrix.end());
        }
        std::array<GLfloat, 4> color{};
        glGetFloatv(GL_CURRENT_COLOR, color.data());
        result.floats.insert(result.floats.end(), color.begin(), color.end());
        GLint active = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        GLint draw_framebuffer = 0;
        GLint default_draw_buffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glGetIntegerv(GL_DRAW_BUFFER, &default_draw_buffer);
        result.integers.push_back(default_draw_buffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer);
        for (uint32_t unit = 0; unit < 2; ++unit)
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            GLint binding = 0;
            GLint environment = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
            glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &environment);
            result.integers.push_back(binding);
            result.integers.push_back(environment);
            for (GLenum target : {GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP})
            {
                result.integers.push_back(glIsEnabled(target));
            }
            std::array<GLfloat, 16> matrix{};
            glGetFloatv(GL_TEXTURE_MATRIX, matrix.data());
            result.floats.insert(result.floats.end(), matrix.begin(), matrix.end());
            glGetFloatv(GL_CURRENT_TEXTURE_COORDS, color.data());
            result.floats.insert(result.floats.end(), color.begin(), color.end());
        }
        glActiveTexture(active);
        result.engine_texture = *p_currenttexture;
        return result;
    }

    class GlClock : public ncl_perf::PerfClockInterface
    {
    public:
        uint64_t Now() override
        {
            return 0;
        }
    };

    class PerformanceGlStateTest : public testing::Test
    {
    protected:
        RadarGlContext context;
        std::array<GLuint, 2> framebuffers{};
        std::array<GLuint, 4> textures{};
        std::array<GLuint, 2> buffers{};
        GLuint program{};
        int engine_texture = 912;

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context.Initialize(error, true, 0, 640, 480)) << error;
            g_DeviceTestContext.reset();
            if (!GLAD_GL_VERSION_3_3)
            {
                GTEST_SKIP() << "GL 3.3 compatibility context unavailable";
            }
            p_currenttexture = &engine_texture;
            glGenTextures(GLsizei(textures.size()), textures.data());
            glGenFramebuffers(GLsizei(framebuffers.size()), framebuffers.data());
            for (uint32_t index = 0; index < 2; ++index)
            {
                glBindTexture(GL_TEXTURE_2D, textures[index]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[index]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[index], 0);
                ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            }
            const char* vertex = "#version 120\nvoid main(){gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;}";
            const char* fragment = "#version 120\nvoid main(){gl_FragColor=gl_Color;}";
            program = glCreateProgram();
            for (GLenum type : {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER})
            {
                GLuint shader = glCreateShader(type);
                const char* source = type == GL_VERTEX_SHADER ? vertex : fragment;
                glShaderSource(shader, 1, &source, nullptr);
                glCompileShader(shader);
                GLint compiled = 0;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
                ASSERT_TRUE(compiled);
                glAttachShader(program, shader);
                glDeleteShader(shader);
            }
            glLinkProgram(program);
            GLint linked = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            ASSERT_TRUE(linked);
            glUseProgram(program);
            const GLfloat vertices[] = {-0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0, 0.5f, 0};
            const GLuint indices[] = {0, 1, 2};
            glGenBuffers(2, buffers.data());
            glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glVertexPointer(3, GL_FLOAT, 0, nullptr);
            glEnableClientState(GL_VERTEX_ARRAY);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glDrawBuffer(GL_FRONT);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[1]);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glViewport(3, 5, 13, 11);
            glScissor(4, 6, 9, 8);
            glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
            glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_MAX);
            glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glColor4f(0.1f, 0.2f, 0.3f, 0.4f);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
            for (GLenum capability :
                 {GL_DEPTH_TEST,
                  GL_ALPHA_TEST,
                  GL_CULL_FACE,
                  GL_LIGHTING,
                  GL_FOG,
                  GL_STENCIL_TEST,
                  GL_SCISSOR_TEST,
                  GL_CLIP_PLANE0,
                  GL_CLIP_PLANE1})
            {
                glEnable(capability);
            }
            glDisable(GL_BLEND);
            for (uint32_t unit = 0; unit < 2; ++unit)
            {
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, textures[2 + unit]);
                glEnable(GL_TEXTURE_2D);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
                glMatrixMode(GL_TEXTURE);
                glLoadIdentity();
                glTranslatef(0.1f * float(unit + 1), 0.2f, 0);
                glTexCoord4f(0.3f, 0.4f, 0.5f, 1);
            }
            glClientActiveTexture(GL_TEXTURE1);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glScalef(0.9f, 0.8f, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glTranslatef(0.2f, 0.3f, 0);
            glMatrixMode(GL_TEXTURE);
            ASSERT_EQ(glGetError(), GLenum(GL_NO_ERROR));
        }

        void TearDown() override
        {
            g_DeviceTestContext.reset();
            if (p_currenttexture)
            {
                g_Font.ReleaseGl();
                glUseProgram(0);
                glDeleteProgram(program);
                glDeleteFramebuffers(2, framebuffers.data());
                glDeleteTextures(4, textures.data());
                glDeleteBuffers(2, buffers.data());
                p_currenttexture = nullptr;
            }
        }
    };
} // namespace

TextRenderInterface* TextRender_Get()
{
    return &g_Font;
}
void Con_DPrintf(ConLogType, const char*, ...) {}

#ifdef _WIN32
TEST_F(PerformanceGlStateTest, IndependentPanelsUseTheirSuppliedSharedTextProvider)
{
    GlClock clock;
    TestFontService first_font;
    TestFontService second_font;
    performance::PerformancePanel first = MakePanel(first_font, clock, p_currenttexture);
    performance::PerformancePanel second = MakePanel(second_font, clock, p_currenttexture);
    performance::PerfSnapshot snapshot;
    snapshot.mode = 1;
    uint32_t global_reads = g_Font.glyph_reads;
    first.Draw(snapshot, 640, 480, 1);
    EXPECT_GT(first_font.glyph_reads, 0u);
    EXPECT_EQ(second_font.glyph_reads, 0u);
    EXPECT_EQ(g_Font.glyph_reads, global_reads);
    uint32_t first_reads = first_font.glyph_reads;
    second.Draw(snapshot, 640, 480, 1);
    EXPECT_GT(second_font.glyph_reads, 0u);
    EXPECT_EQ(first_font.glyph_reads, first_reads);
    EXPECT_EQ(g_Font.glyph_reads, global_reads);
    first_font.ReleaseGl();
    second_font.ReleaseGl();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, ExistingPanelAcceptsLateLocalizationAndMissingOptionalProvider)
{
    GlClock clock;
    TestFontService font;
    PanelLocalization localization;
    performance::PerformancePanel panel = MakePanel(font, clock, p_currenttexture);
    performance::PerfSnapshot snapshot;
    snapshot.mode = 1;
    panel.Draw(snapshot, 640, 480, 1);
    EXPECT_EQ(localization.queries, 0u);
    EXPECT_EQ(font.glyph_text.find("Late bound panel"), std::string::npos);
    uint32_t reads = font.glyph_reads;
    panel.BindPresentation(&localization, nullptr);
    font.glyph_text.clear();
    panel.Draw(snapshot, 640, 480, 1);
    EXPECT_GT(localization.queries, 0u);
    EXPECT_GT(font.glyph_reads, reads);
    EXPECT_NE(font.glyph_text.find("Late bound panel"), std::string::npos);
    uint32_t queries = localization.queries;
    panel.BindPresentation(nullptr, nullptr);
    font.glyph_text.clear();
    panel.Draw(snapshot, 640, 480, 1);
    EXPECT_EQ(localization.queries, queries);
    EXPECT_EQ(font.glyph_text.find("Late bound panel"), std::string::npos);
    font.ReleaseGl();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
#endif

nitroapi::EngineData* eng()
{
    std::abort();
}

TEST_F(PerformanceGlStateTest, PanelAndMeasuredSubmissionRestoreTheCallerState)
{
    GlSnapshot before = Capture();
    performance::PerfSnapshot snapshot;
    snapshot.mode = 3;
    snapshot.graphs[0][0] = {3000000, 2000000, 1};
    snapshot.graphs[0][1] = {7000000, 3000000, 1};
    GlClock panel_clock;
    performance::PerformancePanel panel = MakePanel(g_Font, panel_clock, p_currenttexture);
    performance::PerfFrameGraphs frames;
    frames.frames.resize(performance::PerformancePanelLayout::GraphColumnCount(snapshot.panel_scale, 640, 480, 3));
    frames.frames.back().values[uint32_t(performance::PerfSeries::FrameInterval)] = {7000000, ncl_perf::PerfSampleState::Measured};
    frames.frames.back().values[uint32_t(performance::PerfSeries::HostCpu)] = {2000000, ncl_perf::PerfSampleState::Measured};
    panel.Draw(snapshot, 640, 480, 3, nullptr, nullptr, 2, nullptr, nullptr, &frames);
    GlSnapshot after = Capture();
    ASSERT_EQ(before.integers.size(), after.integers.size());
    for (uint32_t index = 0; index < before.integers.size(); ++index)
    {
        EXPECT_EQ(before.integers[index], after.integers[index]) << "GL property " << index;
    }
    EXPECT_EQ(before.floats, after.floats);
    EXPECT_EQ(before.engine_texture, after.engine_texture);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));

    GlClock clock;
    performance::GpuQueryBackend backend;
    backend.Probe();
    performance::PerformanceMonitor monitor(clock);
    monitor.SetGpuBackend(&backend);
    monitor.SetMainWindow(&clock);
    monitor.SetMode(1);
    monitor.OnContextCreated(&clock);
    const int counts[] = {3};
    const void* offsets[] = {nullptr};
    {
        performance::LocalCpuScope cpu(&monitor, performance::PerfStage::StudioShader);
        performance::LocalGpuScope gpu(&monitor, performance::PerfStage::StudioShader);
        glMultiDrawElements(GL_TRIANGLES, counts, GL_UNSIGNED_INT, offsets, 1);
    }

    performance::reporting::ReportDraw(&monitor, 3, 1);
    after = Capture();
    EXPECT_EQ(before.integers, after.integers);
    EXPECT_EQ(before.floats, after.floats);
    EXPECT_EQ(before.engine_texture, after.engine_texture);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    monitor.Shutdown();
}

TEST_F(PerformanceGlStateTest, OptionalBlocksRestoreStateWithPerformanceDisabled)
{
    GlSnapshot before = Capture();
    performance::PerfSnapshot snapshot;
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    network.connected = true;
    network.latest[uint32_t(performance::PerfNetworkSeries::Latency)] = {42, ncl_perf::PerfSampleState::Measured};
    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][0] = {42, 40, 1};
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = true;
    mapper.map_loaded = true;
    mapper.view_measured = true;
    mapper.wpoly = 1234;
    GlClock panel_clock;
    performance::PerformancePanel panel = MakePanel(g_Font, panel_clock, p_currenttexture);
    panel.Draw(snapshot, 1600, 1200, 0, &network, &mapper);

    GlSnapshot after = Capture();
    EXPECT_EQ(before.integers, after.integers);
    EXPECT_EQ(before.floats, after.floats);
    EXPECT_EQ(before.engine_texture, after.engine_texture);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, MapperPanelLeavesTheAimPointUnmarkedAndRestoresState)
{
    performance::PerfSnapshot snapshot;
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = mapper.target_hit = true;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(17.0f / 255.0f, 34.0f / 255.0f, 51.0f / 255.0f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[1]);
    glPopAttrib();
    GlSnapshot before = Capture();
    GlClock panel_clock;
    performance::PerformancePanel panel = MakePanel(g_Font, panel_clock, p_currenttexture);
    panel.Draw(snapshot, 640, 480, 0, nullptr, &mapper);
    GlSnapshot after = Capture();
    EXPECT_EQ(before.integers, after.integers);
    EXPECT_EQ(before.floats, after.floats);
    EXPECT_EQ(before.engine_texture, after.engine_texture);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    std::array<std::array<uint8_t, 4>, 9 * 9> pixels{};
    glReadPixels(316, 236, 9, 9, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (uint32_t index = 0; index < pixels.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(pixels[index][0], 17);
        EXPECT_EQ(pixels[index][1], 34);
        EXPECT_EQ(pixels[index][2], 51);
    }
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, ObjectOutlineUsesSceneMatricesFramebufferAndDepthWithoutStateLeaks)
{
    GLuint depth = 0;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 16, 16);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    ASSERT_EQ(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
    performance::PerfMapRenderView view;
    for (int index : {0, 5, 10, 15})
    {
        view.modelview[index] = view.projection[index] = 1;
    }
    view.viewport = {0, 0, 16, 16};
    view.depth_range = {0, 1};
    view.depth_function = GL_LEQUAL;
    view.draw_framebuffer = int(framebuffers[1]);
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.target_hit = true;
    mapper.level = 2;
    mapper.outline.push_back({{-0.9f, 0, 0}, {0.9f, 0, 0}});
    auto clear = [&](double value) {
        GLint previous = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[1]);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearColor(0, 0, 0, 1);
        glClearDepth(value);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(previous));
        glPopAttrib();
    };
    auto pixel = [&]() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[1]);
        std::array<uint8_t, 4> color{};
        glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
        return color;
    };
    clear(0.4);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_FRONT);
    GlSnapshot before = Capture();
    performance::panel::DrawMapOutline(mapper, view);
    GlSnapshot after = Capture();
    EXPECT_EQ(before.integers, after.integers);
    EXPECT_EQ(before.floats, after.floats);
    EXPECT_EQ(pixel()[0], 0);
    clear(1);
    performance::panel::DrawMapOutline(mapper, view);
    EXPECT_EQ(pixel()[0], 80);
    EXPECT_EQ(pixel()[1], 220);
    EXPECT_EQ(pixel()[2], 255);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[1]);
    float depth_value = 0;
    glReadPixels(8, 8, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth_value);
    EXPECT_FLOAT_EQ(depth_value, 1);
    view.depth_range = {1, 0};
    view.depth_function = GL_GEQUAL;
    mapper.outline[0].start.z = mapper.outline[0].end.z = 0.5f;
    clear(0.4);
    performance::panel::DrawMapOutline(mapper, view);
    EXPECT_EQ(pixel()[0], 0);
    clear(0);
    performance::panel::DrawMapOutline(mapper, view);
    EXPECT_EQ(pixel()[0], 80);
    mapper.level = 1;
    clear(1);
    performance::panel::DrawMapOutline(mapper, view);
    EXPECT_EQ(pixel()[0], 0);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    glDeleteRenderbuffers(1, &depth);
}

TEST_F(PerformanceGlStateTest, CpuPanelWorksWithoutOptionalLegacyGlEntryPoints)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    GlSnapshot before = Capture();
    performance::PerfSnapshot snapshot;
    snapshot.mode = 1;
    GlClock panel_clock;
    performance::PerformancePanel panel = MakePanel(g_Font, panel_clock, p_currenttexture);
    DrawWithLegacyApis(panel, snapshot);
    GlSnapshot after = Capture();
    EXPECT_EQ(before.integers, after.integers);
    EXPECT_EQ(before.floats, after.floats);
    EXPECT_EQ(before.engine_texture, after.engine_texture);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, DeviceLossDisablesGlBeforeResourceCallbacks)
{
    render::Device_OnContextCreated();
    class Resource : public render::IDeviceResource
    {
    public:
        bool observed_live{};
        void OnDeviceLost() override
        {
            observed_live = render::Device_IsContextAlive();
        }
    } resource;
    render::Device_Register(&resource);
    render::Device_OnContextDestroyed();
    render::Device_Unregister(&resource);
    EXPECT_FALSE(resource.observed_live);
}

namespace
{
    PFNGLBEGINPROC g_PanelOriginalBegin{};
    uint32_t g_PanelQuadBegins{};
    PFNGLCOLOR4UBPROC g_PanelOriginalColor{};
    uint32_t g_PanelColorCalls{};
    uint8_t g_PanelFirstAlpha{};

    void APIENTRY CountPanelBegin(GLenum mode)
    {
        if (mode == GL_QUADS)
        {
            ++g_PanelQuadBegins;
        }
        g_PanelOriginalBegin(mode);
    }

    void APIENTRY RecordPanelColor(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
    {
        if (!g_PanelColorCalls++)
        {
            g_PanelFirstAlpha = alpha;
        }
        g_PanelOriginalColor(red, green, blue, alpha);
    }
}

TEST_F(PerformanceGlStateTest, PanelReusesGlyphsAndGroupsAtlasDraws)
{
    performance::PerfSnapshot snapshot;
    GlClock panel_clock;
    performance::PerformancePanel panel = MakePanel(g_Font, panel_clock, p_currenttexture);
    panel.Draw(snapshot, 1600, 1200, 1);
    uint32_t reads = g_Font.glyph_reads;
    g_PanelQuadBegins = 0;
    g_PanelOriginalBegin = glad_glBegin;
    glad_glBegin = CountPanelBegin;
    panel.Draw(snapshot, 1600, 1200, 1);
    glad_glBegin = g_PanelOriginalBegin;
    EXPECT_EQ(g_Font.glyph_reads, reads);
    std::unique_ptr<performance::PerformancePanelLayout> layout = std::make_unique<performance::PerformancePanelLayout>();
    layout->Build(snapshot, 1600, 1200, 1);
    EXPECT_LE(g_PanelQuadBegins, layout->get_commands().size());
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
namespace
{
    class PanelTestClock : public ncl_perf::PerfClockInterface
    {
    public:
        uint64_t time{};
        uint64_t Now() override
        {
            return time;
        }
    };
}

TEST_F(PerformanceGlStateTest, MapperRevisionRefreshesGazeAndPreservesPerformanceCacheAndGlState)
{
    PanelTestClock clock;
    performance::PerformancePanel panel = MakePanel(g_Font, clock, p_currenttexture);
    performance::PerfSnapshot snapshot;
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = mapper.target_hit = true;
    mapper.level = 2;
    mapper.target_name = "func_wall";
    mapper.revision = 1;
    panel.Draw(snapshot, 640, 480, 1, nullptr, &mapper);
    EXPECT_NE(g_Font.glyph_text.find("E: dump to console"), std::string::npos);
    GlSnapshot before = Capture();
    g_Font.glyph_text.clear();
    mapper.target_name = "func_door";
    mapper.target_position = {4, 5, 6};
    mapper.target_origin = {7, 8, 9};
    ++mapper.revision;
    snapshot.summaries[0].fps = 999;
    panel.Draw(snapshot, 640, 480, 1, nullptr, &mapper);
    GlSnapshot after = Capture();
    EXPECT_EQ(before.integers, after.integers);
    EXPECT_EQ(before.floats, after.floats);
    EXPECT_NE(g_Font.glyph_text.find("func_door"), std::string::npos);
    EXPECT_NE(g_Font.glyph_text.find("Look: 4.0 5.0 6.0"), std::string::npos);
    EXPECT_NE(g_Font.glyph_text.find("Object origin: 7.0 8.0 9.0"), std::string::npos);
    EXPECT_EQ(g_Font.glyph_text.find("FPS 999"), std::string::npos);
    uint32_t reads = g_Font.glyph_reads;
    panel.Draw(snapshot, 640, 480, 1, nullptr, &mapper);
    EXPECT_EQ(g_Font.glyph_reads, reads);

    g_Font.glyph_text.clear();
    mapper.level = 1;
    ++mapper.revision;
    ++g_Font.generation;
    panel.Draw(snapshot, 640, 480, 1, nullptr, &mapper);
    GlSnapshot summary = Capture();
    EXPECT_EQ(before.integers, summary.integers);
    EXPECT_EQ(before.floats, summary.floats);
    EXPECT_EQ(g_Font.glyph_text.find("Object"), std::string::npos);
    EXPECT_EQ(g_Font.glyph_text.find("func_door"), std::string::npos);
    EXPECT_NE(g_Font.glyph_text.find("Look: 4.0 5.0 6.0"), std::string::npos);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, PanelRefreshesAtBoundedIntervalsAndInvalidatesGeometry)
{
    PanelTestClock clock;
    performance::PerformancePanel panel = MakePanel(g_Font, clock, p_currenttexture);
    performance::PerfSnapshot snapshot;
    panel.Draw(snapshot, 1600, 1200, 2);
    uint32_t reads = g_Font.glyph_reads;
    clock.time = 249000000;
    snapshot.summaries[0].fps = 123;
    panel.Draw(snapshot, 1600, 1200, 2);
    EXPECT_EQ(g_Font.glyph_reads, reads);
    clock.time = 250000000;
    panel.Draw(snapshot, 1600, 1200, 2);
    EXPECT_GT(g_Font.glyph_reads, reads);
    reads = g_Font.glyph_reads;
    panel.Draw(snapshot, 640, 480, 2);
    EXPECT_GT(g_Font.glyph_reads, reads);
    reads = g_Font.glyph_reads;
    ++g_Font.generation;
    panel.Draw(snapshot, 640, 480, 2);
    EXPECT_GT(g_Font.glyph_reads, reads);
    reads = g_Font.glyph_reads;
    panel.Invalidate();
    panel.Draw(snapshot, 640, 480, 2);
    EXPECT_GT(g_Font.glyph_reads, reads);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, BackingAlphaChangesWithoutWaitingForTheLayoutRefresh)
{
    PanelTestClock clock;
    performance::PerformancePanel panel = MakePanel(g_Font, clock, p_currenttexture);
    performance::PerfSnapshot snapshot;
    panel.Draw(snapshot, 1600, 1200, 2);
    struct Case
    {
        float alpha;
        uint8_t expected;
    };
    for (const Case& item : {Case{0.0f, 0}, Case{1.0f, 255}, Case{0.25f, 64}})
    {
        panel.SetBackgroundAlpha(item.alpha);
        g_PanelColorCalls = 0;
        g_PanelOriginalColor = glad_glColor4ub;
        glad_glColor4ub = RecordPanelColor;
        panel.Draw(snapshot, 1600, 1200, 2);
        glad_glColor4ub = g_PanelOriginalColor;
        EXPECT_GT(g_PanelColorCalls, 0u);
        EXPECT_EQ(g_PanelFirstAlpha, item.expected);
        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    }
}

TEST_F(PerformanceGlStateTest, PanelRetriesTransientGlyphsAndRefreshesLiveBatchingImmediately)
{
    PanelTestClock clock;
    performance::PerformancePanel panel = MakePanel(g_Font, clock, p_currenttexture);
    performance::PerfSnapshot snapshot;
    performance::PerfSnapshot captured;
    captured.cpu_frame_count = 1;
    captured.cpu_frame_id = 1;
    g_Font.fail_next = true;
    panel.Draw(snapshot, 640, 480, 0, nullptr, nullptr, 2, &captured);
    uint32_t reads = g_Font.glyph_reads;
    panel.Draw(snapshot, 640, 480, 0, nullptr, nullptr, 2, &captured);
    EXPECT_GT(g_Font.glyph_reads, reads);
    reads = g_Font.glyph_reads;
    panel.Draw(snapshot, 640, 480, 0, nullptr, nullptr, 2, &captured);
    EXPECT_EQ(g_Font.glyph_reads, reads);
    captured.cpu_frame_id = 2;
    captured.cpu_frame_count = 2;
    panel.Draw(snapshot, 640, 480, 0, nullptr, nullptr, 2, &captured);
    EXPECT_GT(g_Font.glyph_reads, reads);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, IndependentBatchingRestoresCallerState)
{
    GlSnapshot before = Capture();
    performance::PerfSnapshot snapshot;
    GlClock panel_clock;
    performance::PerformancePanel panel = MakePanel(g_Font, panel_clock, p_currenttexture);
    panel.Draw(snapshot, 640, 480, 0, nullptr, nullptr, 2);
    GlSnapshot after = Capture();
    EXPECT_EQ(before.integers, after.integers);
    EXPECT_EQ(before.floats, after.floats);
    EXPECT_EQ(before.engine_texture, after.engine_texture);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, LiveBatchingReusesUnchangedTextGeometryBetweenFrames)
{
    PanelTestClock clock;
    performance::PerformancePanel panel = MakePanel(g_Font, clock, p_currenttexture);
    performance::PerfSnapshot snapshot;
    performance::PerfSnapshot live;
    live.cpu_frame_count = 8;
    live.cpu_frame_id = 1;
    live.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 7;
    live.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 7;
    panel.Draw(snapshot, 1600, 1200, 2, nullptr, nullptr, 2, &live);
    uint32_t reads = g_Font.glyph_reads;
    live.cpu_frame_id = 2;
    live.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 8;
    live.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 8;
    panel.Draw(snapshot, 1600, 1200, 2, nullptr, nullptr, 2, &live);
    EXPECT_GT(g_Font.glyph_reads, reads);
    EXPECT_LT(g_Font.glyph_reads - reads, 128u);
    reads = g_Font.glyph_reads;
    live.cpu_frame_id = 3;
    panel.Draw(snapshot, 1600, 1200, 2, nullptr, nullptr, 2, &live);
    EXPECT_EQ(g_Font.glyph_reads, reads);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(PerformanceGlStateTest, ShaderRendererAvailabilityUsesCachedContextState)
{
    render::ShaderRendererInterface* renderer =
        reinterpret_cast<render::ShaderRendererInterface*>(CreateInterface(SHADER_RENDERER_INTERFACE_VERSION, nullptr));
    ASSERT_NE(renderer, nullptr);

    render::Device_OnContextCreated();
    ASSERT_TRUE(GLAD_GL_VERSION_3_3);
    EXPECT_TRUE(renderer->is_available());

    int version_available = GLAD_GL_VERSION_3_3;
    GLAD_GL_VERSION_3_3 = 0;
    EXPECT_TRUE(renderer->is_available());
    GLAD_GL_VERSION_3_3 = version_available;

    render::Device_OnContextDestroyed();
    EXPECT_FALSE(renderer->is_available());
    EXPECT_FALSE(renderer->is_available());
    EXPECT_FALSE(render::Device_IsContextAlive());

    render::Device_OnContextCreated();
    EXPECT_TRUE(renderer->is_available());
    render::Device_OnContextDestroyed();
}

TEST_F(PerformanceGlStateTest, PacketRevisionRefreshesNetworkAndBatchingWithoutRefreshingPerformanceOrMapper)
{
    for (int mode : {0, 2})
    {
        for (const std::array<int, 2>& dimensions :
             {std::array<int, 2>{640, 1800}, std::array<int, 2>{1600, 1800}, std::array<int, 2>{1600, 1200}})
        {
            int width = dimensions[0];
            int height = dimensions[1];
            PanelTestClock clock;
            performance::PerformancePanel panel = MakePanel(g_Font, clock, p_currenttexture);
            performance::PerfSnapshot snapshot;
            snapshot.panel_scale = 1;
            performance::PerfSnapshot live;
            live.cpu_frame_count = live.cpu_frame_id = 1;
            performance::PerfNetworkSnapshot network;
            network.enabled = network.connected = true;
            network.packet_age_ms = 1;
            network.revision = 1;
            performance::PerfMapperSnapshot mapper;
            mapper.enabled = mapper.map_loaded = mapper.view_measured = true;
            mapper.level = 2;
            for (int index = 0; index < 20; ++index)
            {
                mapper.properties.push_back({"key" + std::to_string(index), "value"});
            }
            mapper.wpoly = 1234;
            panel.Draw(snapshot, width, height, mode, &network, &mapper, 2, &live);
            uint32_t reads = g_Font.glyph_reads;
            snapshot.summaries[0].fps = 999;
            mapper.wpoly = 9999;
            network.packet_age_ms = 2;
            ++network.revision;
            live.cpu_frame_count = live.cpu_frame_id = 2;
            g_Font.glyph_text.clear();
            panel.Draw(snapshot, width, height, mode, &network, &mapper, 2, &live);

            EXPECT_GT(g_Font.glyph_reads, reads);
            EXPECT_NE(g_Font.glyph_text.find("Last packet 0.002 s ago"), std::string::npos);
            EXPECT_NE(g_Font.glyph_text.find("last 2 frames"), std::string::npos);
            EXPECT_EQ(g_Font.glyph_text.find("FPS 999"), std::string::npos);
            EXPECT_EQ(g_Font.glyph_text.find("wpoly 9999"), std::string::npos);
            reads = g_Font.glyph_reads;
            ++network.revision;
            panel.Draw(snapshot, width, height, mode, &network, &mapper, 2, &live);
            EXPECT_EQ(g_Font.glyph_reads, reads);
            for (std::vector<performance::PerfPlotBin>& graph : network.graphs)
            {
                graph.resize(300);
            }
            g_Font.glyph_text.clear();
            panel.Draw(snapshot, width, height, mode, &network, &mapper, 2, &live);
            EXPECT_NE(g_Font.glyph_text.find("Network | last 300 packets"), std::string::npos);
            EXPECT_EQ(g_Font.glyph_text.find("FPS 999"), std::string::npos);
            EXPECT_EQ(g_Font.glyph_text.find("wpoly 9999"), std::string::npos);
            EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
        }
    }
}

TEST_F(PerformanceGlStateTest, DeviceLossUsesSnapshotAndSkipsOwnersRemovedByAnEarlierCallback)
{
    render::Device_OnContextCreated();
    class Resource : public render::IDeviceResource
    {
    public:
        render::IDeviceResource* remove{};
        uint32_t calls{};
        void OnDeviceLost() override
        {
            EXPECT_FALSE(render::Device_IsContextAlive());
            ++calls;
            if (remove != nullptr)
            {
                render::Device_Unregister(remove);
            }
        }
    } first, removed, remaining;
    first.remove = &removed;
    render::Device_Register(&first);
    render::Device_Register(&removed);
    render::Device_Register(&remaining);
    render::Device_OnContextDestroyed();
    EXPECT_EQ(first.calls, 1u);
    EXPECT_EQ(removed.calls, 0u);
    EXPECT_EQ(remaining.calls, 1u);
    render::Device_Unregister(&first);
    render::Device_Unregister(&removed);
    render::Device_Unregister(&remaining);
}
TEST_F(PerformanceGlStateTest, DeviceCurrentRequiresTheStoredNonNullNativeIdentity)
{
    render::Device_OnContextCreated();
    ASSERT_TRUE(render::Device_IsContextCurrent());
    uint32_t generation = render::Device_Generation();
    render::Device_OnContextCreated();
    EXPECT_EQ(render::Device_Generation(), generation);
    int replacement = 0;
    g_DeviceTestContext = &replacement;
    EXPECT_TRUE(render::Device_IsContextAlive());
    EXPECT_FALSE(render::Device_IsContextCurrent());
    g_DeviceTestContext = nullptr;
    EXPECT_FALSE(render::Device_IsContextCurrent());
    g_DeviceTestContext = context.native_context();
    EXPECT_TRUE(render::Device_IsContextCurrent());
    render::Device_OnContextDestroyed();
    EXPECT_FALSE(render::Device_IsContextCurrent());
}
