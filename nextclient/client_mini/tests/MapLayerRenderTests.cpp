#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include <glad/glad.h>
#include <next_client_mini/radar_cvars.h>

#include "hud/radar/map/MapRender.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/integration/RadarLayersDump.h"
#include "RadarMapLayerFixtures.h"

namespace
{
    struct GlState
    {
        GLint active = GL_TEXTURE1;
        std::array<GLuint, 2> textures{11, 12};
        std::array<bool, 2> texture_enabled{true, false};
        std::array<GLint, 2> env{GL_REPLACE, GL_ADD};
        std::array<std::array<float, 4>, 2> coords{{{.1f, .2f, .3f, 1}, {.4f, .5f, .6f, 1}}};
        GLuint program = 23;
        GLuint draw_fbo = 31;
        GLuint read_fbo = 32;
        GLuint rbo = 33;
        GLint matrix_mode = GL_TEXTURE;
        std::array<int, 4> viewport{3, 4, 800, 600};
        std::array<float, 4> clear{.2f, .3f, .4f, .5f};
        std::array<float, 4> color{.6f, .7f, .8f, .9f};
        std::array<GLboolean, 4> color_mask{GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE};
        GLboolean depth_mask = GL_FALSE;
        GLenum depth_func = GL_GREATER;
        GLenum shade_model = GL_FLAT;
        double clear_depth = .25;
        std::map<GLenum, bool> enabled{
            {GL_BLEND, true},
            {GL_DEPTH_TEST, true},
            {GL_ALPHA_TEST, true},
            {GL_CULL_FACE, true},
            {GL_SCISSOR_TEST, true},
            {GL_LIGHTING, true},
            {GL_FOG, true}
        };
        std::array<GLenum, 4> blend{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO};
        GLenum blend_equation = GL_FUNC_REVERSE_SUBTRACT;
        std::array<GLenum, 2> polygon{GL_LINE, GL_POINT};
        bool operator==(const GlState&) const = default;
    };

    struct DrawVertex
    {
        std::array<float, 3> position;
        std::array<float, 2> uv;
        std::array<float, 2> lightmap_uv;
        std::array<float, 4> color;
        GLenum shade_model;
    };

    struct GlFake
    {
        GlState state;
        std::vector<GlState> attributes;
        std::map<GLenum, int> matrix_depth;
        std::set<GLuint> textures;
        std::set<GLuint> framebuffers;
        std::set<GLuint> renderbuffers;
        GLuint next = 100;
        GLenum status = GL_FRAMEBUFFER_COMPLETE;
        GLenum error = GL_NO_ERROR;
        bool fail_storage{};
        bool fail_depth{};
        bool fail_draw{};
        std::vector<GLuint> mipmap_textures;
        std::map<GLuint, GLint> min_filters;
        std::vector<std::pair<GLuint, int>> deleted_levels;
        int error_reads{};
        std::vector<GLuint> texture_checks;
        int image_count{};
        int clears{};
        int vertices{};
        GLenum primitive{};
        std::vector<std::array<float, 3>> tone_vertices;
        std::vector<std::array<float, 4>> pixels;
        std::vector<DrawVertex> drawn;
        std::array<double, 6> projection{};
        GLenum pass_depth{};
        std::array<float, 4> pass_clear{};
        std::array<GLboolean, 4> pass_color_mask{};
        std::array<int, 4> pass_viewport{};
        std::array<float, 2> last_uv{};
        std::array<float, 2> last_lightmap_uv{};
        GLenum internal_format{};
        int width{};
        int height{};
    };

    GlFake* g_Fake;

    int Unit()
    {
        return g_Fake->state.active - GL_TEXTURE0;
    }

    void APIENTRY GetInteger(GLenum name, GLint* out)
    {
        const GlState& s = g_Fake->state;

        switch (name)
        {
            case GL_ACTIVE_TEXTURE:
                *out = s.active;
                break;
            case GL_TEXTURE_BINDING_2D:
                *out = s.textures[Unit()];
                break;
            case GL_CURRENT_PROGRAM:
                *out = s.program;
                break;
            case GL_DRAW_FRAMEBUFFER_BINDING:
                *out = s.draw_fbo;
                break;
            case GL_READ_FRAMEBUFFER_BINDING:
                *out = s.read_fbo;
                break;
            case GL_RENDERBUFFER_BINDING:
                *out = s.rbo;
                break;
            case GL_MATRIX_MODE:
                *out = s.matrix_mode;
                break;
            default:
                ADD_FAILURE() << "Unexpected GL state query " << name;

                break;
        }
    }

    void APIENTRY ActiveTexture(GLenum unit)
    {
        g_Fake->state.active = unit;
    }

    void APIENTRY BindTexture(GLenum, GLuint texture)
    {
        g_Fake->state.textures[Unit()] = texture;
    }

    void APIENTRY ShadeModel(GLenum mode)
    {
        g_Fake->state.shade_model = mode;
    }

    void APIENTRY BlendFunc(GLenum source, GLenum destination)
    {
        g_Fake->state.blend = {source, destination, source, destination};
    }

    void APIENTRY BlendEquation(GLenum equation)
    {
        g_Fake->state.blend_equation = equation;
    }

    void APIENTRY UseProgram(GLuint program)
    {
        g_Fake->state.program = program;
    }

    void APIENTRY BindFramebuffer(GLenum target, GLuint framebuffer)
    {
        if (target != GL_READ_FRAMEBUFFER)
        {
            g_Fake->state.draw_fbo = framebuffer;
        }

        if (target != GL_DRAW_FRAMEBUFFER)
        {
            g_Fake->state.read_fbo = framebuffer;
        }
    }

    void APIENTRY BindRenderbuffer(GLenum, GLuint buffer)
    {
        g_Fake->state.rbo = buffer;
    }

    void APIENTRY PushAttrib(GLbitfield bits)
    {
        EXPECT_EQ(bits, GL_ALL_ATTRIB_BITS);

        g_Fake->attributes.push_back(g_Fake->state);
    }

