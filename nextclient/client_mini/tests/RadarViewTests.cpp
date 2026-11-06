#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include <goldsrc/pmove.h>
#include <goldsrc/entity.h>
#include <next_client_mini/radar_cvars.h>

#include "hud/radar/render/RadarView.h"
#include "hud/radar/RadarFrame.h"
#include "hud/radar/layers/players/PlayerLastSeen.h"

namespace
{
    RadarViewParams BaseParams()
    {
        RadarViewParams p{};
        p.ov_zoom = 1.0f;
        p.ov_origin = {0.0f, 0.0f};
        p.ov_rotated = false;
        p.map_scale = 1.0f;
        p.map_shift = true;
        p.rotate = false;
        p.fullmap_enabled = false;
        p.round = false;
        p.view_origin = {0.0f, 0.0f};
        p.view_yaw = 0.0f;
        p.sprite_frames = 0;
        return p;
    }
} // namespace

TEST(RadarViewObserver, OnlyFreeFlightUsesTheCameraMarker)
{
    EXPECT_TRUE(RadarView_IsFreeCamera(goldsrc::OBS_ROAMING));
    EXPECT_FALSE(RadarView_IsFreeCamera(goldsrc::OBS_CHASE_FREE));
}

TEST(RadarViewObserver, AttachedViewsDoNotAddACameraMarker)
{
    for (int mode : {goldsrc::OBS_NONE, goldsrc::OBS_IN_EYE, goldsrc::OBS_CHASE_LOCKED, goldsrc::OBS_MAP_FREE, goldsrc::OBS_MAP_CHASE})
    {
        SCOPED_TRACE(mode);
        EXPECT_FALSE(RadarView_IsFreeCamera(mode));
    }
}

TEST(RadarViewObserver, ThirdPersonViewsHaveNoGreenMarker)
{
    for (int mode : {goldsrc::OBS_CHASE_FREE, goldsrc::OBS_CHASE_LOCKED})
    {
        SCOPED_TRACE(mode);
        EXPECT_FALSE(RadarView_IsFreeCamera(mode));
        EXPECT_FALSE(RadarView_ShouldHighlightPlayer(mode));
    }
    EXPECT_TRUE(RadarView_ShouldHighlightPlayer(goldsrc::OBS_NONE));
    EXPECT_TRUE(RadarView_ShouldHighlightPlayer(goldsrc::OBS_IN_EYE));
    EXPECT_FALSE(RadarView_ShouldHighlightPlayer(goldsrc::OBS_ROAMING));
}

TEST(RadarViewComputeBox, AnchorsToScreenHeight)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    EXPECT_FLOAT_EQ(box.size, 1080.0f * 0.20f);
    EXPECT_FLOAT_EQ(box.x, 1080.0f * 0.02f);
    EXPECT_FLOAT_EQ(box.y, box.x);
    EXPECT_FLOAT_EQ(box.marker_px, box.size);

    RadarBox fullmap = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false, true);
    EXPECT_FLOAT_EQ(fullmap.size, 259.2f);
    EXPECT_FLOAT_EQ(fullmap.marker_px, 183.6f);
    EXPECT_FLOAT_EQ(fullmap.x, 21.6f);
    EXPECT_FLOAT_EQ(fullmap.y, 21.6f);
}

TEST(RadarViewComputeBox, NarrowFullmapUsesTenPercentEnlargement)
{
    for (ncl_math::Vector2 screen : {ncl_math::Vector2{1280.0f, 960.0f}, ncl_math::Vector2{1280.0f, 1024.0f}})
    {
        for (float scale : {0.5f, 1.15f, 3.0f})
        {
            for (bool spectating : {false, true})
            {
                RadarBox normal = RadarView_ComputeBox(screen.x, screen.y, scale, spectating);
                RadarBox fullmap = RadarView_ComputeBox(screen.x, screen.y, scale, spectating, true);

                EXPECT_FLOAT_EQ(fullmap.size, normal.size * 1.1f);
                EXPECT_FLOAT_EQ(fullmap.marker_px, normal.marker_px * 0.85f);
                EXPECT_FLOAT_EQ(fullmap.x, normal.x);
                EXPECT_FLOAT_EQ(fullmap.y, normal.y);
            }
        }
    }
}

