#include <gtest/gtest.h>

#include <vector>

#include <goldsrc/client_api.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/triangle_api.h>

#include "graphics/render/StudioIntercept.h"
#include "graphics/render/RendererLifecycle.h"

extern goldsrc::cl_enginefunc_t gEngfuncs;

namespace
{
    std::vector<int> g_InterceptEvents;

    void OriginalDraw()
    {
        g_InterceptEvents.push_back(1);
    }

    void OriginalRestore()
    {
        g_InterceptEvents.push_back(2);
    }

    void OriginalDebug()
    {
        g_InterceptEvents.push_back(3);
    }

    void OriginalShadow(int, float*, float*, float*, float*)
    {
        g_InterceptEvents.push_back(4);
    }

    void OriginalBegin(int primitive)
    {
        g_InterceptEvents.push_back(primitive);
    }

    void ForeignDebug()
    {
        g_InterceptEvents.push_back(5);
    }

    class StudioInterceptTest : public testing::Test
    {
    protected:
        goldsrc::engine_studio_api_t api_{};
        goldsrc::triangleapi_t triangles_{};
        goldsrc::triangleapi_t* saved_triangles_{};

        void SetUp() override
        {
            g_InterceptEvents.clear();
            api_.StudioDrawPoints = OriginalDraw;
            api_.RestoreRenderer = OriginalRestore;
            api_.StudioDrawBones = OriginalDebug;
            api_.StudioDrawHulls = OriginalDebug;
            api_.StudioDrawAbsBBox = OriginalDebug;
            api_.StudioRenderShadow = OriginalShadow;
            api_.GL_StudioDrawShadow = OriginalDebug;
            triangles_.Begin = OriginalBegin;
            saved_triangles_ = gEngfuncs.pTriAPI;
            gEngfuncs.pTriAPI = &triangles_;
            render::Studio_PatchApi(&api_);
        }

        void TearDown() override
        {
            render::Studio_Shutdown();
            gEngfuncs.pTriAPI = saved_triangles_;
        }
    };
} // namespace

TEST_F(StudioInterceptTest, PatchesOnceAndRestoresExtendedApiAndTriangleTable)
{
    goldsrc::engine_studio_api_t copied = api_;
    render::Studio_PatchApi(&api_);
    EXPECT_EQ(api_.StudioDrawPoints, copied.StudioDrawPoints);
    EXPECT_NE(api_.StudioDrawBones, OriginalDebug);
    EXPECT_NE(triangles_.Begin, OriginalBegin);
    api_.StudioDrawBones();
    api_.StudioDrawHulls();
    api_.StudioDrawAbsBBox();
    api_.StudioRenderShadow(0, nullptr, nullptr, nullptr, nullptr);
    api_.RestoreRenderer();
    triangles_.Begin(7);
    EXPECT_EQ(g_InterceptEvents, (std::vector<int>{3, 3, 3, 4, 2, 7}));

    render::Studio_Shutdown();
    EXPECT_EQ(api_.StudioDrawPoints, OriginalDraw);
    EXPECT_EQ(api_.RestoreRenderer, OriginalRestore);
    EXPECT_EQ(api_.StudioDrawBones, OriginalDebug);
    EXPECT_EQ(api_.StudioDrawHulls, OriginalDebug);
    EXPECT_EQ(api_.StudioDrawAbsBBox, OriginalDebug);
    EXPECT_EQ(api_.StudioRenderShadow, OriginalShadow);
    EXPECT_EQ(triangles_.Begin, OriginalBegin);
}

TEST_F(StudioInterceptTest, CopiedEntryPointsRemainCallableAfterShutdown)
{
    goldsrc::engine_studio_api_t copied = api_;
    void (*copied_begin)(int) = triangles_.Begin;
    render::Studio_Shutdown();
    copied.StudioDrawPoints();
    copied.StudioDrawBones();
    copied.StudioRenderShadow(0, nullptr, nullptr, nullptr, nullptr);
    copied.RestoreRenderer();
    copied.GL_StudioDrawShadow();
    copied_begin(8);
    EXPECT_EQ(g_InterceptEvents, (std::vector<int>{1, 3, 4, 2, 3, 8}));
}

TEST_F(StudioInterceptTest, ShutdownPreservesAnotherOwnersReplacement)
{
    api_.StudioDrawBones = ForeignDebug;
    render::Studio_Shutdown();
    EXPECT_EQ(api_.StudioDrawBones, ForeignDebug);
    api_.StudioDrawBones();
    EXPECT_EQ(g_InterceptEvents, (std::vector<int>{5}));
}