    void APIENTRY PopAttrib()
    {
        GlState current = g_Fake->state;
        g_Fake->state = g_Fake->attributes.back();
        g_Fake->attributes.pop_back();
        g_Fake->state.program = current.program;
        g_Fake->state.draw_fbo = current.draw_fbo;
        g_Fake->state.read_fbo = current.read_fbo;
        g_Fake->state.rbo = current.rbo;
        g_Fake->state.matrix_mode = current.matrix_mode;
    }

    void APIENTRY MatrixMode(GLenum mode)
    {
        g_Fake->state.matrix_mode = mode;
    }

    void APIENTRY PushMatrix()
    {
        ++g_Fake->matrix_depth[g_Fake->state.matrix_mode];
    }

    void APIENTRY PopMatrix()
    {
        --g_Fake->matrix_depth[g_Fake->state.matrix_mode];
    }

    void APIENTRY LoadIdentity() {}
    void APIENTRY Ortho(double a, double b, double c, double d, double e, double f)
    {
        g_Fake->projection = {a, b, c, d, e, f};
    }

    void APIENTRY Enable(GLenum cap)
    {
        if (cap == GL_TEXTURE_2D)
        {
            g_Fake->state.texture_enabled[Unit()] = true;
        }
        else
        {
            g_Fake->state.enabled[cap] = true;
        }
    }

    void APIENTRY Disable(GLenum cap)
    {
        if (cap == GL_TEXTURE_2D)
        {
            g_Fake->state.texture_enabled[Unit()] = false;
        }
        else
        {
            g_Fake->state.enabled[cap] = false;
        }
    }

    void APIENTRY DepthMask(GLboolean mask)
    {
        g_Fake->state.depth_mask = mask;
    }

    void APIENTRY DepthFunc(GLenum func)
    {
        g_Fake->state.depth_func = func;
    }

    void APIENTRY ClearDepth(double value)
    {
        g_Fake->state.clear_depth = value;
    }

    void APIENTRY ColorMask(GLboolean a, GLboolean b, GLboolean c, GLboolean d)
    {
        g_Fake->state.color_mask = {a, b, c, d};
    }

    void APIENTRY ClearColor(float a, float b, float c, float d)
    {
        g_Fake->state.clear = {a, b, c, d};
    }

    void APIENTRY Color(float a, float b, float c, float d)
    {
        g_Fake->state.color = {a, b, c, d};
    }

    void APIENTRY Viewport(int a, int b, int c, int d)
    {
        g_Fake->state.viewport = {a, b, c, d};
    }

    void APIENTRY PolygonMode(GLenum face, GLenum mode)
    {
        if (face != GL_BACK)
        {
            g_Fake->state.polygon[0] = mode;
        }

        if (face != GL_FRONT)
        {
            g_Fake->state.polygon[1] = mode;
        }
    }

    void APIENTRY TexEnv(GLenum, GLenum, GLint mode)
    {
        g_Fake->state.env[Unit()] = mode;
    }

    void APIENTRY MultiTexCoord(GLenum unit, float u, float v)
    {
        g_Fake->state.coords[unit - GL_TEXTURE0] = {u, v, 0, 1};

        if (unit == GL_TEXTURE0)
        {
            g_Fake->last_uv = {u, v};
        }
        else
        {
            g_Fake->last_lightmap_uv = {u, v};
        }
    }

    void APIENTRY GenTextures(GLsizei count, GLuint* out)
    {
        for (int i = 0; i < count; i++)
        {
            out[i] = g_Fake->next++;
            g_Fake->textures.insert(out[i]);
        }
    }

    void APIENTRY DeleteTextures(GLsizei count, const GLuint* ids)
    {
        for (int i = 0; i < count; i++)
        {
            g_Fake->textures.erase(ids[i]);
        }
    }

    void APIENTRY GenFramebuffers(GLsizei count, GLuint* out)
    {
        for (int i = 0; i < count; i++)
        {
            out[i] = g_Fake->next++;
            g_Fake->framebuffers.insert(out[i]);
        }
    }

    void APIENTRY DeleteFramebuffers(GLsizei count, const GLuint* ids)
    {
        for (int i = 0; i < count; i++)
        {
            g_Fake->framebuffers.erase(ids[i]);
        }
    }

    void APIENTRY GenRenderbuffers(GLsizei count, GLuint* out)
    {
        for (int i = 0; i < count; i++)
        {
            out[i] = g_Fake->fail_depth ? 0 : g_Fake->next++;

            if (out[i])
            {
                g_Fake->renderbuffers.insert(out[i]);
            }
        }
    }

    void APIENTRY DeleteRenderbuffers(GLsizei count, const GLuint* ids)
    {
        for (int i = 0; i < count; i++)
        {
            g_Fake->renderbuffers.erase(ids[i]);
        }
    }

    void APIENTRY Image(GLenum, GLint level, GLint format, GLsizei width, GLsizei height, GLint, GLenum, GLenum, const void*)
    {
        if (level > 0)
        {
            EXPECT_EQ(width, 0);
            EXPECT_EQ(height, 0);
            g_Fake->deleted_levels.emplace_back(g_Fake->state.textures[Unit()], level);
            return;
        }

        g_Fake->internal_format = format;
        g_Fake->width = width;
        g_Fake->height = height;
        ++g_Fake->image_count;

        if (g_Fake->fail_storage)
        {
            g_Fake->error = GL_OUT_OF_MEMORY;
        }
    }

    void APIENTRY Parameter(GLenum, GLenum name, GLint value)
    {
        if (name == GL_TEXTURE_MIN_FILTER)
        {
            g_Fake->min_filters[g_Fake->state.textures[Unit()]] = value;
        }
    }

    void APIENTRY GenerateMipmap(GLenum target)
    {
        EXPECT_EQ(target, GL_TEXTURE_2D);
        g_Fake->mipmap_textures.push_back(g_Fake->state.textures[Unit()]);
    }
    void APIENTRY RenderbufferStorage(GLenum, GLenum, GLsizei, GLsizei) {}
    void APIENTRY FramebufferTexture(GLenum, GLenum, GLenum, GLuint, GLint) {}
    void APIENTRY FramebufferRenderbuffer(GLenum, GLenum, GLenum, GLuint) {}
    GLenum APIENTRY FramebufferStatus(GLenum)
    {
        return g_Fake->status;
    }

