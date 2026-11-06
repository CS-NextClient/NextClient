#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "GameUi/MaterialEditor/MaterialPreviewState.h"

using material_editor::MaterialPreviewState;
using material_editor::MaterialLightPreset;

TEST(MaterialPreviewState, AnimatedSunOrbitsAboveTheModelWithoutPlaybackOrTheGizmo)
{
    MaterialPreviewState state;
    state.SetFrame(0.4f);
    state.SetPlaybackRate(2.0f);
    state.SetSunGizmo(false);
    state.SetSunAnimating(true);
    PreviewLight initial = state.BuildLight();
    EXPECT_FLOAT_EQ(state.get_light_elevation(), 35.0f);
    state.Advance(10.0);
    state.Advance(10.1);

    EXPECT_GT(std::abs(state.BuildLight().direction[1] - initial.direction[1]), 0.01f);
    EXPECT_FLOAT_EQ(state.get_light_elevation(), 35.0f);
    EXPECT_FLOAT_EQ(state.get_frame(), 0.4f);
    EXPECT_FALSE(state.is_playing());
    EXPECT_FALSE(state.is_sun_gizmo_shown());
    EXPECT_TRUE(state.is_sun_animating());
}

TEST(MaterialPreviewState, SunOrbitKeepsItsHeightAndWrapsContinuouslyIn24Seconds)
{
    MaterialPreviewState state;
    state.SetLightAngles(350.0f, 40.0f);
    state.SetLightColor(96, 128, 160);
    state.SetLightStrengths(0.25f, 0.75f);
    state.SetSunAnimating(true);
    PreviewLight initial = state.BuildLight();
    PreviewLight previous = initial;
    state.Advance(20.0);

    for (int step = 1; step <= 480; step++)
    {
        state.Advance(20.0 + step * 0.05);
        PreviewLight current = state.BuildLight();
        float dot = 0.0f;
        float length = 0.0f;

        for (int axis = 0; axis < 3; axis++)
        {
            dot += previous.direction[axis] * current.direction[axis];
            length += current.direction[axis] * current.direction[axis];
        }

        EXPECT_GT(dot, 0.999f) << step;
        EXPECT_NEAR(length, 1.0f, 0.00001f);
        EXPECT_FLOAT_EQ(state.get_light_elevation(), 40.0f);
        EXPECT_LT(current.direction[2], 0.0f);
        EXPECT_GE(state.get_light_azimuth(), 0.0f);
        EXPECT_LT(state.get_light_azimuth(), 360.0f);
        previous = current;
    }

    for (int axis = 0; axis < 3; axis++)
    {
        EXPECT_NEAR(previous.direction[axis], initial.direction[axis], 0.0001f);
        EXPECT_FLOAT_EQ(previous.color[axis], initial.color[axis]);
    }

    EXPECT_FLOAT_EQ(previous.ambient, initial.ambient);
    EXPECT_FLOAT_EQ(previous.diffuse, initial.diffuse);
}

TEST(MaterialPreviewState, SunClockContinuesAcrossModelAndPlaybackChanges)
{
    MaterialPreviewState state;
    state.SetLightAngles(40.0f, 20.0f);
    state.SetSunAnimating(true);
    state.Advance(10.0);
    state.Advance(10.1);
    state.SetModel("models/test.mdl", {});
    state.SetSequence(2, 1.0f);
    state.SetPlaybackRate(0.25f);
    state.SetPlaying(true);
    state.Advance(10.2);

    EXPECT_NEAR(state.get_light_azimuth(), 43.0f, 0.00001f);
    EXPECT_FLOAT_EQ(state.get_light_elevation(), 20.0f);
    EXPECT_TRUE(state.is_sun_animating());
    EXPECT_FLOAT_EQ(state.get_frame(), 0.0f);
}

