#include <array>
#include <string>
#include <utility>
#include <vector>

#include <glad/glad.h>
#include <goldsrc/format/bsp.h>
#include <gtest/gtest.h>

#include "RadarGlContext.h"
#include "RadarGlAllocations.h"
#include "RadarMapLayerFixtures.h"
#include "hud/radar/map/MapRender.h"
#include "hud/radar/integration/Performance.h"

namespace
{
    struct LiveMapRelease
    {
        MapLayerRenderer& renderer;
        ~LiveMapRelease()
        {
            renderer.Reset();
        }
    };

    class MapLayerRenderGlTest : public testing::TestWithParam<bool>
    {
    protected:
        RadarGlContext context_;
        bool context_ready_{};
        std::array<GLuint, 4> textures_{};

    protected:
        radar::integration::Performance performance_{nullptr};
        RadarMapLayers map_;
        std::vector<MapLayerMaterial> materials_;

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            context_ready_ = true;
            RecordProperty("renderer", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

            RadarMapLayerFixture fixture;
            fixture.AddFloor(0.0f, {{0, 512}, {1024, 768}});
            fixture.AddFloor(0.0f, {{0, 0}, {512, 512}});
            fixture.AddFloor(256.0f, {{0, 0}, {512, 1024}});
            fixture.AddFloor(512.0f, {{0, 0}, {768, 512}});
            ASSERT_EQ(map_.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
            ASSERT_TRUE(map_.ApplyOverrides({{{"lower", -128, 128}, {"middle", 128, 384}, {"upper", 384, 640}}, {}}, error)) << error;

            glGenTextures((GLsizei)textures_.size(), textures_.data());
            const std::array<std::array<unsigned char, 4>, 4> colors{
                {{255, 0, 0, 255}, {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}}
            };
            for (size_t i = 0; i < textures_.size(); ++i)
            {
                glBindTexture(GL_TEXTURE_2D, textures_[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors[i].data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                materials_.push_back({textures_[i], 0, false});
            }
            glDisable(GL_DITHER);
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
        }

        void TearDown() override
        {
            if (context_ready_)
            {
                glDeleteTextures((GLsizei)textures_.size(), textures_.data());
            }
        }

        std::array<unsigned char, 4> Pixel(const RuntimeMap& image, int x, int y)
        {
            glBindTexture(GL_TEXTURE_2D, image.texture);
            std::vector<unsigned char> pixels(kMapImageSize * kMapImageSize * 4);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            size_t offset = ((size_t)y * kMapImageSize + x) * 4;
            return {pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
        }
    };

    TEST_P(MapLayerRenderGlTest, FillsOnlyEmptyRegionsAndDimsOnlyLowerFloors)
    {
        MapLayerRenderer renderer{performance_};
        LiveMapRelease renderer_release{renderer};
        ASSERT_EQ(renderer.Ensure(map_, materials_, 1, GetParam(), 1), RadarMapStatus::Ready);
        const RuntimeMap* middle = renderer.get_map(1);
        ASSERT_NE(middle, nullptr);
        std::array<unsigned char, 4> active = Pixel(*middle, 100, 100);
        std::array<unsigned char, 4> lower = Pixel(*middle, 650, 650);
        std::array<unsigned char, 4> upper = Pixel(*middle, 650, 100);
        EXPECT_EQ(active[3], 255);
        EXPECT_EQ(lower[3], 255);
        EXPECT_EQ(upper[3], 255);
        EXPECT_EQ(Pixel(*middle, 900, 900), (std::array<unsigned char, 4>{0, 0, 0, 0}));

        ASSERT_EQ(renderer.Ensure(map_, materials_, 0, GetParam(), 2), RadarMapStatus::Ready);
        std::array<unsigned char, 4> lower_active = Pixel(*renderer.get_map(0), 650, 650);
        EXPECT_LT(lower[0], lower_active[0]);
        EXPECT_LT(lower[1] + lower[0] + lower[2], lower_active[1] + lower_active[0] + lower_active[2]);

        ASSERT_EQ(renderer.Ensure(map_, materials_, 2, GetParam(), 3), RadarMapStatus::Ready);
        EXPECT_EQ(upper, Pixel(*renderer.get_map(2), 650, 100));
        if (GetParam())
        {
            EXPECT_GT(active[1], active[0]);
            EXPECT_GT(active[1], active[2]);
            EXPECT_GT(lower[0], lower[1]);
            EXPECT_GT(upper[2], upper[0]);
        }
    }

    TEST_P(MapLayerRenderGlTest, NearestFloorFillsOverlappingContext)
    {
        for (float middle_height : {200.0f, 400.0f})
        {
            RadarMapLayerFixture fixture;
            fixture.AddFloor(0.0f, {{0, 0}, {1024, 768}});
            fixture.AddFloor(middle_height, {{0, 0}, {512, 1024}});
            fixture.AddFloor(600.0f, {{0, 0}, {768, 512}});
            RadarMapLayers map;
            ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
            std::string error;
            ASSERT_TRUE(map.ApplyOverrides({{{"lower", -128, 128}, {"middle", 128, 500}, {"upper", 500, 800}}, {}}, error)) << error;
            std::vector<MapLayerMaterial> materials{materials_[0], materials_[2], materials_[3]};
            MapLayerRenderer renderer{performance_};
            LiveMapRelease renderer_release{renderer};
            ASSERT_EQ(renderer.Ensure(map, materials, 1, GetParam(), 1), RadarMapStatus::Ready);
            std::array<unsigned char, 4> context = Pixel(*renderer.get_map(1), 650, 100);
            EXPECT_EQ(context[3], 255);
            int nearest = middle_height < 300.0f ? 0 : 2;
            ASSERT_EQ(renderer.Ensure(map, materials, nearest, GetParam(), 2), RadarMapStatus::Ready);
            std::array<unsigned char, 4> nearest_active = Pixel(*renderer.get_map(nearest), 650, 100);
            if (nearest == 0)
            {
                EXPECT_LT(context[0], nearest_active[0]);
            }
            else
            {
                EXPECT_EQ(context, nearest_active);
            }
            if (GetParam())
            {
                EXPECT_GT(context[nearest], context[2 - nearest]);
            }
        }
    }

    TEST_P(MapLayerRenderGlTest, HiddenActiveMeshStillShowsUnhiddenContext)
    {
        RadarLayerOverrides overrides{
            {{"lower", -128, 128}, {"middle", 128, 384}, {"upper", 384, 640}}, {}, {{{-1, -1, 128}, {1025, 1025, 384}}}
        };
        std::string error;
        ASSERT_TRUE(map_.ApplyOverrides(overrides, error)) << error;
        ASSERT_TRUE(map_.get_surfaces(1).empty());
        MapLayerRenderer renderer{performance_};
        LiveMapRelease renderer_release{renderer};
        ASSERT_EQ(renderer.Ensure(map_, materials_, 1, GetParam(), 1), RadarMapStatus::Ready);
        EXPECT_EQ(Pixel(*renderer.get_map(1), 100, 100)[3], 255);
        EXPECT_EQ(Pixel(*renderer.get_map(1), 900, 100)[3], 0);
    }

    TEST_P(MapLayerRenderGlTest, MissingEntryPointsDoNotPaintBlockedRoofOverGround)
    {
        RadarMapLayerFixture fixture;
        fixture.AddFloor(0.0f, {{0, 0}, {512, 512}});
        fixture.AddFloor(96.0f, {{0, 0}, {512, 512}});
        fixture.geometry.collision_hulls.push_back({
            {{{0.0f, 0.0f, 1.0f}, 46.0f}},
            {{0, {goldsrc::CONTENTS_SOLID, goldsrc::CONTENTS_EMPTY}}},
            0,
            {},
        });

        RadarMapLayers map;
        ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
        std::vector<MapLayerMaterial> materials{materials_[0], materials_[3]};
        MapLayerRenderer renderer{performance_};
        LiveMapRelease renderer_release{renderer};
        ASSERT_EQ(renderer.Ensure(map, materials, 0, GetParam(), 1), RadarMapStatus::Ready);
        std::array<unsigned char, 4> actual = Pixel(*renderer.get_map(0), 500, 500);

        RadarMapLayerFixture ground_fixture;
        ground_fixture.AddFloor(0.0f, {{0, 0}, {512, 512}});
        ground_fixture.AddSupportHull(0);

        RadarMapLayers ground;
        ASSERT_EQ(ground.Build(std::move(ground_fixture.geometry)), RadarMapStatus::Ready);
        MapLayerRenderer ground_renderer{performance_};
        LiveMapRelease ground_renderer_release{ground_renderer};
        ASSERT_EQ(ground_renderer.Ensure(ground, materials, 0, GetParam(), 1), RadarMapStatus::Ready);
        std::array<unsigned char, 4> expected = Pixel(*ground_renderer.get_map(0), 500, 500);

        ASSERT_EQ(expected[3], 255);
        EXPECT_EQ(actual, expected);
    }

    INSTANTIATE_TEST_SUITE_P(MapModes, MapLayerRenderGlTest, testing::Bool());
} // namespace

TEST_P(MapLayerRenderGlTest, ExplicitResetDeletesOwnedImagesTargetsAndAllowsRecovery)
{
    MapLayerRenderer renderer{performance_};
    RadarGlObjects names;
    {
        RadarGlAllocations observe;
        ASSERT_EQ(renderer.Ensure(map_, materials_, 0, GetParam(), 1), RadarMapStatus::Ready);
        names = observe.objects;
    }
    ASSERT_EQ(names.textures.size(), 1u);
    ASSERT_EQ(names.framebuffers.size(), 1u);
    ASSERT_EQ(names.renderbuffers.size(), 1u);
    renderer.Reset(true);
    EXPECT_EQ(glIsTexture(names.textures[0]), GL_FALSE);
    EXPECT_EQ(glIsFramebuffer(names.framebuffers[0]), GL_FALSE);
    EXPECT_EQ(glIsRenderbuffer(names.renderbuffers[0]), GL_FALSE);
    EXPECT_EQ(glIsTexture(textures_[0]), GL_TRUE);
    EXPECT_FALSE(renderer.get_map(0));
    ASSERT_EQ(renderer.Ensure(map_, materials_, 0, GetParam(), 2), RadarMapStatus::Ready);
    renderer.Reset(true);
}

TEST_P(MapLayerRenderGlTest, ForeignAbandonPreservesOverlappingImageAndTargets)
{
    MapLayerRenderer renderer{performance_};
    RadarGlObjects names;
    {
        RadarGlAllocations observe;
        ASSERT_EQ(renderer.Ensure(map_, materials_, 0, GetParam(), 1), RadarMapStatus::Ready);
        names = observe.objects;
    }
    ASSERT_EQ(names.textures.size(), 1u);
    ASSERT_EQ(names.framebuffers.size(), 1u);
    ASSERT_EQ(names.renderbuffers.size(), 1u);
    RadarGlContext foreign;
    std::string error;
    ASSERT_TRUE(foreign.Initialize(error)) << error;
    ASSERT_NE(RadarGlContext::Current(), context_.native_context());
    std::vector<GLuint> framebuffers;
    std::vector<GLuint> renderbuffers;
    for (GLuint name = 0; name < names.framebuffers[0];)
    {
        glGenFramebuffers(1, &name);
        framebuffers.push_back(name);
        ASSERT_LT(framebuffers.size(), 128u);
    }
    for (GLuint name = 0; name < names.renderbuffers[0];)
    {
        glGenRenderbuffers(1, &name);
        renderbuffers.push_back(name);
        ASSERT_LT(renderbuffers.size(), 128u);
    }
    ASSERT_EQ(framebuffers.back(), names.framebuffers[0]);
    ASSERT_EQ(renderbuffers.back(), names.renderbuffers[0]);
    const std::array<unsigned char, 4> color{11, 83, 157, 255};
    glBindTexture(GL_TEXTURE_2D, names.textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
    glBindFramebuffer(GL_FRAMEBUFFER, names.framebuffers[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, names.textures[0], 0);
    glBindRenderbuffer(GL_RENDERBUFFER, names.renderbuffers[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, names.renderbuffers[0]);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    ASSERT_EQ(glGetError(), GL_NO_ERROR);
    {
        RadarGlAllocations observe;
        renderer.Reset(false);
        EXPECT_EQ(observe.delete_calls, 0u);
    }
    EXPECT_FALSE(renderer.get_map(0));
    EXPECT_EQ(glIsTexture(names.textures[0]), GL_TRUE);
    EXPECT_EQ(glIsFramebuffer(names.framebuffers[0]), GL_TRUE);
    EXPECT_EQ(glIsRenderbuffer(names.renderbuffers[0]), GL_TRUE);
    EXPECT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    std::array<unsigned char, 4> pixel{};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, color);
    names.Release();
    glDeleteFramebuffers(static_cast<GLsizei>(framebuffers.size()), framebuffers.data());
    glDeleteRenderbuffers(static_cast<GLsizei>(renderbuffers.size()), renderbuffers.data());
    ASSERT_TRUE(context_.MakeCurrent());
    names.Release();
    ASSERT_EQ(renderer.Ensure(map_, materials_, 0, GetParam(), 2), RadarMapStatus::Ready);
    renderer.Reset(true);
}

TEST_P(MapLayerRenderGlTest, NoCurrentResetAndPassiveDestructorDoNotDeleteImages)
{
    auto renderer = std::make_unique<MapLayerRenderer>(performance_);
    RadarGlObjects names;
    {
        RadarGlAllocations observe;
        ASSERT_EQ(renderer->Ensure(map_, materials_, 0, GetParam(), 1), RadarMapStatus::Ready);
        names = observe.objects;
    }
    ASSERT_TRUE(context_.ClearCurrent());
    {
        RadarGlAllocations observe;
        renderer->Reset(false);
        renderer.reset();
        EXPECT_EQ(observe.delete_calls, 0u);
    }
    ASSERT_TRUE(context_.MakeCurrent());
    EXPECT_EQ(glIsTexture(names.textures[0]), GL_TRUE);
    names.Release();
    renderer = std::make_unique<MapLayerRenderer>(performance_);
    {
        RadarGlAllocations observe;
        ASSERT_EQ(renderer->Ensure(map_, materials_, 0, GetParam(), 2), RadarMapStatus::Ready);
        names = observe.objects;
        unsigned int deletes = observe.delete_calls;
        renderer.reset();
        EXPECT_EQ(observe.delete_calls, deletes);
    }
    EXPECT_EQ(glIsTexture(names.textures[0]), GL_TRUE);
    names.Release();
}
