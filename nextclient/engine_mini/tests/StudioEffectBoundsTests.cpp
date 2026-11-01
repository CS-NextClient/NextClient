#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>
#include <ncl_math/aabb.h>

#include "graphics/render/effects/studio_effect_bounds.h"
#include "graphics/render/model/StudioGeometry.h"
#include "graphics/render/StudioQueue.h"

namespace
{
    using Rect_t = render::effects::Rect_t;

    constexpr std::array<int, 5> kBoundsCounts = {1, 8, 32, 64, 128};

    struct BoundsInput
    {
        render::StudioGpuModel model{};
        std::array<float, render::kStudioBoneFloats> palette{};
        render::StudioPosePool pool;
        render::StudioDrawPacket packet{};
        float radius{};

        explicit BoundsInput(int count = 1)
        {
            packet.viewport = {17, 29, 100, 80};
            packet.modelview = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
            };
            packet.projection = packet.modelview;

            for (size_t bone = 0; bone < render::kStudioBoneFloats / 12; ++bone)
            {
                palette[bone * 12] = 1;
                palette[bone * 12 + 5] = 1;
                palette[bone * 12 + 10] = 1;
            }

            for (int bone = 0; bone < count; ++bone)
            {
                model.bounds.push_back({bone, {-0.5f, -0.25f, -0.1f}, {0.5f, 0.25f, 0.1f}});
            }

            Refresh();
        }

        void Refresh()
        {
            packet.bones = pool.Capture(palette.data());
        }

        void ConstantClip(float x, float y, float z, float w)
        {
            packet.projection.fill(0);
            packet.projection[12] = x;
            packet.projection[13] = y;
            packet.projection[14] = z;
            packet.projection[15] = w;
        }

        Rect_t Project() const
        {
            return render::effects::ProjectBounds(model, packet, radius);
        }

        Rect_t Full() const
        {
            return {0, 0, packet.viewport[2], packet.viewport[3]};
        }
    };

    // Preserve the scalar operation order as an independent reference for SIMD changes.
    Rect_t ScalarProjectBounds(const render::StudioGpuModel& model, const render::StudioDrawPacket& packet, float radius)
    {
        const Rect_t full = {0, 0, packet.viewport[2], packet.viewport[3]};

        if (model.bounds.empty())
        {
            return full;
        }

        ncl_math::Aabb2 bounds{{1.0f, 1.0f}, {-1.0f, -1.0f}};

        for (const render::StudioBoneBounds& box : model.bounds)
        {
            const float* bone = packet.bones.data() + box.bone * 12;

            for (int corner = 0; corner < 8; corner++)
            {
                float local[4] = {box.minimum[0], box.minimum[1], box.minimum[2], 1};

                for (int axis = 0; axis < 3; axis++)
                {
                    if ((corner & (1 << axis)) != 0)
                    {
                        local[axis] = box.maximum[axis];
                    }
                }

                float world[4] = {0, 0, 0, 1};
                float eye[4]{};
                float clip[4]{};

                for (int row = 0; row < 3; row++)
                {
                    for (int column = 0; column < 4; column++)
                    {
                        world[row] += bone[row * 4 + column] * local[column];
                    }
                }

                for (int row = 0; row < 4; row++)
                {
                    for (int column = 0; column < 4; column++)
                    {
                        eye[row] += packet.modelview[column * 4 + row] * world[column];
                    }
                }

                for (int row = 0; row < 4; row++)
                {
                    for (int column = 0; column < 4; column++)
                    {
                        clip[row] += packet.projection[column * 4 + row] * eye[column];
                    }
                }

                if (!std::isfinite(clip[3]) || clip[3] <= 0.00001f || clip[2] <= -clip[3])
                {
                    return full;
                }

                ncl_math::Vector2 projected;

                for (int axis = 0; axis < 2; axis++)
                {
                    const float value = std::clamp(clip[axis] / clip[3], -1.0f, 1.0f);

                    if (!std::isfinite(value))
                    {
                        return full;
                    }

                    projected[axis] = value;
                }

                bounds.Include(projected);
            }
        }

        const int x = std::clamp(static_cast<int>(std::floor((bounds.min.x + 1) * 0.5f * full[2] - radius)), 0, full[2]);
        const int y = std::clamp(static_cast<int>(std::floor((bounds.min.y + 1) * 0.5f * full[3] - radius)), 0, full[3]);
        const int right = std::clamp(static_cast<int>(std::ceil((bounds.max.x + 1) * 0.5f * full[2] + radius)), 0, full[2]);
        const int top = std::clamp(static_cast<int>(std::ceil((bounds.max.y + 1) * 0.5f * full[3] + radius)), 0, full[3]);

        return {x, y, right - x, top - y};
    }

    void ExpectScalar(const BoundsInput& input)
    {
        EXPECT_EQ(input.Project(), ScalarProjectBounds(input.model, input.packet, input.radius));
    }

    uint32_t NextRandom(uint32_t& state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        return state;
    }

    float RandomRange(uint32_t& state, float low, float high)
    {
        return low + static_cast<float>(NextRandom(state) & 0xffffff) * (1.0f / 16777216.0f) * (high - low);
    }
} // namespace