TEST(MaterialPreviewState, SunStopResumeAndResetDoNotConsumeStoppedTime)
{
    MaterialPreviewState state;
    state.SetLightAngles(40.0f, 20.0f);
    state.SetSunAnimating(true);
    state.Advance(10.0);
    state.Advance(10.1);
    state.SetSunAnimating(false);
    state.Advance(1000.0);
    EXPECT_NEAR(state.get_light_azimuth(), 41.5f, 0.00001f);
    state.SetSunAnimating(true);
    state.Advance(2000.0);
    EXPECT_NEAR(state.get_light_azimuth(), 41.5f, 0.00001f);
    state.Advance(2000.1);
    EXPECT_NEAR(state.get_light_azimuth(), 43.0f, 0.00001f);
    state.Reset();
    EXPECT_FALSE(state.is_sun_animating());
}

TEST(MaterialPreviewState, SunOrbitSelectsAUsefulHeightAndAllowsLiveAngleChanges)
{
    const float elevations[] = {-90.0f, 0.0f, 90.0f, 5.0f, 20.0f, 80.0f};

    for (float elevation : elevations)
    {
        MaterialPreviewState state;
        state.SetLightAngles(40.0f, elevation);
        state.SetSunAnimating(true);
        EXPECT_FLOAT_EQ(state.get_light_azimuth(), 40.0f);
        EXPECT_FLOAT_EQ(state.get_light_elevation(), elevation < 5.0f || elevation > 80.0f ? 35.0f : elevation);
    }

    MaterialPreviewState state;
    state.SetSunAnimating(true);
    state.Advance(10.0);
    state.SetLightAngles(450.0f, -20.0f);
    EXPECT_FLOAT_EQ(state.get_light_elevation(), 5.0f);
    state.Advance(10.1);
    EXPECT_NEAR(state.get_light_azimuth(), 91.5f, 0.00001f);
    EXPECT_FLOAT_EQ(state.get_light_elevation(), 5.0f);
    state.SetLightAngles(25.0f, 89.0f);
    EXPECT_FLOAT_EQ(state.get_light_elevation(), 80.0f);
    state.SetSunAnimating(false);
    state.SetLightAngles(25.0f, -45.0f);
    EXPECT_FLOAT_EQ(state.get_light_elevation(), -45.0f);
}

TEST(MaterialPreviewState, SunSpeedChangesLiveWithoutResettingItsClockOrModelPlayback)
{
    MaterialPreviewState state;
    state.SetLightAngles(40.0f, 20.0f);
    state.SetSunSpeed(0.25f);
    state.SetSunAnimating(true);
    state.SetSequence(0, 2.0f);
    state.SetPlaying(true);
    state.Advance(10.0);
    state.Advance(10.1);
    EXPECT_NEAR(state.get_light_azimuth(), 40.375f, 0.00001f);
    EXPECT_NEAR(state.get_frame(), 0.05f, 0.00001f);
    state.SetSunSpeed(4.0f);
    state.Advance(10.2);
    EXPECT_NEAR(state.get_light_azimuth(), 46.375f, 0.00001f);
    EXPECT_FLOAT_EQ(state.get_light_elevation(), 20.0f);
    EXPECT_NEAR(state.get_frame(), 0.1f, 0.00001f);
    state.SetSunAnimating(false);
    state.SetSunSpeed(2.0f);
    state.Advance(1000.0);
    state.SetSunAnimating(true);
    state.Advance(2000.0);
    EXPECT_NEAR(state.get_light_azimuth(), 46.375f, 0.00001f);
    state.Advance(2000.1);
    EXPECT_NEAR(state.get_light_azimuth(), 49.375f, 0.00001f);
}