    GLenum APIENTRY GetError()
    {
        ++g_Fake->error_reads;
        GLenum error = g_Fake->error;
        g_Fake->error = GL_NO_ERROR;

        return error;
    }

    GLboolean APIENTRY IsTexture(GLuint texture)
    {
        g_Fake->texture_checks.push_back(texture);

        return texture != 0;
    }

    void APIENTRY Clear(GLbitfield bits)
    {
        EXPECT_TRUE(bits == (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) || bits == GL_DEPTH_BUFFER_BIT);

        if (bits & GL_COLOR_BUFFER_BIT)
        {
            ++g_Fake->clears;
        }
        g_Fake->pass_depth = g_Fake->state.depth_func;
        g_Fake->pass_clear = g_Fake->state.clear;
        g_Fake->pass_color_mask = g_Fake->state.color_mask;
        g_Fake->pass_viewport = g_Fake->state.viewport;

        EXPECT_TRUE(g_Fake->state.depth_mask);
        EXPECT_FALSE(g_Fake->state.enabled[GL_BLEND]);
        EXPECT_FALSE(g_Fake->state.enabled[GL_SCISSOR_TEST]);
    }

    void APIENTRY Begin(GLenum mode)
    {
        EXPECT_TRUE(mode == GL_POLYGON || mode == GL_TRIANGLE_STRIP);
        g_Fake->primitive = mode;
    }

    void APIENTRY End()
    {
        if (g_Fake->primitive != GL_TRIANGLE_STRIP)
        {
            return;
        }

        const GlState& state = g_Fake->state;
        EXPECT_FALSE(state.texture_enabled[0]);
        EXPECT_FALSE(state.texture_enabled[1]);
        EXPECT_FALSE(state.enabled.at(GL_DEPTH_TEST));
        EXPECT_TRUE(state.enabled.at(GL_BLEND));
        EXPECT_EQ(state.blend_equation, GL_FUNC_ADD);

        for (std::array<float, 4>& pixel : g_Fake->pixels)
        {
            float source = state.blend[0] == GL_ZERO ? 0.0f : (state.blend[0] == GL_DST_ALPHA ? pixel[3] : state.color[3]);
            float destination =
                state.blend[1] == GL_DST_ALPHA ? pixel[3] : (state.blend[1] == GL_ONE_MINUS_SRC_ALPHA ? 1.0f - state.color[3] : 1.0f);

            for (size_t channel = 0; channel < pixel.size(); ++channel)
            {
                if (state.color_mask[channel])
                {
                    pixel[channel] = state.color[channel] * source + pixel[channel] * destination;
                }
            }
        }
    }
    void APIENTRY Vertex(float x, float y, float z)
    {
        ++g_Fake->vertices;

        if (g_Fake->fail_draw)
        {
            g_Fake->error = GL_INVALID_OPERATION;
        }

        if (g_Fake->primitive == GL_TRIANGLE_STRIP)
        {
            g_Fake->tone_vertices.push_back({x, y, z});
            return;
        }

        g_Fake->drawn.push_back({{x, y, z}, g_Fake->last_uv, g_Fake->last_lightmap_uv, g_Fake->state.color, g_Fake->state.shade_model});
    }

    class MapLayerRender : public testing::Test
    {
    protected:
        GlFake fake_;
        std::vector<std::function<void()>> restore_;
        RadarMapLayers map_;
        std::vector<MapLayerMaterial> materials_;

        template <class T>
        void Bind(T& entry, T replacement)
        {
            T saved = entry;
            restore_.emplace_back([&entry, saved] { entry = saved; });
            entry = replacement;
        }

        void SetUp() override
        {
            g_Fake = &fake_;
            Bind(GLAD_GL_VERSION_3_0, 1);
            Bind(glad_glGetIntegerv, GetInteger);
            Bind(glad_glActiveTexture, ActiveTexture);
            Bind(glad_glBindTexture, BindTexture);
            Bind(glad_glUseProgram, UseProgram);
            Bind(glad_glBlendFunc, BlendFunc);
            Bind(glad_glBlendEquation, BlendEquation);
            Bind(glad_glShadeModel, ShadeModel);
            Bind(glad_glBindFramebuffer, BindFramebuffer);
            Bind(glad_glBindRenderbuffer, BindRenderbuffer);
            Bind(glad_glPushAttrib, PushAttrib);
            Bind(glad_glPopAttrib, PopAttrib);
            Bind(glad_glMatrixMode, MatrixMode);
            Bind(glad_glPushMatrix, PushMatrix);
            Bind(glad_glPopMatrix, PopMatrix);
            Bind(glad_glLoadIdentity, LoadIdentity);
            Bind(glad_glOrtho, Ortho);
            Bind(glad_glEnable, Enable);
            Bind(glad_glDisable, Disable);
            Bind(glad_glDepthMask, DepthMask);
            Bind(glad_glDepthFunc, DepthFunc);
            Bind(glad_glClearDepth, ClearDepth);
            Bind(glad_glClearColor, ClearColor);
            Bind(glad_glColorMask, ColorMask);
            Bind(glad_glColor4f, Color);
            Bind(glad_glViewport, Viewport);
            Bind(glad_glPolygonMode, PolygonMode);
            Bind(glad_glTexEnvi, TexEnv);
            Bind(glad_glMultiTexCoord2f, MultiTexCoord);
            Bind(glad_glGenTextures, GenTextures);
            Bind(glad_glDeleteTextures, DeleteTextures);
            Bind(glad_glGenFramebuffers, GenFramebuffers);
            Bind(glad_glDeleteFramebuffers, DeleteFramebuffers);
            Bind(glad_glGenRenderbuffers, GenRenderbuffers);
            Bind(glad_glDeleteRenderbuffers, DeleteRenderbuffers);
            Bind(glad_glTexImage2D, Image);
            Bind(glad_glTexParameteri, Parameter);
            Bind(glad_glGenerateMipmap, GenerateMipmap);
            Bind(glad_glRenderbufferStorage, RenderbufferStorage);
            Bind(glad_glFramebufferTexture2D, FramebufferTexture);
            Bind(glad_glFramebufferRenderbuffer, FramebufferRenderbuffer);
            Bind(glad_glCheckFramebufferStatus, FramebufferStatus);
            Bind(glad_glGetError, GetError);
            Bind(glad_glIsTexture, IsTexture);
            Bind(glad_glClear, Clear);
            Bind(glad_glBegin, Begin);
            Bind(glad_glEnd, End);
            Bind(glad_glVertex3f, Vertex);
            RadarMapLayerFixture fixture;

            for (int i = 0; i < 4; i++)
            {
                fixture.AddFloor((float)i * 256, {{0, 0}, {256, 256}});
            }

            for (RadarMapSurface& surface : fixture.geometry.surfaces)
            {
                for (RadarMapVertex& vertex : surface.vertices)
                {
                    vertex.lightmap_uv = {vertex.uv.x * .5f + .25f, vertex.uv.y * .5f + .5f};
                }
            }

            ASSERT_EQ(map_.Build(fixture.geometry), RadarMapStatus::Ready);

            std::string error;
            ASSERT_TRUE(map_.ApplyOverrides(
                {{{"floor0", -128.0f, 128.0f}, {"floor1", 128.0f, 384.0f}, {"floor2", 384.0f, 640.0f}, {"floor3", 640.0f, 896.0f}}, {}},
                error
            )) << error;
            materials_.assign(4, {7, 8, true});
        }

