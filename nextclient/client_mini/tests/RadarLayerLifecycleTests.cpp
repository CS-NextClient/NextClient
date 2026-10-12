#include <gtest/gtest.h>

#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/map/Overview.h"

TEST(RadarLayerLifecycle, LayeredModesUseMatchingTopPresentation)
{
    EXPECT_EQ(RadarLayerContext_TopMode(RadarMapMode::SliceTextured), RadarMapMode::RuntimeTextured);
    EXPECT_EQ(RadarLayerContext_TopMode(RadarMapMode::SliceFlat), RadarMapMode::RuntimeFlat);
    EXPECT_EQ(RadarLayerContext_TopMode(RadarMapMode::OverviewTextured), RadarMapMode::OverviewTextured);
}

TEST(RadarLayerLifecycle, MarkersUseLayersOnlyWhileTheSelectedImageIsReady)
{
    RadarLayerLifecycle lifecycle;
    EXPECT_FALSE(lifecycle.is_image_ready());

    lifecycle.SetGeometryStatus(RadarMapStatus::Ready);

    EXPECT_TRUE(lifecycle.should_render_image());
    EXPECT_FALSE(lifecycle.is_image_ready());

    lifecycle.SetImageStatus(RadarMapStatus::Ready);

    EXPECT_TRUE(lifecycle.is_image_ready());

    lifecycle.SetImageStatus(RadarMapStatus::Waiting);

    EXPECT_FALSE(lifecycle.is_image_ready());
    EXPECT_TRUE(lifecycle.is_layout_ready());

    lifecycle.SetImageStatus(RadarMapStatus::Ready);

    EXPECT_TRUE(lifecycle.is_image_ready());

    lifecycle.SetImageStatus(RadarMapStatus::Failed);

    EXPECT_FALSE(lifecycle.is_image_ready());
    EXPECT_FALSE(lifecycle.should_render_image());
    EXPECT_TRUE(lifecycle.should_use_fallback());

    lifecycle.Reset();

    EXPECT_FALSE(lifecycle.is_image_ready());
}

TEST(RadarLayerLifecycle, TextureWaitKeepsGeometryReady)
{
    RadarLayerLifecycle lifecycle;
    lifecycle.SetGeometryStatus(RadarMapStatus::Ready);
    lifecycle.SetImageStatus(RadarMapStatus::Waiting);

    EXPECT_TRUE(lifecycle.is_layout_ready());
    EXPECT_FALSE(lifecycle.should_build_geometry());
    EXPECT_TRUE(lifecycle.should_render_image());
    EXPECT_FALSE(lifecycle.should_use_fallback());
}

TEST(RadarLayerLifecycle, ResetRequiresRebuild)
{
    RadarLayerLifecycle lifecycle;
    lifecycle.SetGeometryStatus(RadarMapStatus::Ready);
    lifecycle.SetImageStatus(RadarMapStatus::Ready);
    lifecycle.Reset();

    EXPECT_FALSE(lifecycle.is_layout_ready());
    EXPECT_TRUE(lifecycle.should_build_geometry());
    EXPECT_FALSE(lifecycle.should_render_image());
    EXPECT_FALSE(lifecycle.should_use_fallback());
}

TEST(RadarLayerLifecycle, FailedImageDoesNotRetryEveryFrame)
{
    RadarLayerLifecycle lifecycle;
    lifecycle.SetGeometryStatus(RadarMapStatus::Ready);
    lifecycle.SetImageStatus(RadarMapStatus::Failed);

    EXPECT_FALSE(lifecycle.should_render_image());
    EXPECT_TRUE(lifecycle.should_use_fallback());
}

TEST(RadarLayerLifecycle, ImageResetKeepsGeometryReady)
{
    RadarLayerLifecycle lifecycle;
    lifecycle.SetGeometryStatus(RadarMapStatus::Ready);
    lifecycle.SetImageStatus(RadarMapStatus::Failed);
    lifecycle.ResetImage();

    EXPECT_TRUE(lifecycle.is_layout_ready());
    EXPECT_FALSE(lifecycle.should_build_geometry());
    EXPECT_TRUE(lifecycle.should_render_image());
    EXPECT_FALSE(lifecycle.should_use_fallback());
}

TEST(RadarLayerLifecycle, FailedGeometryWaitsForInvalidation)
{
    RadarLayerLifecycle lifecycle;
    lifecycle.SetGeometryStatus(RadarMapStatus::Failed);

    EXPECT_FALSE(lifecycle.should_build_geometry());
    EXPECT_FALSE(lifecycle.should_render_image());
    EXPECT_TRUE(lifecycle.should_use_fallback());

    lifecycle.Reset();

    EXPECT_TRUE(lifecycle.should_build_geometry());
}