TEST(RadarViewComputeBox, SpectatorMarginAndHiResDamping)
{
    RadarBox spec = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, true);
    EXPECT_GT(spec.y, spec.x);

    RadarBox hi = RadarView_ComputeBox(3840.0f, 2160.0f, 1.0f, false);
    EXPECT_FLOAT_EQ(hi.size, 2160.0f * 0.20f);
    EXPECT_LT(hi.marker_px, hi.size);

    RadarBox fullmap = RadarView_ComputeBox(3840.0f, 2160.0f, 3.0f, true, true);
    EXPECT_FLOAT_EQ(fullmap.size, 1555.2f);
    EXPECT_FLOAT_EQ(fullmap.marker_px, 771.12f);
    EXPECT_FLOAT_EQ(fullmap.x, 43.2f);
    EXPECT_FLOAT_EQ(fullmap.y, 345.6f);
}

TEST(RadarViewComputeTransform, ViewOriginMapsToBoxCenter)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.view_origin = {512.0f, -256.0f};

    RadarTransform t = RadarView_ComputeTransform(box, p);
    ncl_math::Vector2 s = t.WorldToScreen(p.view_origin);

    EXPECT_NEAR(s.x, box.x + box.size * 0.5f, 1e-3f);
    EXPECT_NEAR(s.y, box.y + box.size * 0.5f, 1e-3f);
}

TEST(RadarViewComputeTransform, WorldOffsetsKeepScreenDistanceRatio)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();

    RadarTransform t = RadarView_ComputeTransform(box, p);
    ncl_math::Vector2 c = t.WorldToScreen({0.0f, 0.0f});
    ncl_math::Vector2 a = t.WorldToScreen({100.0f, 0.0f});
    ncl_math::Vector2 b = t.WorldToScreen({200.0f, 0.0f});

    EXPECT_NEAR((b - c).Length(), 2.0f * (a - c).Length(), 1e-3f);
}

TEST(RadarViewComputeTransform, RotationTurnsMarkerDirectionNotDistance)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams flat = BaseParams();
    RadarViewParams turned = BaseParams();
    turned.rotate = true;
    turned.view_yaw = 90.0f;

    RadarTransform t0 = RadarView_ComputeTransform(box, flat);
    RadarTransform t1 = RadarView_ComputeTransform(box, turned);

    ncl_math::Vector2 w{300.0f, 40.0f};
    float d0 = (t0.WorldToScreen(w) - t0.center).Length();
    float d1 = (t1.WorldToScreen(w) - t1.center).Length();
    EXPECT_NEAR(d0, d1, 1e-2f);

    ncl_math::Vector2 m0 = t0.MarkerDirection(0.0f);
    ncl_math::Vector2 m1 = t1.MarkerDirection(90.0f);
    EXPECT_NEAR(m0.x, m1.x, 1e-3f);
    EXPECT_NEAR(m0.y, m1.y, 1e-3f);
}

TEST(RadarViewComputeTransform, FullmapRoundZoomsOutToFitTheDisc)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams square = BaseParams();
    square.fullmap_enabled = true;
    square.scoreboard_visible = true;
    RadarViewParams round = square;
    round.round = true;

    RadarTransform ts = RadarView_ComputeTransform(box, square);
    RadarTransform tr = RadarView_ComputeTransform(box, round);

    ncl_math::Vector2 w{1000.0f, 0.0f};
    float ds = (ts.WorldToScreen(w) - ts.center).Length();
    float dr = (tr.WorldToScreen(w) - tr.center).Length();
    EXPECT_LT(dr, ds);
}

TEST(RadarViewComputeTransform, SpriteFrameCountDrivesTileGrid)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);

    RadarViewParams none = BaseParams();
    RadarViewParams one = BaseParams();
    one.sprite_frames = 12; // one zoom level
    RadarViewParams multi = BaseParams();
    multi.sprite_frames = 48; // two zoom levels

    RadarTransform tn = RadarView_ComputeTransform(box, none);
    RadarTransform t1 = RadarView_ComputeTransform(box, one);
    RadarTransform tm = RadarView_ComputeTransform(box, multi);

    EXPECT_EQ(tn.total_frames, 12);
    EXPECT_EQ(tn.x_tiles, 4);
    EXPECT_EQ(tn.y_tiles, 3);

    EXPECT_EQ(t1.x_tiles, 4);
    EXPECT_EQ(t1.y_tiles, 3);

    EXPECT_EQ(tm.total_frames, 48);
    EXPECT_EQ(tm.x_tiles, 8);
    EXPECT_EQ(tm.y_tiles, 6);
}

