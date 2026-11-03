#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include <ncl_math/Matrix3x4.h>
#include <ncl_studio/bone_animation.h>

namespace
{
    constexpr float kQuaternionTolerance = 2e-6f;
    constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0f;

    struct AnimationStorage
    {
        goldsrc::mstudioanim_t animation{};
        std::array<goldsrc::mstudioanimvalue_t, 64> words{};
    };

    struct ControllerStorage
    {
        goldsrc::studiohdr_t header{};
        std::array<goldsrc::mstudiobonecontroller_t, 4> controllers{};
    };

    goldsrc::mstudiobone_t MakeBone()
    {
        goldsrc::mstudiobone_t bone{};

        for (int axis = 0; axis < 6; ++axis)
        {
            bone.bonecontroller[axis] = -1;
            bone.scale[axis] = 1.0f;
        }

        return bone;
    }

    void SetChannel(AnimationStorage& storage, int axis, std::initializer_list<int16_t> words)
    {
        storage.animation.offset[axis] = offsetof(AnimationStorage, words);
        size_t index = 0;

        for (int16_t word : words)
        {
            storage.words[index++].value = word;
        }
    }

    ControllerStorage MakeController(int type, int input, float start, float end)
    {
        ControllerStorage storage{};
        storage.header.numbonecontrollers = 1;
        storage.header.bonecontrollerindex = offsetof(ControllerStorage, controllers);
        storage.controllers[0].type = type;
        storage.controllers[0].index = input;
        storage.controllers[0].start = start;
        storage.controllers[0].end = end;

        return storage;
    }

    void ExpectQuaternion(const ncl_math::Quaternion& actual, const std::array<float, 4>& expected)
    {
        for (int coordinate = 0; coordinate < 4; ++coordinate)
        {
            EXPECT_NEAR(actual[coordinate], expected[coordinate], kQuaternionTolerance) << coordinate;
        }
    }
} // namespace

TEST(StudioBoneAnimation, PositionSelectsExplicitHeldAndNextRunSamples)
{
    goldsrc::mstudiobone_t bone = MakeBone();
    bone.value[0] = 1;
    bone.scale[0] = 2;
    AnimationStorage storage{};
    SetChannel(storage, 0, {0x0503, 10, 20, 30, 0x0202, 100, 200, 0x0401, 300});
    const float expected[] = {26, 46, 61, 61, 96, 251, 401};

    for (int frame = 0; frame < 7; ++frame)
    {
        ncl_math::Vector3 position{};
        ncl_studio::CalcBonePosition(frame, 0.25f, bone, storage.animation, nullptr, position);

        EXPECT_EQ(position[0], expected[frame]) << frame;
    }
}

TEST(StudioBoneAnimation, QuaternionSamplesNextRunAfterFinalExplicitSample)
{
    goldsrc::mstudiobone_t bone = MakeBone();
    bone.scale[3] = 0.01f;
    AnimationStorage storage{};
    SetChannel(storage, 3, {0x0503, 10, 20, 30, 0x0202, 100, 200, 0x0401, 300});
    ncl_math::Quaternion quaternion{};
    ncl_studio::CalcBoneQuaternion(4, 0.25f, bone, storage.animation, nullptr, quaternion);
    ExpectQuaternion(quaternion, {0.2352735412f, 0, 0, 0.9719291954f});
    ncl_studio::CalcBoneQuaternion(6, 0.25f, bone, storage.animation, nullptr, quaternion);
    ExpectQuaternion(quaternion, {0.9022675941f, 0, 0, 0.4311765168f});
}

TEST(StudioBoneAnimation, MalformedRunResetsFrameOnInitialAndAdvancedBlocks)
{
    goldsrc::mstudiobone_t bone = MakeBone();
    bone.value[0] = 1;
    bone.scale[0] = 2;
    bone.scale[3] = 0.01f;

    for (bool preceding_run : {false, true})
    {
        AnimationStorage storage{};

        if (preceding_run)
        {
            SetChannel(storage, 0, {0x0201, 5, 0x0203, 10, 20, 30});
        }
        else
        {
            SetChannel(storage, 0, {0x0203, 10, 20, 30});
        }

        storage.animation.offset[3] = storage.animation.offset[0];

        ncl_math::Vector3 position{};
        ncl_math::Quaternion quaternion{};
        int frame = preceding_run ? 3 : 1;
        ncl_studio::CalcBonePosition(frame, 0.25f, bone, storage.animation, nullptr, position);
        ncl_studio::CalcBoneQuaternion(frame, 0.25f, bone, storage.animation, nullptr, quaternion);

        EXPECT_EQ(position[0], 26.0f);

        ExpectQuaternion(quaternion, {0.0624593178f, 0, 0, 0.9980475107f});
    }
}