TEST(StudioEffectBoundsTest, ProjectsFixedBoundsAndExpandsInViewportLocalPixels)
{
    BoundsInput input;

    EXPECT_EQ(input.Project(), (Rect_t{25, 30, 50, 20}));

    input.radius = 2.25f;

    EXPECT_EQ(input.Project(), (Rect_t{22, 27, 56, 26}));

    input.radius = 1000;

    EXPECT_EQ(input.Project(), input.Full());
}

TEST(StudioEffectBoundsTest, CombinesRowMajorLastBoneWithColumnMajorViewAndProjection)
{
    BoundsInput input;
    input.packet.viewport = {117, 229, 128, 128};
    const int last_bone = goldsrc::MAXSTUDIOBONES - 1;
    input.model.bounds.front().bone = last_bone;
    float* bone = input.palette.data() + last_bone * 12;
    bone[0] = 0.5f;
    bone[3] = 0.125f;
    bone[5] = 0.25f;
    bone[7] = -0.125f;
    input.packet.modelview = {
        2, -0.5f, 0, 0,
        0.25f, 1, 0, 0,
        0, 0, 1, 0,
        0.125f, 0.25f, 0, 1
    };
    input.packet.projection = {
        0.5f, 0, 0, 0,
        0, 2, 0, 0,
        0, 0, 1, 0,
        0.25f, -0.125f, 0, 1
    };
    input.Refresh();

    EXPECT_EQ(input.Project(), (Rect_t{74, 40, 34, 48}));

    ExpectScalar(input);
}

TEST(StudioEffectBoundsTest, HandlesEmptyBoundsWithoutPoseOrFiniteMatrices)
{
    BoundsInput input(0);
    input.packet.bones = {};
    input.packet.projection.fill(std::numeric_limits<float>::quiet_NaN());

    EXPECT_EQ(input.Project(), (Rect_t{0, 0, 100, 80}));
}

TEST(StudioEffectBoundsTest, HandlesZeroViewportDimensionsAndDegenerateBounds)
{
    BoundsInput input;
    input.model.bounds.front().maximum = input.model.bounds.front().minimum;

    EXPECT_EQ(input.Project(), (Rect_t{25, 30, 0, 0}));

    for (int width : {0, 1, 100})
    {
        for (int height : {0, 1, 80})
        {
            SCOPED_TRACE(::testing::Message() << "viewport=" << width << 'x' << height);
            input.packet.viewport[2] = width;
            input.packet.viewport[3] = height;
            ExpectScalar(input);
        }
    }

    input.packet.viewport = {17, 29, 0, 0};

    EXPECT_EQ(input.Project(), (Rect_t{0, 0, 0, 0}));
}

