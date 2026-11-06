#include <array>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "graphics/render/StudioPose.h"

namespace
{
    size_t g_Allocations{};
    size_t g_Releases{};
    bool g_CountAllocations{};
} // namespace

void* operator new(size_t size)
{
    void* memory = std::malloc(size);
    if (memory == nullptr)
    {
        throw std::bad_alloc();
    }

    if (g_CountAllocations)
    {
        ++g_Allocations;
    }

    return memory;
}

void operator delete(void* memory) noexcept
{
    if (memory != nullptr && g_CountAllocations)
    {
        ++g_Releases;
    }

    std::free(memory);
}

namespace
{
    std::array<float, render::kStudioBoneFloats> MakePose(float offset)
    {
        std::array<float, render::kStudioBoneFloats> values;

        for (size_t i = 0; i < values.size(); ++i)
        {
            values[i] = offset + static_cast<float>(i);
        }

        return values;
    }
} // namespace

static_assert(std::is_same_v<decltype(std::declval<render::StudioPose&>().data()), const float*>);
static_assert(std::is_same_v<decltype(std::declval<render::StudioPose&>()[0]), const float&>);

TEST(StudioPoseTest, CapturesCompleteImmutableIndependentPalette)
{
    render::StudioPosePool pool;
    std::array<float, render::kStudioBoneFloats> values = MakePose(10);
    render::StudioPose first = pool.Capture(values.data());
    values.fill(-17);
    render::StudioPose second = pool.Capture(values.data());

    ASSERT_NE(first.data(), second.data());

    for (size_t i = 0; i < first.size(); ++i)
    {
        EXPECT_EQ(first[i], 10 + static_cast<float>(i));
        EXPECT_EQ(second[i], -17);
    }

    EXPECT_EQ(first.back(), 10 + static_cast<float>(render::kStudioBoneFloats - 1));
}

TEST(StudioPoseTest, CopyMoveAliasAndSelfAssignmentKeepOneSnapshot)
{
    render::StudioPosePool pool;
    const std::array<float, render::kStudioBoneFloats> values = MakePose(2);
    render::StudioPose source = pool.Capture(values.data());
    const float* address = source.data();
    render::StudioPose copy(source);
    render::StudioPose assigned;

    assigned = copy;
    assigned = source;
    source = source;
    source = std::move(source);

    render::StudioPose moved(std::move(copy));
    EXPECT_EQ(copy.data(), nullptr);

    assigned = std::move(moved);
    EXPECT_EQ(moved.data(), nullptr);
    source = {};

    ASSERT_EQ(assigned.data(), address);
    EXPECT_EQ(assigned.back(), values.back());

    assigned = {};
    render::StudioPose reused = pool.Capture(values.data());

    EXPECT_EQ(reused.data(), address);
}

TEST(StudioPoseTest, RetainedSnapshotsSurviveGrowthResetAndOwnerDestruction)
{
    render::StudioPose retained;
    const std::array<float, render::kStudioBoneFloats> values = MakePose(3);
    const float* address = nullptr;

    {
        render::StudioPosePool pool;
        retained = pool.Capture(values.data());
        address = retained.data();
        std::vector<render::StudioPose> occupied;

        for (int i = 0; i < 400; ++i)
        {
            const std::array<float, render::kStudioBoneFloats> next = MakePose(static_cast<float>(i + 20));
            occupied.push_back(pool.Capture(next.data()));
            EXPECT_NE(occupied.back().data(), address);
        }

        pool.Reset();

        const std::array<float, render::kStudioBoneFloats> next = MakePose(1000);
        render::StudioPose replacement = pool.Capture(next.data());
        EXPECT_NE(replacement.data(), address);

        for (size_t i = 0; i < values.size(); ++i)
        {
            EXPECT_EQ(retained[i], values[i]);
        }

        EXPECT_EQ(occupied.back().back(), 419 + static_cast<float>(values.size() - 1));
    }

    EXPECT_EQ(retained.data(), address);
    EXPECT_EQ(retained.back(), values.back());
    retained = {};
}

TEST(StudioPoseTest, ResetFreesUnusedSlotsAndLastOrphanReleaseFreesItsSlot)
{
    render::StudioPose retained;
    render::StudioPose second;
    render::StudioPose alias;
    const std::array<float, render::kStudioBoneFloats> values = MakePose(7);
    size_t reset_releases = 0;
    size_t orphan_releases = 0;
    size_t alias_releases = 0;
    size_t second_releases = 0;

    {
        render::StudioPosePool pool;
        retained = pool.Capture(values.data());
        second = pool.Capture(values.data());
        alias = second;
        render::StudioPose free_slot = pool.Capture(values.data());
        free_slot = {};

        g_Releases = 0;
        g_CountAllocations = true;
        pool.Reset();
        reset_releases = g_Releases;
        retained = {};
        orphan_releases = g_Releases - reset_releases;
        second = {};
        alias_releases = g_Releases - reset_releases - orphan_releases;
        alias = {};
        second_releases = g_Releases - reset_releases - orphan_releases - alias_releases;
        g_CountAllocations = false;
    }

    EXPECT_GE(reset_releases, 1u);
    EXPECT_EQ(orphan_releases, 1u);
    EXPECT_EQ(alias_releases, 0u);
    EXPECT_EQ(second_releases, 1u);
}

TEST(StudioPoseTest, LastHandleReleasesOrphanAfterPoolDestruction)
{
    render::StudioPose retained;
    const std::array<float, render::kStudioBoneFloats> values = MakePose(12);

    {
        render::StudioPosePool pool;
        retained = pool.Capture(values.data());
    }

    render::StudioPose alias = retained;
    g_Releases = 0;
    g_CountAllocations = true;
    retained = {};
    const size_t first_releases = g_Releases;
    alias = {};
    const size_t final_releases = g_Releases;
    g_CountAllocations = false;

    EXPECT_EQ(first_releases, 0u);
    EXPECT_EQ(final_releases, 1u);
}

TEST(StudioPoseTest, WarmedCaptureAndHandleOperationsAllocateNothing)
{
    render::StudioPosePool pool;
    const std::array<float, render::kStudioBoneFloats> values = MakePose(1);
    std::array<render::StudioPose, 8> handles;

    for (render::StudioPose& handle : handles)
    {
        handle = pool.Capture(values.data());
    }

    for (render::StudioPose& handle : handles)
    {
        handle = {};
    }

    g_Allocations = 0;
    g_CountAllocations = true;

    for (int iteration = 0; iteration < 100; ++iteration)
    {
        for (render::StudioPose& handle : handles)
        {
            handle = pool.Capture(values.data());
            render::StudioPose copy = handle;
            render::StudioPose moved = std::move(copy);
        }

        for (render::StudioPose& handle : handles)
        {
            handle = {};
        }
    }

    g_CountAllocations = false;

    EXPECT_EQ(g_Allocations, 0u);
}