TEST(StudioBoneAnimation, ZeroValidRunUsesStoredHeaderWord)
{
    goldsrc::mstudiobone_t bone = MakeBone();
    bone.value[0] = 1;
    bone.scale[0] = 2;
    AnimationStorage storage{};
    SetChannel(storage, 0, {0x0200, 0x0201, 100});
    ncl_math::Vector3 position{};
    ncl_studio::CalcBonePosition(0, 0.25f, bone, storage.animation, nullptr, position);

    EXPECT_EQ(position[0], 1025.0f);

    ncl_studio::CalcBonePosition(1, 0.25f, bone, storage.animation, nullptr, position);

    EXPECT_EQ(position[0], 819.0f);
}

TEST(StudioBoneAnimation, AbsentChannelsStillApplyControllersAndAllowNullPositionAdjustment)
{
    goldsrc::mstudiobone_t bone = MakeBone();
    bone.value[0] = 1;
    bone.value[1] = 2;
    bone.value[2] = 3;
    bone.bonecontroller[1] = 0;
    bone.bonecontroller[2] = 1;
    const float adjustment[] = {4, 8};
    AnimationStorage storage{};
    ncl_math::Vector3 position{};
    ncl_studio::CalcBonePosition(0, 0.5f, bone, storage.animation, adjustment, position);

    EXPECT_EQ(position[0], 1.0f);
    EXPECT_EQ(position[1], 6.0f);
    EXPECT_EQ(position[2], 11.0f);

    ncl_studio::CalcBonePosition(0, 0.5f, bone, storage.animation, nullptr, position);

    EXPECT_EQ(position[0], 1.0f);
    EXPECT_EQ(position[1], 2.0f);
    EXPECT_EQ(position[2], 3.0f);

    bone.bonecontroller[5] = 0;
    const float angle_adjustment[] = {kHalfPi};
    ncl_math::Quaternion quaternion{};
    ncl_studio::CalcBoneQuaternion(0, 0.5f, bone, storage.animation, angle_adjustment, quaternion);
    ExpectQuaternion(quaternion, {0, 0, 0.7071067812f, 0.7071067812f});
}

TEST(StudioBoneAnimation, PositionDoesNotClampFraction)
{
    goldsrc::mstudiobone_t bone = MakeBone();
    bone.value[0] = 1;
    bone.scale[0] = 2;
    AnimationStorage storage{};
    SetChannel(storage, 0, {0x0202, 10, 20});
    ncl_math::Vector3 position{};
    ncl_studio::CalcBonePosition(0, -0.5f, bone, storage.animation, nullptr, position);

    EXPECT_EQ(position[0], 11.0f);

    ncl_studio::CalcBonePosition(0, 1.5f, bone, storage.animation, nullptr, position);

    EXPECT_EQ(position[0], 51.0f);
}

TEST(StudioBoneAnimation, HeldPositionStoresFloatBeforeControllerAdjustment)
{
    goldsrc::mstudiobone_t bone = MakeBone();
    bone.value[0] = 16777216.0f;
    bone.bonecontroller[0] = 0;
    const float adjustment[] = {-16777216.0f};
    AnimationStorage storage{};
    SetChannel(storage, 0, {0x0201, 1});
    ncl_math::Vector3 position = {7, 7, 7};
    ncl_studio::CalcBonePosition(0, 0.25f, bone, storage.animation, adjustment, position);

    EXPECT_EQ(position[0], 0.0f);

    SetChannel(storage, 0, {0x0202, 1, 1});
    ncl_studio::CalcBonePosition(0, 0.25f, bone, storage.animation, adjustment, position);

    EXPECT_EQ(position[0], 0.0f);
}

TEST(StudioBoneAnimation, QuaternionEndpointStoresFloatBeforeControllerAdjustment)
{
    goldsrc::mstudiobone_t bone = MakeBone();
    bone.value[3] = 16777216.0f;
    bone.bonecontroller[3] = 0;
    const float adjustment[] = {-16777216.0f};
    AnimationStorage storage{};
    SetChannel(storage, 3, {0x0201, 1});
    ncl_math::Quaternion quaternion = {7, 7, 7, 7};
    ncl_studio::CalcBoneQuaternion(0, 0.25f, bone, storage.animation, adjustment, quaternion);
    ExpectQuaternion(quaternion, {0, 0, 0, 1});
}