TEST(StudioEffectBoundsTest, ClipsCoordinatesAndPreservesDivisionOverflowClamp)
{
    BoundsInput input;
    input.ConstantClip(2, -3, 0, 1);

    EXPECT_EQ(input.Project(), (Rect_t{100, 0, 0, 0}));

    const float maximum = (std::numeric_limits<float>::max)();
    input.ConstantClip(maximum, -maximum, 0, 0.0001f);

    EXPECT_EQ(input.Project(), (Rect_t{100, 0, 0, 0}));

    ExpectScalar(input);
}

TEST(StudioEffectBoundsTest, PreservesNonfiniteClipSemantics)
{
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    for (int axis = 0; axis < 4; ++axis)
    {
        for (float special : {nan, infinity, -infinity})
        {
            SCOPED_TRACE(::testing::Message() << "axis=" << axis << " value=" << special);
            BoundsInput input;
            input.ConstantClip(0, 0, 0, 1);
            input.packet.projection[12 + axis] = special;
            Rect_t expected = {50, 40, 0, 0};

            if (axis == 3 || (std::isnan(special) && axis < 2) || (axis == 2 && special == -infinity))
            {
                expected = input.Full();
            }
            else if (axis < 2)
            {
                expected[axis] = special > 0 ? input.packet.viewport[axis + 2] : 0;
            }

            EXPECT_EQ(input.Project(), expected);

            ExpectScalar(input);
        }
    }
}

TEST(StudioEffectBoundsTest, AppliesNearPlaneFallbackAtEveryCorner)
{
    for (int corner = 0; corner < 8; ++corner)
    {
        SCOPED_TRACE(corner);
        BoundsInput input;
        input.model.bounds.front().minimum = {-1, -1, -1};
        input.model.bounds.front().maximum = {1, 1, 1};
        input.packet.projection[0] = 0.125f;
        input.packet.projection[5] = 0.125f;
        input.packet.projection[14] = -0.25f;

        for (int axis = 0; axis < 3; ++axis)
        {
            input.packet.projection[axis * 4 + 2] = (corner & (1 << axis)) != 0 ? -0.25f : 0.25f;
        }

        EXPECT_EQ(input.Project(), input.Full());

        ExpectScalar(input);
    }
}

TEST(StudioEffectBoundsTest, AppliesWThresholdAndNearPlaneBoundaryExactly)
{
    const float infinity = std::numeric_limits<float>::infinity();

    for (float w : {0.0f, -0.0f, -1.0f, std::numeric_limits<float>::denorm_min(), std::nextafter(0.00001f, 0.0f), 0.00001f})
    {
        SCOPED_TRACE(w);
        BoundsInput input;
        input.ConstantClip(0, 0, 0, w);

        EXPECT_EQ(input.Project(), input.Full());
    }

    for (float w : {std::nextafter(0.00001f, infinity), 1.0f})
    {
        BoundsInput input;
        input.ConstantClip(0, 0, -w, w);

        EXPECT_EQ(input.Project(), input.Full());

        input.packet.projection[14] = std::nextafter(-w, -infinity);

        EXPECT_EQ(input.Project(), input.Full());

        input.packet.projection[14] = std::nextafter(-w, infinity);

        EXPECT_EQ(input.Project(), (Rect_t{50, 40, 0, 0}));
    }
}

TEST(StudioEffectBoundsTest, RoundsOutwardAtPixelBoundaries)
{
    BoundsInput input;
    input.packet.viewport = {17, 29, 128, 128};
    const float infinity = std::numeric_limits<float>::infinity();

    for (float coordinate :
         {-1.0f,
          std::nextafter(-1.0f, -infinity),
          -0.0f,
          0.0f,
          std::nextafter(0.0f, 1.0f),
          std::nextafter(0.25f, 0.0f),
          0.25f,
          std::nextafter(0.25f, 1.0f),
          1.0f,
          std::nextafter(1.0f, infinity)})
    {
        SCOPED_TRACE(coordinate);
        input.ConstantClip(coordinate, coordinate, 0, 1);
        ExpectScalar(input);
    }

    input.ConstantClip(0.25f, 0.25f, 0, 1);

    EXPECT_EQ(input.Project(), (Rect_t{80, 80, 0, 0}));

    input.radius = 0.125f;

    EXPECT_EQ(input.Project(), (Rect_t{79, 79, 2, 2}));

    input.radius = 0;
    input.ConstantClip(0.25390625f, -0.25390625f, 0, 1);

    EXPECT_EQ(input.Project(), (Rect_t{80, 47, 1, 1}));
}