        void TearDown() override
        {
            for (const auto& restore : restore_)
            {
                restore();
            }

            g_Fake = nullptr;
        }
    };

    TEST(RadarCvars, MapSmoothingDefaultsDisabledAndBounded)
    {
        RadarCvarDef smoothing = RadarCvarDef_Get("hud_radar_map_smoothing");

        EXPECT_STREQ(smoothing.default_value, "0");
        EXPECT_EQ(smoothing.min_value, 0.0f);
        EXPECT_EQ(smoothing.max_value, 1.0f);
    }

    TEST_F(MapLayerRender, GeneratedImagesRetainLinearFilteringWithoutMipLevels)
    {
        for (bool textured : {false, true})
        {
            SCOPED_TRACE(textured);
            GlState before = fake_.state;
            MapLayerRenderer renderer;
            ASSERT_EQ(renderer.Ensure(map_, materials_, 0, textured, 10), RadarMapStatus::Ready);
            GLuint texture = renderer.get_map(0)->texture;
            EXPECT_EQ(fake_.min_filters[texture], GL_LINEAR);
            EXPECT_TRUE(fake_.mipmap_textures.empty());
            EXPECT_TRUE(fake_.deleted_levels.empty());
            EXPECT_EQ(fake_.state, before);
            int vertices = fake_.vertices;
            int allocations = fake_.image_count;

            ASSERT_EQ(renderer.Ensure(map_, {}, 0, textured, 11), RadarMapStatus::Ready);
            EXPECT_EQ(fake_.vertices, vertices);
            EXPECT_EQ(fake_.image_count, allocations);
            EXPECT_EQ(renderer.get_map(0)->texture, texture);
        }
    }

    TEST_F(MapLayerRender, CachedLayerImagesPreserveTheirPixelsAndLinearFilter)
    {
        MapLayerRenderer renderer;
        GlState before = fake_.state;
        std::array<GLuint, 3> textures{};

        for (int layer = 0; layer < 3; ++layer)
        {
            ASSERT_EQ(renderer.Ensure(map_, {}, layer, false, layer + 10), RadarMapStatus::Ready);
            textures[layer] = renderer.get_map(layer)->texture;
        }

        int vertices = fake_.vertices;
        int allocations = fake_.image_count;
        for (int layer = 0; layer < 3; ++layer)
        {
            ASSERT_EQ(renderer.Ensure(map_, {}, layer, false, layer + 20), RadarMapStatus::Ready);
            const RuntimeMap& image = *renderer.get_map(layer);
            EXPECT_EQ(image.texture, textures[layer]);
            EXPECT_EQ(fake_.min_filters[image.texture], GL_LINEAR);
        }

        EXPECT_TRUE(fake_.mipmap_textures.empty());
        EXPECT_TRUE(fake_.deleted_levels.empty());
        EXPECT_EQ(fake_.vertices, vertices);
        EXPECT_EQ(fake_.image_count, allocations);
        EXPECT_EQ(fake_.state, before);
    }


    TEST_F(MapLayerRender, GeneratedPixelsPremultiplyAlphaForFiltering)
    {
        fake_.pixels = {{0.2f, 0.4f, 0.8f, 0.5f}, {0.8f, 0.6f, 0.4f, 0.0f}, {0.6f, 0.7f, 0.8f, 1.0f}};
        GlState before = fake_.state;
        MapLayerRenderer renderer;
        ASSERT_EQ(renderer.Ensure(map_, {}, 0, false, 10), RadarMapStatus::Ready);
        EXPECT_EQ(
            fake_.pixels, (std::vector<std::array<float, 4>>{{0.1f, 0.2f, 0.4f, 0.5f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.6f, 0.7f, 0.8f, 1.0f}})
        );
        EXPECT_EQ(fake_.state, before);
    }