TEST(RadarViewComputeTransform, RotatedOverviewProjectsMarkersConsistently)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);

    RadarViewParams flat = BaseParams();
    flat.view_origin = {512.0f, -256.0f};
    RadarViewParams rot = flat;
    rot.ov_rotated = true;

    RadarTransform tf = RadarView_ComputeTransform(box, flat);
    RadarTransform tr = RadarView_ComputeTransform(box, rot);

    ncl_math::Vector2 cr = tr.WorldToScreen(rot.view_origin);
    EXPECT_NEAR(cr.x, box.x + box.size * 0.5f, 1e-3f);
    EXPECT_NEAR(cr.y, box.y + box.size * 0.5f, 1e-3f);

    // Catches a sign error in the rotated basis (basis_x = up / basis_y = -right): only the
    // map image orientation changes, so the marker must land where the flat branch puts it.
    ncl_math::Vector2 w{512.0f + 300.0f, -256.0f + 40.0f};
    ncl_math::Vector2 pr = tr.WorldToScreen(w);
    ncl_math::Vector2 pf = tf.WorldToScreen(w);
    EXPECT_NEAR(pr.x, pf.x, 1e-3f);
    EXPECT_NEAR(pr.y, pf.y, 1e-3f);
}

TEST(RadarViewClampToRadar, InsidePointIsUntouched)
{
    RadarBox box{0.0f, 0.0f, 200.0f, 200.0f, 800.0f, 600.0f};
    ncl_math::Vector2 p{100.0f, 100.0f};
    ncl_math::Vector2 r = RadarView_ComputeShape(box, true).Clamp(10, p);
    ncl_math::Vector2 s = RadarView_ComputeShape(box, false).Clamp(10, p);
    EXPECT_FLOAT_EQ(r.x, p.x);
    EXPECT_FLOAT_EQ(r.y, p.y);
    EXPECT_FLOAT_EQ(s.x, p.x);
    EXPECT_FLOAT_EQ(s.y, p.y);
}

TEST(RadarViewClampToRadar, RoundClampsToRim)
{
    RadarBox box{0.0f, 0.0f, 200.0f, 200.0f, 800.0f, 600.0f};
    ncl_math::Vector2 c = RadarView_ComputeShape(box, true).Clamp(10, {300.0f, 100.0f});
    EXPECT_FLOAT_EQ(c.x, 190.0f); // centre 100 + (half 100 - size 10)
    EXPECT_FLOAT_EQ(c.y, 100.0f);
}

TEST(RadarViewClampToRadar, SquareClampsPerAxis)
{
    RadarBox box{0.0f, 0.0f, 200.0f, 200.0f, 800.0f, 600.0f};
    ncl_math::Vector2 c = RadarView_ComputeShape(box, false).Clamp(10, {300.0f, -50.0f});
    EXPECT_FLOAT_EQ(c.x, 190.0f);
    EXPECT_FLOAT_EQ(c.y, 10.0f);
}

TEST(RadarViewComputeTransform, FullmapOptionFollowsPlayerUntilScoreboardIsVisible)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.ov_origin = {-128.0f, 256.0f};
    p.view_origin = {512.0f, -256.0f};
    p.view_yaw = 90.0f;
    p.rotate = true;
    p.map_scale = 2.0f;
    RadarTransform follow = RadarView_ComputeTransform(box, p);

    p.fullmap_enabled = true;
    RadarTransform without_scoreboard = RadarView_ComputeTransform(box, p);
    ncl_math::Vector2 sample = p.view_origin + ncl_math::Vector2{100.0f, 50.0f};
    EXPECT_NEAR((without_scoreboard.WorldToScreen(sample) - follow.WorldToScreen(sample)).Length(), 0.0f, 1e-3f);

    p.scoreboard_visible = true;
    RadarTransform full = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR((full.WorldToScreen(p.ov_origin) - box.center()).Length(), 0.0f, 1e-3f);
    EXPECT_GT((full.WorldToScreen(p.view_origin) - box.center()).Length(), 1.0f);

    p.scoreboard_visible = false;
    RadarTransform restored = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR((restored.WorldToScreen(sample) - follow.WorldToScreen(sample)).Length(), 0.0f, 1e-3f);
}

TEST(RadarViewComputeTransform, DisabledFullmapOptionKeepsFollowViewWithScoreboard)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.view_origin = {512.0f, -256.0f};
    p.view_yaw = 90.0f;
    p.rotate = true;
    p.map_scale = 2.0f;
    RadarTransform follow = RadarView_ComputeTransform(box, p);

    p.scoreboard_visible = true;
    RadarTransform with_scoreboard = RadarView_ComputeTransform(box, p);
    ncl_math::Vector2 sample = p.view_origin + ncl_math::Vector2{100.0f, 50.0f};
    EXPECT_NEAR((with_scoreboard.WorldToScreen(sample) - follow.WorldToScreen(sample)).Length(), 0.0f, 1e-3f);
}