TEST(StudioBoneAnimation, ControllerInputIndexDoesNotChooseOutputRow)
{
    ControllerStorage storage = MakeController(goldsrc::STUDIO_X, 2, 10, 265);
    const uint8_t current[4] = {0, 0, 64, 0};
    const uint8_t previous[4] = {0, 0, 192, 0};
    float adjustment[4] = {-1000, -1001, -1002, -1003};
    ncl_studio::CalcBoneAdj(storage.header, 0.25f, adjustment, current, previous, 0);

    EXPECT_FLOAT_EQ(adjustment[0], 170.0f);
    EXPECT_EQ(adjustment[1], -1001.0f);
    EXPECT_EQ(adjustment[2], -1002.0f);
    EXPECT_EQ(adjustment[3], -1003.0f);
}

TEST(StudioBoneAnimation, WrappedControllerUsesRowBytesAfterIndexThreshold)
{
    ControllerStorage storage = MakeController(goldsrc::STUDIO_RLOOP | goldsrc::STUDIO_X, 2, 5, 0);
    const uint8_t current[4] = {40, 0, 250, 0};
    const uint8_t previous[4] = {200, 0, 10, 0};
    float adjustment[4]{};
    ncl_studio::CalcBoneAdj(storage.header, 0.25f, adjustment, current, previous, 0);

    EXPECT_EQ(adjustment[0], -40.0f);
}

TEST(StudioBoneAnimation, WrappedControllerThresholdIsStrictlyGreaterThan128)
{
    ControllerStorage storage = MakeController(goldsrc::STUDIO_RLOOP | goldsrc::STUDIO_X, 0, 0, 0);
    uint8_t current[4] = {192, 0, 0, 0};
    const uint8_t previous[4] = {64, 0, 0, 0};
    float adjustment[4]{};
    ncl_studio::CalcBoneAdj(storage.header, 0.25f, adjustment, current, previous, 0);

    EXPECT_EQ(adjustment[0], 135.0f);

    current[0] = 193;
    ncl_studio::CalcBoneAdj(storage.header, 0.25f, adjustment, current, previous, 0);

    EXPECT_EQ(adjustment[0], 45.3515625f);
}

TEST(StudioBoneAnimation, OrdinaryControllerClampsFactorAndMouthHasUpperLimit)
{
    ControllerStorage storage = MakeController(goldsrc::STUDIO_X, 0, 10, 265);
    const uint8_t current[4] = {255, 0, 0, 0};
    const uint8_t previous[4]{};
    float adjustment[4]{};
    ncl_studio::CalcBoneAdj(storage.header, 2.0f, adjustment, current, previous, 0);

    EXPECT_EQ(adjustment[0], 265.0f);

    ncl_studio::CalcBoneAdj(storage.header, -1.0f, adjustment, current, previous, 0);

    EXPECT_EQ(adjustment[0], 10.0f);

    storage.controllers[0].index = 4;
    storage.controllers[0].end = 90;
    ncl_studio::CalcBoneAdj(storage.header, 0.25f, adjustment, current, previous, 32);

    EXPECT_EQ(adjustment[0], 50.0f);

    ncl_studio::CalcBoneAdj(storage.header, 0.25f, adjustment, current, previous, 128);

    EXPECT_EQ(adjustment[0], 90.0f);
}

TEST(StudioBoneAnimation, ControllerDispatchConvertsOnlyAngularTypes)
{
    ControllerStorage storage = MakeController(goldsrc::STUDIO_XR, 0, 90, 90);
    storage.header.numbonecontrollers = 3;
    storage.controllers[1] = storage.controllers[0];
    storage.controllers[1].type = goldsrc::STUDIO_X;
    storage.controllers[2] = storage.controllers[0];
    storage.controllers[2].type = 3;
    const uint8_t input[4]{};
    float adjustment[4] = {-1, -2, -3, -4};
    ncl_studio::CalcBoneAdj(storage.header, 0.25f, adjustment, input, input, 0);
    uint32_t angle_bits = 0;
    std::memcpy(&angle_bits, &adjustment[0], sizeof(angle_bits));

    EXPECT_EQ(angle_bits, 0x3fc90fdbu);
    EXPECT_EQ(adjustment[1], 90.0f);
    EXPECT_EQ(adjustment[2], -3.0f);
    EXPECT_EQ(adjustment[3], -4.0f);
}