    TEST_F(MapLayerRender, CachedImageSkipsMaterialsOnlyForMatchingMapAndMode)
    {
        MapLayerRenderer renderer;
        EXPECT_FALSE(renderer.is_cached(map_, 0, true));

        ASSERT_EQ(renderer.Ensure(map_, materials_, 0, true, 10), RadarMapStatus::Ready);
        ASSERT_TRUE(renderer.is_cached(map_, 0, true));
        EXPECT_FALSE(renderer.is_cached(map_, 1, true));
        EXPECT_FALSE(renderer.is_cached(map_, 0, false));

        RadarMapLayers other;
        EXPECT_FALSE(renderer.is_cached(other, 0, true));

        int vertices = fake_.vertices;
        EXPECT_EQ(renderer.Ensure(map_, {}, 0, true, 11), RadarMapStatus::Ready);
        EXPECT_EQ(fake_.vertices, vertices);
        EXPECT_EQ(renderer.get_cache().get_entries()[0].last_frame, 11u);

        renderer.Reset();

        EXPECT_FALSE(renderer.is_cached(map_, 0, true));

        EXPECT_EQ(renderer.Ensure(map_, {}, 0, false, 12), RadarMapStatus::Ready);
    }

    TEST_F(MapLayerRender, RestoresStateAndUsesTransparentUppermostPass)
    {
        GlState before = fake_.state;
        MapLayerRenderer renderer;

        ASSERT_EQ(renderer.Ensure(map_, materials_, 0, true, 10), RadarMapStatus::Ready);
        ASSERT_NE(renderer.get_map(0), nullptr);
        EXPECT_TRUE(renderer.get_map(0)->valid);
        EXPECT_EQ(renderer.get_map(0)->bounds.min, map_.get_bounds().min);
        EXPECT_EQ(renderer.get_map(0)->bounds.max, map_.get_bounds().max);
        EXPECT_EQ(fake_.state, before);
        EXPECT_EQ(fake_.pass_depth, GL_LEQUAL);
        EXPECT_EQ(fake_.pass_clear, (std::array<float, 4>{0, 0, 0, 0}));
        EXPECT_EQ(fake_.pass_color_mask, (std::array<GLboolean, 4>{GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}));
        EXPECT_EQ(fake_.pass_viewport, (std::array<int, 4>{0, 0, 1024, 1024}));
        EXPECT_EQ(fake_.internal_format, GL_RGBA8);
        EXPECT_EQ(fake_.width, 1024);
        EXPECT_EQ(fake_.height, 1024);
        EXPECT_GT(fake_.vertices, 0);

        bool clipped_corner = false;

        for (const DrawVertex& vertex : fake_.drawn)
        {
            if (vertex.position == std::array<float, 3>{32, 32, 0})
            {
                clipped_corner = true;

                EXPECT_EQ(vertex.uv, (std::array<float, 2>{.125f, .125f}));
                EXPECT_EQ(vertex.lightmap_uv, (std::array<float, 2>{.3125f, .5625f}));
                EXPECT_EQ(vertex.color, (std::array<float, 4>{.55f, .55f, .55f, 1}));
            }
        }

        EXPECT_TRUE(clipped_corner);

        fake_.drawn.clear();
        ASSERT_EQ(renderer.Ensure(map_, materials_, 3, true, 11), RadarMapStatus::Ready);
        ASSERT_FALSE(fake_.drawn.empty());
        EXPECT_EQ(fake_.drawn.back().color, (std::array<float, 4>{.55f, .55f, .55f, 1}));
        EXPECT_TRUE(fake_.attributes.empty());
        EXPECT_EQ(fake_.matrix_depth[GL_PROJECTION], 0);
        EXPECT_EQ(fake_.matrix_depth[GL_MODELVIEW], 0);
    }

    TEST_F(MapLayerRender, PendingContextErrorsDoNotFailNewLayerImages)
    {
        for (bool textured : {false, true})
        {
            for (GLenum error : {GL_INVALID_VALUE, GL_INVALID_ENUM})
            {
                fake_.error = error;
                GlState before = fake_.state;
                MapLayerRenderer renderer;

                EXPECT_EQ(renderer.Ensure(map_, materials_, 0, textured, 10), RadarMapStatus::Ready);
                EXPECT_NE(renderer.get_map(0), nullptr);
                EXPECT_TRUE(renderer.get_failure_reason().empty());
                EXPECT_EQ(fake_.state, before);
                EXPECT_EQ(fake_.error, GL_NO_ERROR);
            }
        }
    }

    TEST_F(MapLayerRender, FloorShadingUsesSelectedSupportRangeWhileProjectionIncludesTallDetails)
    {
        for (float adjacent_height : {512.0f, 1024.0f})
        {
            RadarMapLayerFixture fixture;
            fixture.AddFloor(0.0f, {{-128.0f, 0.0f}, {0.0f, 256.0f}});
            fixture.AddRamp({0.0f, 128.0f, 0.0f}, {128.0f, 128.0f, 32.0f}, 256.0f);
            fixture.AddRamp({128.0f, 128.0f, 32.0f}, {256.0f, 128.0f, 64.0f}, 256.0f);
            fixture.AddFloor(64.0f, {{256.0f, 0.0f}, {384.0f, 256.0f}});
            fixture.AddFloor(adjacent_height, {{0.0f, 0.0f}, {256.0f, 256.0f}});
            RadarMapSurface detail{};
            detail.surface_id = 5;
            detail.normal = {1.0f, 0.0f, 0.0f};
            detail.vertices = {{{128.0f, 0.0f, 32.0f}}, {{128.0f, 256.0f, 32.0f}}, {{128.0f, 256.0f, 192.0f}}, {{128.0f, 0.0f, 192.0f}}};
            fixture.geometry.surfaces.push_back(std::move(detail));

            RadarMapLayers map;
            ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

            std::string error;
            ASSERT_TRUE(map.ApplyOverrides({{{"selected", -128.0f, 128.0f}, {"adjacent", 128.0f, 2048.0f}}, {}}, error)) << error;
            ASSERT_EQ(map.get_analysis().layers.size(), 2u);
            ASSERT_FLOAT_EQ(map.get_analysis().layers[0].min_height, 0.0f);
            ASSERT_FLOAT_EQ(map.get_analysis().layers[0].max_height, 64.0f);

            std::vector<MapLayerMaterial> materials(6, {7, 8, true});

            for (bool textured : {false, true})
            {
                SCOPED_TRACE(testing::Message() << "textured=" << textured << " adjacent_height=" << adjacent_height);
                fake_.drawn.clear();

                MapLayerRenderer renderer;
                ASSERT_EQ(renderer.Ensure(map, materials, 0, textured, 10), RadarMapStatus::Ready);
                EXPECT_DOUBLE_EQ(fake_.projection[4], -192.0);
                EXPECT_DOUBLE_EQ(fake_.projection[5], 0.0);

                std::array<bool, 4> found{};

                for (const DrawVertex& vertex : fake_.drawn)
                {
                    const std::array<float, 4> heights{0.0f, 32.0f, 64.0f, 192.0f};
                    const std::array<float, 4> textured_colors{0.55f, 0.775f, 1.0f, 1.0f};
                    const std::array<float, 4> flat_green{0.16f, 0.355f, 0.55f, 0.55f};
                    const std::array<float, 4> flat_red_blue{0.1312f, 0.2911f, 0.451f, 0.451f};

                    for (size_t i = 0; i < heights.size(); ++i)
                    {
                        if (vertex.position[2] == heights[i])
                        {
                            found[i] = true;

                            EXPECT_NEAR(vertex.color[0], textured ? textured_colors[i] : flat_red_blue[i], 0.000001f);
                            EXPECT_NEAR(vertex.color[1], textured ? textured_colors[i] : flat_green[i], 0.000001f);
                            EXPECT_NEAR(vertex.color[2], textured ? textured_colors[i] : flat_red_blue[i], 0.000001f);
                            EXPECT_FLOAT_EQ(vertex.color[3], 1.0f);
                            EXPECT_EQ(vertex.shade_model, GL_SMOOTH);
                        }
                    }
                }

                EXPECT_EQ(found, (std::array<bool, 4>{true, true, true, true}));
            }
        }
    }