TEST(RadarViewComputeTransform, FullmapScoreboardFitsMapIndependentlyOfFollowZoom)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.fullmap_enabled = true;
    p.scoreboard_visible = true;
    RadarTransform base = RadarView_ComputeTransform(box, p);

    p.map_scale = 3.5f;
    p.rotate = true;
    p.view_yaw = 90.0f;
    RadarTransform zoomed = RadarView_ComputeTransform(box, p);
    ncl_math::Vector2 sample{1000.0f, 500.0f};
    EXPECT_NEAR((zoomed.WorldToScreen(sample) - base.WorldToScreen(sample)).Length(), 0.0f, 1e-3f);
}

TEST(RadarViewComputeTransform, FullmapFitsLargeMapCornersWithInset)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    const ncl_math::Aabb2 bounds{{-6000.0f, -1000.0f}, {5000.0f, 9000.0f}};

    for (bool rotated : {false, true})
    {
        for (int frames : {0, 48, 192})
        {
            SCOPED_TRACE(testing::Message() << "rotated=" << rotated << " frames=" << frames);
            RadarViewParams p = BaseParams();
            p.fullmap_enabled = true;
            p.scoreboard_visible = true;
            p.ov_origin = {300.0f, -700.0f};
            p.ov_zoom = 1.75f;
            p.ov_rotated = rotated;
            p.sprite_frames = frames;
            p.map_bounds = bounds;
            RadarTransform transform = RadarView_ComputeTransform(box, p);
            EXPECT_NEAR((transform.WorldToScreen(bounds.Center()) - box.center()).Length(), 0.0f, 0.001f);

            for (ncl_math::Vector2 corner :
                 {bounds.min, bounds.max, ncl_math::Vector2{bounds.min.x, bounds.max.y}, ncl_math::Vector2{bounds.max.x, bounds.min.y}})
            {
                ncl_math::Vector2 screen = transform.WorldToScreen(corner);
                EXPECT_GE(screen.x, box.x + 3.0f - 0.001f);
                EXPECT_LE(screen.x, box.x + box.size - 3.0f + 0.001f);
                EXPECT_GE(screen.y, box.y + 3.0f - 0.001f);
                EXPECT_LE(screen.y, box.y + box.size - 3.0f + 0.001f);
            }
        }
    }
}

TEST(RadarViewComputeTransform, FullmapFitIsIndependentOfOverviewZoom)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.fullmap_enabled = true;
    p.scoreboard_visible = true;
    p.map_bounds = ncl_math::Aabb2{{-4096.0f, -2048.0f}, {4096.0f, 2048.0f}};
    RadarTransform base = RadarView_ComputeTransform(box, p);

    p.ov_zoom = 0.85f;
    RadarTransform changed = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR((base.WorldToScreen(p.map_bounds->max) - changed.WorldToScreen(p.map_bounds->max)).Length(), 0.0f, 0.001f);
}

TEST(RadarViewComputeTransform, FullmapRoundFitsWideAndTallMapCorners)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);

    for (ncl_math::Aabb2 bounds :
         {ncl_math::Aabb2{{-6000.0f, -1000.0f}, {6000.0f, 1000.0f}}, ncl_math::Aabb2{{-1000.0f, -6000.0f}, {1000.0f, 6000.0f}}})
    {
        RadarViewParams p = BaseParams();
        p.fullmap_enabled = true;
        p.scoreboard_visible = true;
        p.round = true;
        p.map_bounds = bounds;
        RadarTransform transform = RadarView_ComputeTransform(box, p);
        EXPECT_NEAR((transform.WorldToScreen(bounds.max) - box.center()).Length(), box.radius() - 3.0f, 0.001f);
        EXPECT_NEAR((transform.WorldToScreen(bounds.min) - box.center()).Length(), box.radius() - 3.0f, 0.001f);
    }
}

TEST(RadarViewComputeTransform, DefaultInputsProduceFiniteFollowProjection)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p;
    RadarTransform transform = RadarView_ComputeTransform(box, p);
    ncl_math::Vector2 center = transform.WorldToScreen({0.0f, 0.0f});
    ncl_math::Vector2 sample = transform.WorldToScreen({100.0f, 50.0f});

    EXPECT_NEAR((center - box.center()).Length(), 0.0f, 1e-3f);
    EXPECT_TRUE(std::isfinite(sample.x));
    EXPECT_TRUE(std::isfinite(sample.y));
    EXPECT_GT((sample - center).Length(), 0.0f);
}

