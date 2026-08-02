#include <gtest/gtest.h>

#include <cmath>

#include "RadarView.h"

namespace
{
    RadarViewParams BaseParams()
    {
        RadarViewParams p{};
        p.ov_zoom = 1.0f;
        p.ov_origin = {0.0f, 0.0f};
        p.ov_rotated = false;
        p.map_scale = 1.0f;
        p.rotate = false;
        p.fullmap = false;
        p.round = false;
        p.view_origin = {0.0f, 0.0f};
        p.view_yaw = 0.0f;
        p.sprite_frames = 0;
        return p;
    }
} // namespace

TEST(RadarViewComputeBox, AnchorsToScreenHeight)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    EXPECT_FLOAT_EQ(box.size, 1080.0f * 0.20f);
    EXPECT_FLOAT_EQ(box.x, 1080.0f * 0.02f);
    EXPECT_FLOAT_EQ(box.y, box.x);
    EXPECT_FLOAT_EQ(box.elem, box.size); // no damping at the reference resolution
}

TEST(RadarViewComputeBox, SpectatorMarginAndHiResDamping)
{
    RadarBox spec = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, true);
    EXPECT_GT(spec.y, spec.x); // pushed below the letterbox

    RadarBox hi = RadarView_ComputeBox(3840.0f, 2160.0f, 1.0f, false);
    EXPECT_FLOAT_EQ(hi.size, 2160.0f * 0.20f);
    EXPECT_LT(hi.elem, hi.size); // markers damped above the reference resolution
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
    EXPECT_NEAR(d0, d1, 1e-2f); // rotation preserves distances from the centre

    ncl_math::Vector2 m0 = t0.MarkerDirection(0.0f);
    ncl_math::Vector2 m1 = t1.MarkerDirection(90.0f);
    EXPECT_NEAR(m0.x, m1.x, 1e-3f); // facing rotates together with the map
    EXPECT_NEAR(m0.y, m1.y, 1e-3f);
}

TEST(RadarViewComputeTransform, FullmapRoundZoomsOutToFitTheDisc)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);
    RadarViewParams square = BaseParams();
    square.fullmap = true;
    RadarViewParams round = square;
    round.round = true;

    RadarTransform ts = RadarView_ComputeTransform(box, square);
    RadarTransform tr = RadarView_ComputeTransform(box, round);

    ncl_math::Vector2 w{1000.0f, 0.0f};
    float ds = (ts.WorldToScreen(w) - ts.center).Length();
    float dr = (tr.WorldToScreen(w) - tr.center).Length();
    EXPECT_LT(dr, ds); // the round variant shows more world per pixel
}

TEST(RadarViewComputeTransform, SpriteFrameCountDrivesTileGrid)
{
    RadarBox box = RadarView_ComputeBox(1920.0f, 1080.0f, 1.0f, false);

    RadarViewParams none = BaseParams(); // no sprite -> single zoom level (4x3)
    RadarViewParams one = BaseParams();
    one.sprite_frames = 12; // one zoom level
    RadarViewParams multi = BaseParams();
    multi.sprite_frames = 48; // two zoom levels -> 8x6 tiles

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

    // The follow centre still maps to the box centre in the rotated overview branch.
    ncl_math::Vector2 cr = tr.WorldToScreen(rot.view_origin);
    EXPECT_NEAR(cr.x, box.x + box.size * 0.5f, 1e-3f);
    EXPECT_NEAR(cr.y, box.y + box.size * 0.5f, 1e-3f);

    // The rotated branch re-derives the basis from swapped axes; for the square 4:3
    // tile grid the marker projection must still land at the same screen position
    // (only the map image orientation changes). A sign error in the rotated basis
    // assignment (basis_x = up / basis_y = -right) would break this equality.
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
    ncl_math::Vector2 r = RadarView_ClampToRadar(box, true, 10, p);
    ncl_math::Vector2 s = RadarView_ClampToRadar(box, false, 10, p);
    EXPECT_FLOAT_EQ(r.x, p.x);
    EXPECT_FLOAT_EQ(r.y, p.y);
    EXPECT_FLOAT_EQ(s.x, p.x);
    EXPECT_FLOAT_EQ(s.y, p.y);
}

TEST(RadarViewClampToRadar, RoundClampsToRim)
{
    RadarBox box{0.0f, 0.0f, 200.0f, 200.0f, 800.0f, 600.0f};
    ncl_math::Vector2 c = RadarView_ClampToRadar(box, true, 10, {300.0f, 100.0f});
    EXPECT_FLOAT_EQ(c.x, 190.0f); // centre 100 + (half 100 - size 10)
    EXPECT_FLOAT_EQ(c.y, 100.0f);
}

TEST(RadarViewClampToRadar, SquareClampsPerAxis)
{
    RadarBox box{0.0f, 0.0f, 200.0f, 200.0f, 800.0f, 600.0f};
    ncl_math::Vector2 c = RadarView_ClampToRadar(box, false, 10, {300.0f, -50.0f});
    EXPECT_FLOAT_EQ(c.x, 190.0f);
    EXPECT_FLOAT_EQ(c.y, 10.0f);
}