    TEST_F(MapLayerRender, TexturedShadowLiftPreservesAlphaAndDoesNotAccumulateInTheCache)
    {
        const std::vector<std::array<float, 4>> source{
            {0.0f, 0.0f, 0.0f, 1.0f},
            {0.1f, 0.1f, 0.1f, 1.0f},
            {0.5f, 0.5f, 0.5f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.2f, 0.4f, 0.8f, 1.0f},
            {0.0f, 0.0f, 0.0f, 0.0f}
        };
        const std::vector<std::array<float, 4>> lifted{
            {0.17f, 0.17f, 0.17f, 1.0f},
            {0.253f, 0.253f, 0.253f, 1.0f},
            {0.585f, 0.585f, 0.585f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.336f, 0.502f, 0.834f, 1.0f},
            {0.0f, 0.0f, 0.0f, 0.0f}
        };

        for (bool textured : {false, true})
        {
            SCOPED_TRACE(textured);
            fake_.pixels = source;
            fake_.tone_vertices.clear();
            GlState before = fake_.state;
            MapLayerRenderer renderer;
            ASSERT_EQ(renderer.Ensure(map_, materials_, 0, textured, 10), RadarMapStatus::Ready);
            const std::vector<std::array<float, 4>>& expected = textured ? lifted : source;

            for (size_t pixel = 0; pixel < expected.size(); ++pixel)
            {
                for (size_t channel = 0; channel < 4; ++channel)
                {
                    EXPECT_FLOAT_EQ(fake_.pixels[pixel][channel], expected[pixel][channel]);
                }
            }

            EXPECT_EQ(fake_.state, before);
            const std::vector<std::array<float, 3>> quad{{-1, -1, 0}, {1, -1, 0}, {-1, 1, 0}, {1, 1, 0}};
            std::vector<std::array<float, 3>> passes = quad;

            if (textured)
            {
                passes.insert(passes.end(), quad.begin(), quad.end());
            }

            EXPECT_EQ(fake_.tone_vertices, passes);
            std::vector<std::array<float, 4>> cached = fake_.pixels;
            ASSERT_EQ(renderer.Ensure(map_, materials_, 0, textured, 11), RadarMapStatus::Ready);
            EXPECT_EQ(fake_.pixels, cached);
        }
    }

    TEST_F(MapLayerRender, AllocationFailureReleasesPartialObjects)
    {
        for (int failure = 0; failure < 3; failure++)
        {
            fake_.error = GL_INVALID_VALUE;
            fake_.fail_storage = failure == 0;
            fake_.fail_depth = failure == 1;
            fake_.status = failure == 2 ? GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT : GL_FRAMEBUFFER_COMPLETE;
            GlState before = fake_.state;
            MapLayerRenderer renderer;

            EXPECT_EQ(renderer.Ensure(map_, materials_, 0, true, 10), RadarMapStatus::Failed);

            const char* reasons[] = {
                "texture storage failed (GL error=0x505)", "depth buffer allocation returned zero", "framebuffer incomplete (status=0x8cd6)"
            };

            EXPECT_EQ(renderer.get_failure_reason(), reasons[failure]);
            EXPECT_EQ(renderer.get_map(0), nullptr);
            EXPECT_TRUE(fake_.textures.empty());
            EXPECT_TRUE(fake_.framebuffers.empty());
            EXPECT_TRUE(fake_.renderbuffers.empty());
            EXPECT_EQ(fake_.state, before);
            EXPECT_EQ(fake_.clears, 0);
        }
    }