TEST(RadarLayerLifecycle, NoSupportFailureNotifiesOnceUntilMapReset)
{
    RadarMapLayers map;
    ASSERT_EQ(map.Build(RadarMapGeometry{}), RadarMapStatus::Failed);

    RadarLayerLifecycle lifecycle;
    EXPECT_TRUE(lifecycle.SetGeometryStatus(map.get_analysis().status));
    EXPECT_FALSE(lifecycle.SetGeometryStatus(RadarMapStatus::Failed));
    EXPECT_FALSE(lifecycle.SetGeometryStatus(RadarMapStatus::Waiting));
    EXPECT_EQ(lifecycle.get_geometry_status(), RadarMapStatus::Failed);
    EXPECT_EQ(lifecycle.get_failure_reason(), "analysis produced no usable support layers");

    lifecycle.ResetImage();
    EXPECT_FALSE(lifecycle.SetGeometryStatus(RadarMapStatus::Failed));
    EXPECT_TRUE(lifecycle.should_use_fallback());

    lifecycle.Reset();

    EXPECT_TRUE(lifecycle.get_failure_reason().empty());

    EXPECT_TRUE(lifecycle.SetGeometryStatus(RadarMapStatus::Failed));
}

TEST(RadarLayerLifecycle, ImageFailureRetainsReasonAndNotificationUntilResourceReset)
{
    RadarLayerLifecycle lifecycle;
    EXPECT_FALSE(lifecycle.SetGeometryStatus(RadarMapStatus::Ready));

    std::string reason = "texture storage failed (GL error=0x505)";
    EXPECT_TRUE(lifecycle.SetImageStatus(RadarMapStatus::Failed, reason));

    reason = "modified producer";

    RadarLayerContext context;
    context.Reset();
    EXPECT_FALSE(lifecycle.SetImageStatus(RadarMapStatus::Failed, "later failure"));
    EXPECT_FALSE(lifecycle.SetImageStatus(RadarMapStatus::Waiting));
    EXPECT_EQ(lifecycle.get_image_status(), RadarMapStatus::Failed);
    EXPECT_EQ(lifecycle.get_failure_reason(), "texture storage failed (GL error=0x505)");
    EXPECT_FALSE(lifecycle.should_render_image());

    lifecycle.ResetImage();

    EXPECT_TRUE(lifecycle.get_failure_reason().empty());
    EXPECT_EQ(lifecycle.get_image_status(), RadarMapStatus::Waiting);

    EXPECT_TRUE(lifecycle.SetImageStatus(RadarMapStatus::Failed, "framebuffer incomplete"));

    lifecycle.Reset();

    EXPECT_FALSE(lifecycle.should_use_fallback());
    EXPECT_TRUE(lifecycle.get_failure_reason().empty());
}

TEST(RadarLayerLifecycle, WaitingDoesNotNotifyFailure)
{
    RadarLayerLifecycle lifecycle;
    EXPECT_FALSE(lifecycle.SetGeometryStatus(RadarMapStatus::Waiting));
    EXPECT_FALSE(lifecycle.SetGeometryStatus(RadarMapStatus::Ready));
    EXPECT_FALSE(lifecycle.SetImageStatus(RadarMapStatus::Waiting));
    EXPECT_EQ(lifecycle.get_image_status(), RadarMapStatus::Waiting);
    EXPECT_TRUE(lifecycle.get_failure_reason().empty());
    EXPECT_FALSE(lifecycle.should_use_fallback());
}

TEST(RadarMapMode, GeneratedChoicesUseCurrentLayers)
{
    EXPECT_EQ(RadarMapMode_FromCvar(1.0f), RadarMapMode::OverviewTextured);
    EXPECT_EQ(RadarMapMode_FromCvar(2.0f), RadarMapMode::OverviewTextured);
    EXPECT_EQ(RadarMapMode_FromCvar(3.0f), RadarMapMode::SliceTextured);
    EXPECT_EQ(RadarMapMode_FromCvar(4.0f), RadarMapMode::SliceFlat);
    EXPECT_EQ(RadarMapMode_FromCvar(5.0f), RadarMapMode::SliceTextured);
    EXPECT_EQ(RadarMapMode_FromCvar(6.0f), RadarMapMode::SliceFlat);
    EXPECT_EQ(RadarMapMode_FromCvar(0.0f), RadarMapMode::OverviewTextured);
    EXPECT_EQ(RadarMapMode_FromCvar(7.0f), RadarMapMode::SliceFlat);
}