TEST(RadarViewComputeTransform, FullmapInsetStaysFixedAcrossRadarSizes)
{
    for (float screen_h : {480.0f, 1080.0f, 2160.0f})
    {
        for (float scale : {0.5f, 1.0f, 3.0f})
        {
            RadarBox box = RadarView_ComputeBox(screen_h * 16.0f / 9.0f, screen_h, scale, false, true);

            for (ncl_math::Aabb2 bounds :
                 {ncl_math::Aabb2{{-6000.0f, -1000.0f}, {6000.0f, 1000.0f}}, ncl_math::Aabb2{{-1000.0f, -6000.0f}, {1000.0f, 6000.0f}}})
            {
                for (bool rotated : {false, true})
                {
                    SCOPED_TRACE(testing::Message() << "height=" << screen_h << " scale=" << scale << "rotated=" << rotated);
                    RadarViewParams p = BaseParams();
                    p.fullmap_enabled = true;
                    p.scoreboard_visible = true;
                    p.ov_rotated = rotated;
                    p.map_bounds = bounds;
                    RadarTransform transform = RadarView_ComputeTransform(box, p);
                    float nearest_inset = box.size;

                    for (ncl_math::Vector2 corner :
                         {bounds.min,
                          bounds.max,
                          ncl_math::Vector2{bounds.min.x, bounds.max.y},
                          ncl_math::Vector2{bounds.max.x, bounds.min.y}})
                    {
                        ncl_math::Vector2 screen = transform.WorldToScreen(corner);
                        nearest_inset = std::min(
                            {nearest_inset, screen.x - box.x, box.x + box.size - screen.x, screen.y - box.y, box.y + box.size - screen.y}
                        );
                    }

                    EXPECT_NEAR(nearest_inset, 3.0f, 0.001f);
                    float screen_width =
                        (transform.WorldToScreen({bounds.max.x, bounds.min.y}) - transform.WorldToScreen(bounds.min)).Length();
                    float screen_height =
                        (transform.WorldToScreen({bounds.min.x, bounds.max.y}) - transform.WorldToScreen(bounds.min)).Length();
                    EXPECT_NEAR(screen_width / screen_height, bounds.Size().x / bounds.Size().y, 0.001f);
                }
            }
        }
    }
}

TEST(RadarViewEdgeShift, MapEdgeKeepsAnInsetWithoutChangingScale)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.round = true;
    p.view_origin = {-1700.0f, 0.0f};
    RadarTransform centered = RadarView_ComputeTransform(box, p);
    p.map_bounds = ncl_math::Aabb2{{-2000.0f, -1600.0f}, {2000.0f, 1600.0f}};
    RadarTransform shifted = RadarView_ComputeTransform(box, p);

    EXPECT_NEAR(shifted.world_center.x, -1244.0f, 0.001f);
    EXPECT_NEAR(shifted.world_center.y, 0.0f, 0.001f);
    EXPECT_NEAR((shifted.WorldToScreen({-2000.0f, 0.0f}) - box.center()).Length(), box.radius() * 0.7f, 0.001f);
    EXPECT_GT((shifted.WorldToScreen(p.view_origin) - box.center()).Length(), 1.0f);
    EXPECT_TRUE(RadarView_ComputeShape(box, p.round).Contains(shifted.WorldToScreen(p.view_origin)));
    ncl_math::Vector2 offset{100.0f, 70.0f};
    EXPECT_NEAR(
        (shifted.WorldToScreen(p.view_origin + offset) - shifted.WorldToScreen(p.view_origin)).Length(),
        (centered.WorldToScreen(p.view_origin + offset) - centered.WorldToScreen(p.view_origin)).Length(),
        0.001f
    );
}

TEST(RadarViewEdgeShift, SmallMapCentersWithoutChangingZoom)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.view_origin = {80.0f, 25.0f};
    p.map_bounds = ncl_math::Aabb2{{-100.0f, -50.0f}, {100.0f, 50.0f}};
    RadarTransform shifted = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR((shifted.world_center - p.map_bounds->Center()).Length(), 0.0f, 0.001f);
    EXPECT_NEAR((shifted.WorldToScreen({0.0f, 0.0f}) - box.center()).Length(), 0.0f, 0.001f);
    EXPECT_NEAR((shifted.WorldToScreen({100.0f, 0.0f}) - shifted.WorldToScreen({0.0f, 0.0f})).Length(), 10.0f, 0.001f);
}