    TEST_F(MapLayerRender, TextureWaitKeepsCacheAndRetries)
    {
        MapLayerRenderer renderer;
        materials_[0].lightmap_texture = 0;
        std::string reason = "stale";

        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, true, 10, &reason), RadarMapStatus::Waiting);
        EXPECT_TRUE(fake_.textures.empty());
        EXPECT_EQ(
            reason,
            "stage=material_texture layer=0 surface=0 material=0 sky=0 tiled=0 base_texture=7 lightmap_texture=0 "
            "needs_lightmap=1 base_is_texture=1 lightmap_is_texture=0 failed_texture=lightmap"
        );

        EXPECT_EQ(fake_.texture_checks, (std::vector<GLuint>{7, 0}));

        materials_[0].lightmap_texture = 8;
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, true, 10, &reason), RadarMapStatus::Ready);
        EXPECT_TRUE(reason.empty());
        EXPECT_EQ(fake_.clears, 1);
    }

    TEST_F(MapLayerRender, ContextTextureWaitDoesNotConsumeFrameBudget)
    {
        MapLayerRenderer renderer;
        materials_[3].lightmap_texture = 0;
        std::string reason;
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, true, 10, &reason), RadarMapStatus::Waiting);
        EXPECT_NE(reason.find("layer=3 surface=3"), std::string::npos);
        EXPECT_TRUE(fake_.textures.empty());
        EXPECT_EQ(fake_.image_count, 0);
        materials_[3].lightmap_texture = 8;
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, true, 10, &reason), RadarMapStatus::Ready);
        EXPECT_TRUE(reason.empty());
        EXPECT_EQ(fake_.image_count, 1);
        EXPECT_EQ(fake_.clears, 1);
    }

    TEST_F(MapLayerRender, BaseTextureWaitIsOwnedAndDumpPerformsNoQueries)
    {
        MapLayerRenderer renderer;
        materials_[0].base_texture = 0;
        std::string reason;

        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, true, 10, &reason), RadarMapStatus::Waiting);
        EXPECT_EQ(
            reason,
            "stage=material_texture layer=0 surface=0 material=0 sky=0 tiled=0 base_texture=0 lightmap_texture=8 "
            "needs_lightmap=1 base_is_texture=0 lightmap_is_texture=unchecked failed_texture=base"
        );

        EXPECT_EQ(fake_.texture_checks, (std::vector<GLuint>{0}));

        std::string dump = RadarLayersDump_Format(&map_, nullptr, &renderer.get_cache(), "test", {}, 1.0f, nullptr, nullptr, &reason);
        EXPECT_NE(dump.find("image_waiting_reason=\"" + reason + "\""), std::string::npos);
        EXPECT_EQ(RadarLayersDump_Format(&map_, nullptr, &renderer.get_cache(), "test", {}, 1.0f, nullptr, nullptr, &reason), dump);
        EXPECT_EQ(fake_.texture_checks, (std::vector<GLuint>{0}));
        EXPECT_EQ(fake_.error_reads, 0);

        materials_[0].base_texture = 7;
        ASSERT_EQ(renderer.Ensure(map_, materials_, 0, true, 10, &reason), RadarMapStatus::Ready);
        EXPECT_TRUE(reason.empty());

        size_t queries = fake_.texture_checks.size();
        reason = "stale";
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, true, 10, &reason), RadarMapStatus::Ready);
        EXPECT_TRUE(reason.empty());
        EXPECT_EQ(fake_.texture_checks.size(), queries);
    }

    TEST_F(MapLayerRender, SkyAndCeilingWithAbsentMaterialsDoNotBlockTexturedFloor)
    {
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
        fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
        fixture.geometry.surfaces.back().sky = true;
        fixture.geometry.surfaces.back().tiled = true;
        fixture.AddFloor(128.0f, {{0.0f, 0.0f}, {128.0f, 128.0f}});
        fixture.geometry.surfaces.back().normal = {0.0f, 0.0f, -1.0f};
        ASSERT_EQ(map_.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        ASSERT_EQ(map_.get_layers().size(), 1u);

        materials_.assign(3, {0, 0, true});
        materials_[0] = {7, 8, true};

        MapLayerRenderer renderer;
        std::string reason;

        ASSERT_EQ(renderer.Ensure(map_, materials_, 0, true, 10, &reason), RadarMapStatus::Ready);
        EXPECT_TRUE(reason.empty());
        ASSERT_FALSE(fake_.texture_checks.empty());
        EXPECT_TRUE(std::all_of(fake_.texture_checks.begin(), fake_.texture_checks.end(), [](GLuint texture) {
            return texture == 7 || texture == 8;
        }));

        ASSERT_FALSE(fake_.drawn.empty());
        EXPECT_TRUE(std::all_of(fake_.drawn.begin(), fake_.drawn.end(), [](const DrawVertex& vertex) {
            return vertex.position[2] == 0.0f;
        }));

        EXPECT_EQ(fake_.clears, 1);
    }

    TEST_F(MapLayerRender, MissingMaterialAndEmptyMeshHavePreciseWaitReasons)
    {
        MapLayerRenderer renderer;
        std::string reason;

        EXPECT_EQ(renderer.Ensure(map_, {}, 0, true, 10, &reason), RadarMapStatus::Waiting);
        EXPECT_EQ(reason, "stage=material_index layer=0 surface=0 material_count=0");

        EXPECT_EQ(renderer.Ensure(map_, materials_, -1, true, 10, &reason), RadarMapStatus::Waiting);
        EXPECT_EQ(reason, "stage=layer_mesh layer=-1 surfaces=0");
        EXPECT_TRUE(fake_.texture_checks.empty());
        EXPECT_EQ(fake_.image_count, 0);
    }

    TEST_F(MapLayerRender, ThreeImagesShareOneDepthTargetAndFrameBudget)
    {
        MapLayerRenderer renderer;

        for (int layer = 0; layer < 4; layer++)
        {
            EXPECT_EQ(renderer.Ensure(map_, materials_, layer, false, layer + 10), RadarMapStatus::Ready);
            ASSERT_NE(renderer.get_map(layer), nullptr);
        }

        EXPECT_EQ(fake_.textures.size(), 3u);
        EXPECT_EQ(fake_.framebuffers.size(), 1u);
        EXPECT_EQ(fake_.renderbuffers.size(), 1u);
        EXPECT_EQ(renderer.get_map(0), nullptr);

        std::string reason;
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, false, 13, &reason), RadarMapStatus::Waiting);
        EXPECT_EQ(reason, "stage=image_budget layer=0 frame=13");
        EXPECT_EQ(fake_.clears, 4);

        EXPECT_EQ(renderer.Ensure(map_, materials_, 3, false, 13, &reason), RadarMapStatus::Ready);
        EXPECT_TRUE(reason.empty());
    }

    TEST_F(MapLayerRender, FailureWaitsForReset)
    {
        MapLayerRenderer renderer;
        fake_.status = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, false, 10), RadarMapStatus::Failed);

        int allocations = fake_.image_count;
        int error_reads = fake_.error_reads;
        std::string reason = renderer.get_failure_reason();

        EXPECT_EQ(reason, "framebuffer incomplete (status=0x8cd6)");

        RadarLayerLifecycle lifecycle;
        lifecycle.SetGeometryStatus(RadarMapStatus::Ready);
        ASSERT_TRUE(lifecycle.SetImageStatus(RadarMapStatus::Failed, reason));

        std::string dump = RadarLayersDump_Format(&map_, nullptr, &renderer.get_cache(), "test", {}, 1.0f, &lifecycle);
        EXPECT_EQ(RadarLayersDump_Format(&map_, nullptr, &renderer.get_cache(), "test", {}, 1.0f, &lifecycle), dump);
        EXPECT_NE(dump.find(reason), std::string::npos);
        EXPECT_EQ(fake_.error_reads, error_reads);

        fake_.status = GL_FRAMEBUFFER_COMPLETE;
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, false, 11), RadarMapStatus::Failed);
        EXPECT_EQ(fake_.image_count, allocations);
        EXPECT_EQ(fake_.error_reads, error_reads);
        EXPECT_EQ(renderer.get_failure_reason(), reason);

        EXPECT_FALSE(lifecycle.SetImageStatus(RadarMapStatus::Failed, renderer.get_failure_reason()));

        renderer.Reset();

        EXPECT_TRUE(renderer.get_failure_reason().empty());

        lifecycle.ResetImage();
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, false, 12), RadarMapStatus::Ready);
    }

    TEST_F(MapLayerRender, MissingFramebufferEntryPointRetainsReasonWithoutGlQueries)
    {
        Bind(glad_glGenFramebuffers, static_cast<PFNGLGENFRAMEBUFFERSPROC>(nullptr));

        MapLayerRenderer renderer;
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, false, 10), RadarMapStatus::Failed);
        EXPECT_EQ(renderer.get_failure_reason(), "glGenFramebuffers unavailable");

        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, false, 11), RadarMapStatus::Failed);
        EXPECT_EQ(fake_.error_reads, 0);
        EXPECT_EQ(fake_.image_count, 0);
    }

    TEST_F(MapLayerRender, DrawFailureRetainsExactErrorAndRestoresState)
    {
        GlState before = fake_.state;
        fake_.fail_draw = true;

        MapLayerRenderer renderer;
        EXPECT_EQ(renderer.Ensure(map_, materials_, 0, false, 10), RadarMapStatus::Failed);
        EXPECT_EQ(renderer.get_failure_reason(), "layer mesh rendering failed (GL error=0x502)");
        EXPECT_EQ(fake_.state, before);
        EXPECT_TRUE(fake_.textures.empty());
        EXPECT_TRUE(fake_.framebuffers.empty());
        EXPECT_TRUE(fake_.renderbuffers.empty());
    }

    TEST_F(MapLayerRender, ModeResetDropsImagesAndDestructorReleasesObjects)
    {
        GlState before = fake_.state;
        {
            MapLayerRenderer renderer;
            ASSERT_EQ(renderer.Ensure(map_, materials_, 0, true, 10), RadarMapStatus::Ready);
            EXPECT_EQ(renderer.Ensure(map_, materials_, 1, false, 11), RadarMapStatus::Ready);
            EXPECT_EQ(renderer.get_map(0), nullptr);
            EXPECT_EQ(fake_.textures.size(), 1u);
        }

        EXPECT_TRUE(fake_.textures.empty());
        EXPECT_TRUE(fake_.framebuffers.empty());
        EXPECT_TRUE(fake_.renderbuffers.empty());
        EXPECT_EQ(fake_.state, before);
    }

    TEST_F(MapLayerRender, ProjectionAndRuntimeBoundsUseCommonVisibleLayerUnion)
    {
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
        fixture.AddFloor(256.0f, {{128.0f, -128.0f}, {640.0f, 384.0f}});
        fixture.geometry.bounds = {{-2048.0f, -2048.0f}, {2048.0f, 2048.0f}};
        ASSERT_EQ(map_.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        RadarLayerOverrides overrides;
        overrides.hides = {{{-1.0f, -256.0f, -1.0f}, {128.0f, 1024.0f, 1.0f}}, {{512.0f, -256.0f, 255.0f}, {1024.0f, 1024.0f, 257.0f}}};
        std::string error;
        ASSERT_TRUE(map_.ApplyOverrides(overrides, error)) << error;
        ASSERT_EQ(map_.get_layers().size(), 2u);
        MapLayerRenderer renderer;

        for (int layer = 0; layer < 2; layer++)
        {
            ASSERT_EQ(renderer.Ensure(map_, {}, layer, false, layer + 10), RadarMapStatus::Ready);
            ASSERT_NE(renderer.get_map(layer), nullptr);
            const RuntimeMap& image = *renderer.get_map(layer);
            EXPECT_EQ(image.bounds.min, (ncl_math::Vector2{128.0f, -128.0f}));
            EXPECT_EQ(image.bounds.max, (ncl_math::Vector2{512.0f, 512.0f}));
            EXPECT_DOUBLE_EQ(fake_.projection[0], 128.0);
            EXPECT_DOUBLE_EQ(fake_.projection[1], 512.0);
            EXPECT_DOUBLE_EQ(fake_.projection[2], -128.0);
            EXPECT_DOUBLE_EQ(fake_.projection[3], 512.0);
        }

        int allocations = fake_.image_count;
        ASSERT_EQ(renderer.Ensure(map_, {}, 0, false, 12), RadarMapStatus::Ready);
        EXPECT_EQ(fake_.image_count, allocations);
        EXPECT_EQ(renderer.get_map(0)->bounds.max, (ncl_math::Vector2{512.0f, 512.0f}));
    }
} // namespace
