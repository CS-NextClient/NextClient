#include <array>

#include <gtest/gtest.h>

#include "hud/radar/layers/players/PlayerObserver.h"

namespace
{
    FrameContext Observing(goldsrc::cl_entity_t& local)
    {
        FrameContext context{};
        context.local = &local;
        context.spectating = true;
        context.self_idx = 1;
        context.highlight_idx = 3;
        context.observed_idx = 3;
        context.message_num = 42;
        return context;
    }
} // namespace

TEST(PlayerObserver, CurrentObservedPlayerUsesConfirmedPositionInsteadOfTheDeadLocalPlayer)
{
    goldsrc::cl_entity_t local{};
    local.index = 1;
    local.curstate.messagenum = 40;
    local.origin[0] = 999.0f;
    goldsrc::cl_entity_t observed{};
    observed.player = 1;
    observed.index = 3;
    observed.curstate.messagenum = 42;
    observed.curstate.origin[0] = 300.0f;
    observed.curstate.origin[1] = 400.0f;
    observed.curstate.origin[2] = 500.0f;
    observed.origin[0] = 100.0f;
    observed.angles[1] = 60.0f;
    PlayerObserverMarker marker = PlayerObserver_ChooseMarker(Observing(local), &observed, true, {30.0f, 40.0f, 50.0f}, 90.0f);
    EXPECT_EQ(marker.pos, (ncl_math::Vector3{300.0f, 400.0f, 500.0f}));
    EXPECT_FLOAT_EQ(marker.yaw, 60.0f);
    EXPECT_EQ(marker.player, 3);
}

TEST(PlayerObserver, ThirdPersonTargetDoesNotDependOnInEyeHighlighting)
{
    goldsrc::cl_entity_t local{};
    FrameContext context = Observing(local);
    context.highlight_idx = 0;
    goldsrc::cl_entity_t target{};
    target.index = context.observed_idx;
    target.player = 1;
    target.curstate.messagenum = 42;
    target.curstate.origin[0] = 300.0f;
    target.curstate.origin[1] = 400.0f;
    target.curstate.origin[2] = 500.0f;
    PlayerObserverMarker marker = PlayerObserver_ChooseMarker(context, &target, true, {10.0f, 20.0f, 30.0f}, 45.0f);
    EXPECT_EQ(marker.pos, (ncl_math::Vector3{300.0f, 400.0f, 500.0f}));
    EXPECT_EQ(marker.player, 3);
}

TEST(PlayerObserver, MissingOrStaleTargetUsesTheCameraWithoutAnOldPlayerMarker)
{
    goldsrc::cl_entity_t local{};
    FrameContext context = Observing(local);
    goldsrc::cl_entity_t target{};
    target.player = 1;
    target.index = 3;
    target.curstate.messagenum = 41;
    target.origin[0] = 900.0f;
    std::array<const goldsrc::cl_entity_t*, 2> targets{&target, nullptr};
    for (const goldsrc::cl_entity_t* entity : targets)
    {
        PlayerObserverMarker marker = PlayerObserver_ChooseMarker(context, entity, true, {100.0f, 200.0f, 300.0f}, 45.0f);
        EXPECT_EQ(marker.pos, (ncl_math::Vector3{100.0f, 200.0f, 300.0f}));
        EXPECT_FLOAT_EQ(marker.yaw, 45.0f);
        EXPECT_EQ(marker.player, 0);
    }
}

TEST(PlayerObserver, FreeCameraIgnoresACurrentObservedEntity)
{
    goldsrc::cl_entity_t local{};
    FrameContext context = Observing(local);
    context.free_camera = true;
    goldsrc::cl_entity_t target{};
    target.player = 1;
    target.index = 3;
    target.curstate.messagenum = 42;
    target.origin[0] = 900.0f;
    PlayerObserverMarker marker = PlayerObserver_ChooseMarker(context, &target, true, {100.0f, 200.0f, 300.0f}, 45.0f);
    EXPECT_EQ(marker.pos, (ncl_math::Vector3{100.0f, 200.0f, 300.0f}));
    EXPECT_FLOAT_EQ(marker.yaw, 45.0f);
    EXPECT_EQ(marker.player, 0);
}

TEST(PlayerObserver, DeadTargetUsesTheCamera)
{
    goldsrc::cl_entity_t local{};
    goldsrc::cl_entity_t target{};
    target.player = 1;
    target.index = 3;
    target.curstate.messagenum = 42;
    PlayerObserverMarker marker = PlayerObserver_ChooseMarker(Observing(local), &target, false, {10.0f, 20.0f, 30.0f}, 45.0f);
    EXPECT_EQ(marker.pos, (ncl_math::Vector3{10.0f, 20.0f, 30.0f}));
    EXPECT_EQ(marker.player, 0);
}

TEST(PlayerObserverStatus, BadgeStaysInsideRoundAndSquareRadarIncludingTab)
{
    for (float height : {720.0f, 1080.0f, 2160.0f})
    {
        for (float scale : {0.5f, 1.0f, 3.0f})
        {
            for (bool fullmap : {false, true})
            {
                RadarBox box = RadarView_ComputeBox(height * 16.0f / 9.0f, height, scale, true, fullmap);
                for (bool round : {false, true})
                {
                    SCOPED_TRACE(testing::Message() << height << " " << scale << " " << fullmap << " " << round);
                    RadarShape shape = RadarView_ComputeShape(box, round);
                    PlayerObserverIcon icon = PlayerObserver_BuildStatusIcon(box, shape);
                    ASSERT_GT(icon.radius, 0.0f);
                    ASSERT_GT(icon.line_width, 0.0f);
                    EXPECT_GT(icon.center.x, shape.center.x);
                    EXPECT_GT(icon.center.y, shape.center.y);
                    for (int step = 0; step < 32; step++)
                    {
                        ncl_math::Vector2 edge = icon.center + ncl_math::Vector2{icon.radius, 0.0f}.Rotated(step * ncl_math::kPi / 16.0f);
                        EXPECT_TRUE(shape.Contains(edge));
                    }
                }
            }
        }
    }
}

TEST(PlayerObserverStatus, SmallScreensKeepAReadableGlyphAndStroke)
{
    for (float height : {480.0f, 720.0f, 1080.0f})
    {
        RadarBox box = RadarView_ComputeBox(height * 16.0f / 9.0f, height, 1.0f, true, false);
        PlayerObserverIcon icon = PlayerObserver_BuildStatusIcon(box, RadarView_ComputeShape(box, true));
        EXPECT_GE(icon.half_extent, 8.0f);
        EXPECT_GE(icon.radius, 11.5f);
        EXPECT_GE(icon.line_width, 2.1f);
    }
}