TEST(RadarViewEdgeShift, CornersKeepPlayerVisibleAndTilesAlignedUnderRotation)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    for (bool round : {false, true})
    {
        for (bool rotated : {false, true})
        {
            for (float yaw : {0.0f, 45.0f, 123.0f, 270.0f})
            {
                SCOPED_TRACE(testing::Message() << "round=" << round << " rotated=" << rotated << " yaw=" << yaw);
                RadarViewParams p = BaseParams();
                p.round = round;
                p.ov_rotated = rotated;
                p.sprite_frames = 48;
                p.rotate = true;
                p.view_yaw = yaw;
                p.view_origin = {-1990.0f, 1990.0f};
                RadarTransform centered = RadarView_ComputeTransform(box, p);
                p.map_bounds = ncl_math::Aabb2{{-2000.0f, -2000.0f}, {2000.0f, 2000.0f}};
                RadarTransform shifted = RadarView_ComputeTransform(box, p);
                EXPECT_GT(shifted.world_center.x, p.view_origin.x);
                EXPECT_LT(shifted.world_center.y, p.view_origin.y);
                ncl_math::Vector2 marker = shifted.WorldToScreen(p.view_origin) - box.center();
                float extent = round ? marker.Length() : std::max(std::abs(marker.x), std::abs(marker.y));
                EXPECT_LE(extent, box.radius() * 0.9f + 0.001f);
                EXPECT_TRUE(RadarView_ComputeShape(box, round).Contains(shifted.WorldToScreen(p.view_origin)));
                ncl_math::Vector2 image_shift = shifted.tile_origin - centered.tile_origin;
                ncl_math::Vector2 marker_shift = shifted.WorldToScreen({0.0f, 0.0f}) - centered.WorldToScreen({0.0f, 0.0f});
                EXPECT_NEAR((image_shift - marker_shift).Length(), 0.0f, 0.001f);
            }
        }
    }
}

TEST(RadarCvars, MapShiftDefaultsDisabledAndBounded)
{
    cvars::CvarDefinition shift = radar::FindCvar("hud_radar_map_shift");
    ASSERT_NE(shift.name, nullptr);
    EXPECT_STREQ(shift.default_value, "0");
    EXPECT_FLOAT_EQ(shift.min_value, 0.0f);
    EXPECT_FLOAT_EQ(shift.max_value, 1.0f);
}

TEST(RadarViewEdgeShift, DefaultOptionKeepsReferenceCenteredNearMapEdges)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p{};
    p.view_origin = {-1900.0f, 1900.0f};
    p.map_bounds = ncl_math::Aabb2{{-2000.0f, -2000.0f}, {2000.0f, 2000.0f}};
    RadarTransform transform = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR(transform.WorldToScreen(p.view_origin).x, box.center().x, 0.001f);
    EXPECT_NEAR(transform.WorldToScreen(p.view_origin).y, box.center().y, 0.001f);
    EXPECT_NEAR(transform.world_center.x, -1900.0f, 0.001f);
    EXPECT_NEAR(transform.world_center.y, 1900.0f, 0.001f);
}

TEST(RadarViewEdgeShift, DisabledOptionKeepsReferenceCentered)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.view_origin = {-1900.0f, 1900.0f};
    RadarTransform without_bounds = RadarView_ComputeTransform(box, p);
    p.map_bounds = ncl_math::Aabb2{{-2000.0f, -2000.0f}, {2000.0f, 2000.0f}};
    p.map_shift = false;
    RadarTransform disabled = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR((disabled.WorldToScreen(p.view_origin) - box.center()).Length(), 0.0f, 0.001f);
    EXPECT_NEAR((disabled.tile_origin - without_bounds.tile_origin).Length(), 0.0f, 0.001f);
    EXPECT_NEAR((disabled.world_center - p.view_origin).Length(), 0.0f, 0.001f);
}

TEST(RadarViewEdgeShift, InteriorReferenceKeepsItsProjection)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.view_origin = {100.0f, -200.0f};
    p.rotate = true;
    p.view_yaw = 37.0f;
    RadarTransform centered = RadarView_ComputeTransform(box, p);
    p.map_bounds = ncl_math::Aabb2{{-5000.0f, -5000.0f}, {5000.0f, 5000.0f}};
    RadarTransform shifted = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR((shifted.world_center - p.view_origin).Length(), 0.0f, 0.001f);
    EXPECT_NEAR((shifted.tile_origin - centered.tile_origin).Length(), 0.0f, 0.001f);
}