TEST(StudioEffectBoundsTest, ChecksFirstAndLastBoundsForEveryBatchSize)
{
    for (int count : kBoundsCounts)
    {
        for (int fallback_index : {0, count - 1})
        {
            SCOPED_TRACE(::testing::Message() << "count=" << count << " fallback=" << fallback_index);
            BoundsInput input(count);
            input.palette[fallback_index * 12 + 11] = -1;
            input.Refresh();

            EXPECT_EQ(input.Project(), input.Full());

            ExpectScalar(input);
        }
    }
}

TEST(StudioEffectBoundsTest, MatchesScalarForNonfiniteSourceComponents)
{
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    for (int source = 0; source < 4; ++source)
    {
        const int components = source == 0 ? 12 : source == 3 ? 6 : 16;

        for (int component = 0; component < components; ++component)
        {
            for (float special : {nan, infinity, -infinity})
            {
                SCOPED_TRACE(::testing::Message() << "source=" << source << " component=" << component << " value=" << special);
                BoundsInput input(8);

                if (source == 0)
                {
                    input.palette[component] = special;
                }
                else if (source == 1)
                {
                    input.packet.modelview[component] = special;
                }
                else if (source == 2)
                {
                    input.packet.projection[component] = special;
                }
                else if (component < 3)
                {
                    input.model.bounds.back().minimum[component] = special;
                }
                else
                {
                    input.model.bounds.back().maximum[component - 3] = special;
                }

                input.Refresh();
                ExpectScalar(input);
            }
        }
    }
}

TEST(StudioEffectBoundsTest, MatchesScalarForDeterministicPosesAndProjectionMatrices)
{
    uint32_t random = 0x83147a91;

    for (int count : kBoundsCounts)
    {
        for (int iteration = 0; iteration < 160; ++iteration)
        {
            SCOPED_TRACE(::testing::Message() << "count=" << count << " iteration=" << iteration);
            BoundsInput input(count);
            input.radius = RandomRange(random, 0, 64);
            input.packet.viewport[2] = static_cast<int>(NextRandom(random) % 8193);
            input.packet.viewport[3] = static_cast<int>(NextRandom(random) % 8193);
            input.packet.projection = {
                0.97428f, 0, 0, 0,
                0, 1.73205f, 0, 0,
                0, 0, -1.002002f, -1,
                0, 0, -0.2002002f, 0
            };

            for (size_t bone = 0; bone < render::kStudioBoneFloats / 12; ++bone)
            {
                float* transform = input.palette.data() + bone * 12;

                for (int row = 0; row < 3; ++row)
                {
                    for (int column = 0; column < 3; ++column)
                    {
                        transform[row * 4 + column] = RandomRange(random, -0.75f, 0.75f);
                    }
                }

                transform[3] = RandomRange(random, -0.5f, 0.5f);
                transform[7] = RandomRange(random, -0.5f, 0.5f);
                transform[11] = RandomRange(random, -8, -2);
            }

            for (render::StudioBoneBounds& box : input.model.bounds)
            {
                box.bone = static_cast<int>(NextRandom(random) % goldsrc::MAXSTUDIOBONES);

                for (int axis = 0; axis < 3; ++axis)
                {
                    box.minimum[axis] = RandomRange(random, -0.5f, 0.25f);
                    box.maximum[axis] = box.minimum[axis] + RandomRange(random, 0, 0.5f);
                }

                if (iteration % 7 == 0)
                {
                    box.maximum = box.minimum;
                }
            }

            if (iteration % 3 == 0)
            {
                for (float& value : input.packet.modelview)
                {
                    value = RandomRange(random, -1, 1);
                }

                for (float& value : input.packet.projection)
                {
                    value = RandomRange(random, -1, 1);
                }
            }

            input.Refresh();
            ExpectScalar(input);
        }
    }
}