TEST(MaterialPreviewState, SunSpeedClampsRejectsNonfiniteAndSurvivesModelAndPresetChanges)
{
    MaterialPreviewState state;
    EXPECT_FLOAT_EQ(state.get_sun_speed(), 1.0f);
    state.SetSunSpeed(0.0f);
    EXPECT_FLOAT_EQ(state.get_sun_speed(), 0.25f);
    state.SetSunSpeed(10.0f);
    EXPECT_FLOAT_EQ(state.get_sun_speed(), 4.0f);
    state.SetSunSpeed(std::numeric_limits<float>::quiet_NaN());
    state.SetSunSpeed(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(state.get_sun_speed(), 4.0f);
    state.SetSunSpeed(2.5f);
    state.SetSunAnimating(true);
    state.SetModel("models/test.mdl", {});
    state.SetLightPreset(MaterialLightPreset::Neutral);
    EXPECT_FLOAT_EQ(state.get_sun_speed(), 2.5f);
    EXPECT_TRUE(state.is_sun_animating());
    EXPECT_FLOAT_EQ(state.get_light_elevation(), 80.0f);
    state.Reset();
    EXPECT_FLOAT_EQ(state.get_sun_speed(), 1.0f);
    EXPECT_FALSE(state.is_sun_animating());
}

TEST(MaterialPreviewState, LightingPresetsUseReproducibleStudioTerms)
{
    MaterialPreviewState state;
    PreviewLight light = state.BuildLight();
    EXPECT_NEAR(light.direction[0], 0.0f, 0.00001f);
    EXPECT_NEAR(light.direction[2], -1.0f, 0.00001f);
    EXPECT_EQ(light.model_ambient, 64);
    EXPECT_EQ(light.model_shade, 128);

    state.SetLightPreset(MaterialLightPreset::AmbientOnly);
    light = state.BuildLight();
    EXPECT_EQ(light.model_ambient, 128);
    EXPECT_EQ(light.model_shade, 0);

    state.SetLightPreset(MaterialLightPreset::Grazing);
    light = state.BuildLight();
    EXPECT_NEAR(light.direction[0], -1.0f, 0.00001f);
    EXPECT_NEAR(light.direction[2], 0.0f, 0.00001f);
    EXPECT_EQ(light.model_ambient, 19);
    EXPECT_EQ(light.model_shade, 217);
}

TEST(MaterialPreviewState, LightInputsAreClampedAndNonfiniteValuesAreRejected)
{
    MaterialPreviewState state;
    state.SetLightAngles(450.0f, -120.0f);
    state.SetLightStrengths(-1.0f, 4.0f);
    state.SetLightColor(-1, 128, 999);
    PreviewLight light = state.BuildLight();
    EXPECT_NEAR(light.direction[2], 1.0f, 0.00001f);
    EXPECT_FLOAT_EQ(light.ambient, 0.0f);
    EXPECT_FLOAT_EQ(light.diffuse, 1.0f);
    EXPECT_FLOAT_EQ(light.color[0], 0.0f);
    EXPECT_NEAR(light.color[1], 0.5019608f, 0.000001f);
    EXPECT_FLOAT_EQ(light.color[2], 1.0f);

    state.SetLightStrengths(std::numeric_limits<float>::quiet_NaN(), 0.2f);
    state.SetLightAngles(std::numeric_limits<float>::infinity(), 0.0f);
    light = state.BuildLight();
    EXPECT_FLOAT_EQ(light.diffuse, 1.0f);
    EXPECT_NEAR(light.direction[2], 1.0f, 0.00001f);
}

TEST(MaterialPreviewState, AnimationUsesTheOwnedClockAndSequenceDuration)
{
    MaterialPreviewState state;
    state.SetSequence(1, 2.0f);
    state.SetPlaying(true);
    state.Advance(20.0);

    for (int index = 1; index <= 100; index++)
    {
        state.Advance(20.0 + index * 0.01);
    }

    EXPECT_NEAR(state.get_frame(), 0.5f, 0.00001f);
    EXPECT_EQ(state.BuildModel().sequence, 1);
}

TEST(MaterialPreviewState, ResumingPlaybackDoesNotConsumeTimeSpentPaused)
{
    MaterialPreviewState state;
    state.SetSequence(0, 1.0f);
    state.SetFrame(0.25f);
    state.SetPlaying(true);
    state.Advance(10.0);
    state.Advance(10.1);
    state.SetPlaying(false);
    state.Advance(100.0);
    state.SetPlaying(true);
    state.Advance(200.0);

    EXPECT_NEAR(state.get_frame(), 0.35f, 0.00001f);
}

TEST(MaterialPreviewState, ChangingModelResetsAnimationAndKeepsTheSceneLight)
{
    MaterialPreviewState state;
    state.SetLightPreset(MaterialLightPreset::Grazing);
    state.SetSequence(3, 0.5f);
    state.SetFrame(0.75f);
    PreviewModelInfo info{};
    info.bounds_min[0] = -10.0f;
    info.bounds_max[0] = 10.0f;
    state.SetModel("models/test.mdl", info);

    EXPECT_STREQ(state.BuildModel().path, "models/test.mdl");
    EXPECT_FLOAT_EQ(state.get_frame(), 0.0f);
    EXPECT_EQ(state.BuildModel().sequence, 0);
    EXPECT_EQ(state.BuildLight().model_ambient, 19);
}

TEST(MaterialPreviewState, OrbitingTheCameraDoesNotRotateTheSceneLight)
{
    MaterialPreviewState state;
    state.SetLightPreset(MaterialLightPreset::Grazing);
    state.Orbit(90.0f, 0.0f);
    PreviewCamera camera = state.BuildCamera();

    EXPECT_GT(camera.origin[1], 0.0f);
    EXPECT_NEAR(state.BuildLight().direction[0], -1.0f, 0.00001f);
}

TEST(MaterialPreviewState, ResetRestoresNeutralLightAndClearsTheModel)
{
    MaterialPreviewState state;
    state.SetModel("models/test.mdl", {});
    state.SetLightPreset(MaterialLightPreset::Grazing);
    state.Reset();

    EXPECT_STREQ(state.BuildModel().path, "");
    EXPECT_EQ(state.BuildLight().model_ambient, 64);
    EXPECT_EQ(state.BuildLight().model_shade, 128);
}

TEST(MaterialPreviewState, WeaponViewSupportsRotationPanAndZoom)
{
    MaterialPreviewState state;
    state.SetWeaponView(true);
    state.Orbit(20.0f, 10.0f);
    state.Pan(10.0f, 5.0f);
    state.Zoom(1.0f);
    PreviewModel model = state.BuildModel();

    EXPECT_FLOAT_EQ(model.angles[0], 10.0f);
    EXPECT_FLOAT_EQ(model.angles[1], 20.0f);
    EXPECT_NE(model.origin[1], 0.0f);
    EXPECT_NE(model.origin[2], 0.0f);
    EXPECT_LT(model.fov, 90.0f);
}

TEST(MaterialPreviewState, PlaybackRateScalesAndWrapsAnimationWithoutChangingClockTime)
{
    MaterialPreviewState state;
    state.SetSequence(0, 2.0f);
    state.SetPlaybackRate(2.0f);
    state.SetFrame(0.98f);
    state.SetPlaying(true);
    state.Advance(10.0);
    state.Advance(10.1);

    EXPECT_NEAR(state.get_frame(), 0.08f, 0.00001f);
    EXPECT_NEAR(state.get_elapsed(), 0.1f, 0.00001f);
    state.SetPlaybackRate(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(state.get_playback_rate(), 2.0f);
}

TEST(MaterialPreviewState, FrameModelTargetsTheScaledAndRotatedBoundsCenter)
{
    struct Case
    {
        float pitch;
        float yaw;
        float scale;
        ncl_math::Vector3 expected_center;
    };

    const Case cases[] = {
        {0.0f, 0.0f, 2.0f, {200.0f, 40.0f, 20.0f}},
        {0.0f, 90.0f, 1.0f, {-20.0f, 100.0f, 10.0f}},
        {30.0f, 90.0f, 2.0f, {-40.0f, 163.20508f, 117.32051f}},
    };

    for (const Case& test_case : cases)
    {
        SCOPED_TRACE(test_case.yaw);
        MaterialPreviewState state;
        PreviewModelInfo info{};
        info.bounds_min[0] = 99.0f;
        info.bounds_min[1] = 19.0f;
        info.bounds_min[2] = 9.0f;
        info.bounds_max[0] = 101.0f;
        info.bounds_max[1] = 21.0f;
        info.bounds_max[2] = 11.0f;
        state.SetModel("models/offset.mdl", info);
        state.SetPresentation(0, 0, test_case.pitch, test_case.yaw, test_case.scale);
        state.FrameModel();
        PreviewCamera camera = state.BuildCamera();
        ncl_math::Vector3 forward;
        ncl_math::AngleVectors(camera.angles, forward.data(), nullptr, nullptr);
        ncl_math::Vector3 offset = test_case.expected_center - ncl_math::Vector3::Ref(camera.origin);

        EXPECT_NEAR(offset.Normalized().Dot(forward), 1.0f, 0.00001f);
    }
}

TEST(MaterialPreviewState, ResetWeaponViewRestoresTheViewAndPreservesSceneSettings)
{
    MaterialPreviewState state;
    state.SetWeaponView(true);
    state.SetPresentation(2, 3, 20.0f, 30.0f, 2.0f);
    state.SetSequence(1, 2.0f);
    state.SetFrame(0.6f);
    state.SetPlaying(true);
    state.SetMirror(true);
    state.SetLightPreset(MaterialLightPreset::Grazing);
    state.Pan(40.0f, -20.0f);
    state.Zoom(3.0f);
    state.ResetView();
    PreviewModel model = state.BuildModel();

    EXPECT_FLOAT_EQ(model.origin[0], 0.0f);
    EXPECT_FLOAT_EQ(model.origin[1], 0.0f);
    EXPECT_FLOAT_EQ(model.origin[2], 0.0f);
    EXPECT_FLOAT_EQ(model.angles[0], 0.0f);
    EXPECT_FLOAT_EQ(model.angles[1], 0.0f);
    EXPECT_FLOAT_EQ(model.fov, 90.0f);
    EXPECT_EQ(model.skin, 2);
    EXPECT_EQ(model.body, 3);
    EXPECT_FLOAT_EQ(model.scale, 2.0f);
    EXPECT_EQ(model.sequence, 1);
    EXPECT_FLOAT_EQ(model.frame, 0.6f);
    EXPECT_TRUE(model.mirror);
    EXPECT_TRUE(state.is_playing());
    EXPECT_EQ(state.BuildLight().model_shade, 217);
}

TEST(MaterialPreviewState, SunGizmoVisibilityDoesNotChangeLighting)
{
    MaterialPreviewState state;
    EXPECT_TRUE(state.is_sun_gizmo_shown());
    state.SetLightPreset(MaterialLightPreset::Grazing);
    state.SetSunGizmo(false);

    EXPECT_FALSE(state.is_sun_gizmo_shown());
    EXPECT_EQ(state.BuildLight().model_shade, 217);
    state.Reset();
    EXPECT_TRUE(state.is_sun_gizmo_shown());
}

TEST(MaterialPreviewState, OccluderTogglePreservesModelCameraAndLightingAndResetsIndependently)
{
    MaterialPreviewState state;
    EXPECT_FALSE(state.is_occluder_shown());
    state.SetWeaponView(true);
    state.SetPresentation(1, 2, 10, 20, 2);
    state.SetLightPreset(MaterialLightPreset::Grazing);
    const PreviewCamera camera = state.BuildCamera();
    state.SetOccluder(true);
    EXPECT_TRUE(state.is_occluder_shown());
    EXPECT_FLOAT_EQ(state.BuildCamera().fov, camera.fov);
    EXPECT_FLOAT_EQ(state.BuildModel().scale, 2);
    EXPECT_EQ(state.BuildLight().model_shade, 217);
    state.ResetView();
    EXPECT_TRUE(state.is_occluder_shown());
    state.Reset();
    EXPECT_FALSE(state.is_occluder_shown());
}