TEST(RadarViewEdgeShift, RotatedSquareAccountsForItsCorners)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.rotate = true;
    p.view_yaw = 45.0f;
    p.view_origin = {-3900.0f, 0.0f};
    p.map_bounds = ncl_math::Aabb2{{-5000.0f, -5000.0f}, {5000.0f, 5000.0f}};
    RadarTransform shifted = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR(shifted.world_center.x, -5000.0f + 864.0f * std::sqrt(2.0f), 0.001f);
    EXPECT_NEAR(shifted.world_center.y, 0.0f, 0.001f);
}

TEST(RadarViewEdgeShift, CircularShiftIsStableWhileTurning)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.round = true;
    p.rotate = true;
    p.view_origin = {-4990.0f, 4990.0f};
    p.map_bounds = ncl_math::Aabb2{{-5000.0f, -5000.0f}, {5000.0f, 5000.0f}};
    RadarTransform base = RadarView_ComputeTransform(box, p);
    for (float yaw : {17.0f, 45.0f, 90.0f, 123.0f, 270.0f})
    {
        p.view_yaw = yaw;
        RadarTransform turned = RadarView_ComputeTransform(box, p);
        EXPECT_NEAR((turned.world_center - base.world_center).Length(), 0.0f, 0.001f);
    }
}

TEST(RadarViewEdgeShift, EdgePaddingScalesWithMapZoomAndRadarSize)
{
    for (float height : {480.0f, 1080.0f, 2160.0f})
    {
        for (float radar_scale : {0.5f, 1.0f, 3.0f})
        {
            RadarBox box = RadarView_ComputeBox(height * 16.0f / 9.0f, height, radar_scale, false);
            for (float map_scale : {0.25f, 1.0f, 3.0f})
            {
                RadarViewParams p = BaseParams();
                p.round = true;
                p.map_scale = map_scale;
                RadarTransform base = RadarView_ComputeTransform(box, p);
                float world_radius = box.radius() * 1000.0f / (base.WorldToScreen({1000.0f, 0.0f}) - base.center).Length();
                p.view_origin = {-20000.0f + world_radius * 0.25f, 0.0f};
                p.map_bounds = ncl_math::Aabb2{{-20000.0f, -20000.0f}, {20000.0f, 20000.0f}};
                RadarTransform shifted = RadarView_ComputeTransform(box, p);
                EXPECT_NEAR((shifted.WorldToScreen({-20000.0f, 0.0f}) - box.center()).Length(), box.radius() * 0.7f, 0.01f);
                EXPECT_NEAR((shifted.WorldToScreen(p.view_origin) - box.center()).Length(), box.radius() * 0.45f, 0.01f);
            }
        }
    }
}

TEST(RadarViewEdgeShift, ThresholdHasNoPositionJump)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams p = BaseParams();
    p.round = true;
    p.map_bounds = ncl_math::Aabb2{{-2000.0f, -2000.0f}, {2000.0f, 2000.0f}};
    p.view_origin = {-1243.99f, 0.0f};
    RadarTransform inside = RadarView_ComputeTransform(box, p);
    p.view_origin = {-1244.01f, 0.0f};
    RadarTransform outside = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR(inside.world_center.x, -1243.99f, 0.001f);
    EXPECT_NEAR(outside.world_center.x, -1244.0f, 0.001f);
    EXPECT_LT((inside.tile_origin - outside.tile_origin).Length(), 0.01f);
}

TEST(RadarViewEdgeShift, FullmapUsesFittedProjectionWithEitherToggleValue)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false, true);
    RadarViewParams p = BaseParams();
    p.fullmap_enabled = true;
    p.scoreboard_visible = true;
    p.view_origin = {-1900.0f, 1900.0f};
    p.map_bounds = ncl_math::Aabb2{{-2000.0f, -2000.0f}, {2000.0f, 2000.0f}};
    RadarTransform enabled = RadarView_ComputeTransform(box, p);
    p.map_shift = false;
    RadarTransform disabled = RadarView_ComputeTransform(box, p);
    EXPECT_NEAR((enabled.world_center - p.map_bounds->Center()).Length(), 0.0f, 0.001f);
    EXPECT_NEAR((enabled.tile_origin - disabled.tile_origin).Length(), 0.0f, 0.001f);
    EXPECT_NEAR((enabled.WorldToScreen(p.map_bounds->min) - disabled.WorldToScreen(p.map_bounds->min)).Length(), 0.0f, 0.001f);
}

TEST(RadarFrameSnapshot, ObservedAllyRemainsCurrentWhenLocalSnapshotStopsUpdating)
{
    goldsrc::cl_entity_t local{};
    local.curstate.messagenum = 40;
    goldsrc::cl_entity_t ally{};
    ally.curstate.messagenum = 42;
    FrameContext context{};
    context.local = &local;
    context.spectating = true;
    context.message_num = 42;
    PlayerLastSeen memory;
    PlayerLastSeen::Observation visible[] = {{3, {100.0f, 100.0f, 0.0f}, RadarTeam::CounterTerrorist}};
    memory.Update(visible, 10.0f);
    memory.Update(
        RadarFrame_IsEntityCurrent(context, &ally) ? std::span<const PlayerLastSeen::Observation>{visible}
                                                   : std::span<const PlayerLastSeen::Observation>{},
        10.2f
    );
    EXPECT_TRUE(memory.get_marks().empty());
    EXPECT_FALSE(RadarFrame_IsEntityCurrent(context, &local));
    EXPECT_FALSE(RadarFrame_IsEntityCurrent(context, nullptr));
    ally.curstate.messagenum = 41;
    EXPECT_FALSE(RadarFrame_IsEntityCurrent(context, &ally));
}

TEST(RadarViewComputeBox, TabMarkersKeepTheSelectedRatioAcrossResolutionAndRadarScale)
{
    for (float height : {720.0f, 1080.0f, 2160.0f})
    {
        for (float scale : {0.5f, 1.0f, 3.0f})
        {
            for (bool spectating : {false, true})
            {
                RadarBox normal = RadarView_ComputeBox(height * 16.0f / 9.0f, height, scale, spectating);
                RadarBox tab = RadarView_ComputeBox(height * 16.0f / 9.0f, height, scale, spectating, true);
                EXPECT_NEAR(tab.marker_px / normal.marker_px, 0.85f, 0.000001f);
                EXPECT_NEAR(tab.size / normal.size, 1.2f, 0.000001f);
            }
        }
    }
}

TEST(RadarFrameSnapshot, ReappearingPlayerUsesConfirmedPositionAcrossPacketGap)
{
    goldsrc::cl_entity_t entity{};
    entity.prevstate.messagenum = 40;
    entity.curstate.messagenum = 43;
    entity.curstate.origin[0] = 300.0f;
    entity.origin[0] = 100.0f;
    EXPECT_FLOAT_EQ(RadarFrame_GetPlayerPosition(entity).x, 300.0f);
    entity.prevstate.messagenum = 43;
    entity.curstate.messagenum = 44;
    entity.origin[0] = 310.0f;
    entity.curstate.origin[0] = 330.0f;
    EXPECT_FLOAT_EQ(RadarFrame_GetPlayerPosition(entity).x, 330.0f);
    entity.curstate.effects = goldsrc::EF_NOINTERP;
    EXPECT_FLOAT_EQ(RadarFrame_GetPlayerPosition(entity).x, 330.0f);
}

TEST(RadarViewEdgeShift, EveryMapEdgeKeepsPaddingInRoundAndRotatedSquareViews)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    for (bool round : {false, true})
    {
        for (bool rotated : {false, true})
        {
            for (float yaw : {0.0f, 37.0f, 90.0f, 135.0f})
            {
                for (int axis = 0; axis < 2; axis++)
                {
                    for (float side : {-1.0f, 1.0f})
                    {
                        SCOPED_TRACE(testing::Message() << round << " " << rotated << " " << yaw << " " << axis << " " << side);
                        RadarViewParams p = BaseParams();
                        p.round = round;
                        p.ov_rotated = rotated;
                        p.rotate = true;
                        p.view_yaw = yaw;
                        p.map_bounds = ncl_math::Aabb2{{-5000.0f, -5000.0f}, {5000.0f, 5000.0f}};
                        p.view_origin[axis] = side * 4700.0f;
                        RadarTransform transform = RadarView_ComputeTransform(box, p);
                        ncl_math::Vector2 edge{};
                        edge[axis] = side * 5000.0f;
                        ncl_math::Vector2 normal{};
                        normal[axis] = 1.0f;
                        normal = transform.projection.ApplyVector(normal).Normalized();
                        float rim_distance = box.radius() * (round ? 1.0f : std::abs(normal.x) + std::abs(normal.y));
                        float edge_distance = std::abs((transform.WorldToScreen(edge) - box.center()).Dot(normal));
                        EXPECT_NEAR(rim_distance - edge_distance, rim_distance * (round ? 0.3f : 0.2f), 0.001f);
                        EXPECT_TRUE(RadarView_ComputeShape(box, round).Contains(transform.WorldToScreen(p.view_origin)));
                    }
                }
            }
        }
    }
}
