#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <goldsrc/format/studio.h>
#include <ncl_studio/StudioReferencePose.h>

namespace
{
    struct PoseBlock
    {
        goldsrc::studiohdr_t header{};
        goldsrc::mstudiobone_t bones[3]{};

        PoseBlock()
        {
            header.version = 10;
            header.numbones = 3;
            header.boneindex = offsetof(PoseBlock, bones);

            for (goldsrc::mstudiobone_t& bone : bones)
            {
                bone.parent = -1;
            }
        }
    };

    void ExpectVector(const ncl_math::Vector3& actual, const ncl_math::Vector3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, 1e-5f);
        EXPECT_NEAR(actual.y, expected.y, 1e-5f);
        EXPECT_NEAR(actual.z, expected.z, 1e-5f);
    }
} // namespace

TEST(StudioReferencePose, TranslatesPointsWithoutTranslatingNormals)
{
    PoseBlock block;
    block.bones[0].value[0] = 10.0f;
    block.bones[0].value[1] = -4.0f;
    block.bones[0].value[2] = 2.0f;
    ncl_utils::ResultT<std::vector<ncl_studio::StudioBoneTransform>> pose = ncl_studio::ReadStudioReferencePose(&block, sizeof(block));
    ASSERT_FALSE(pose.has_error()) << pose.error_str();
    ExpectVector((*pose)[0].Apply({1.0f, 2.0f, 3.0f}), {11.0f, -2.0f, 5.0f});
    ExpectVector((*pose)[0].ApplyVector({1.0f, 2.0f, 3.0f}), {1.0f, 2.0f, 3.0f});
}

TEST(StudioReferencePose, ComposesRadianRotationsInGoldSrcOrder)
{
    PoseBlock block;
    block.bones[0].value[3] = ncl_math::kPi / 2.0f;
    block.bones[0].value[5] = ncl_math::kPi / 2.0f;
    ncl_utils::ResultT<std::vector<ncl_studio::StudioBoneTransform>> pose = ncl_studio::ReadStudioReferencePose(&block, sizeof(block));
    ASSERT_FALSE(pose.has_error()) << pose.error_str();
    ExpectVector((*pose)[0].ApplyVector({1.0f, 0.0f, 0.0f}), {0.0f, 1.0f, 0.0f});
    ExpectVector((*pose)[0].ApplyVector({0.0f, 1.0f, 0.0f}), {0.0f, 0.0f, 1.0f});
    ExpectVector((*pose)[0].ApplyVector({0.0f, 0.0f, 1.0f}), {1.0f, 0.0f, 0.0f});
    block.bones[1].value[4] = ncl_math::kPi / 2.0f;
    pose = ncl_studio::ReadStudioReferencePose(&block, sizeof(block));
    ASSERT_FALSE(pose.has_error()) << pose.error_str();
    ExpectVector((*pose)[1].ApplyVector({1.0f, 0.0f, 0.0f}), {0.0f, 0.0f, -1.0f});
}

TEST(StudioReferencePose, ResolvesParentHierarchyEvenWhenParentFollowsChild)
{
    PoseBlock block;
    block.bones[0].parent = 2;
    block.bones[0].value[0] = 2.0f;
    block.bones[2].parent = 1;
    block.bones[2].value[1] = 3.0f;
    block.bones[1].value[0] = 10.0f;
    block.bones[1].value[5] = ncl_math::kPi / 2.0f;
    ncl_utils::ResultT<std::vector<ncl_studio::StudioBoneTransform>> pose = ncl_studio::ReadStudioReferencePose(&block, sizeof(block));
    ASSERT_FALSE(pose.has_error()) << pose.error_str();
    ExpectVector((*pose)[0].Apply({1.0f, 0.0f, 0.0f}), {7.0f, 3.0f, 0.0f});
    ExpectVector((*pose)[0].ApplyVector({1.0f, 0.0f, 0.0f}), {0.0f, 1.0f, 0.0f});
}

TEST(StudioReferencePose, RejectsTruncatedBoneTableAndInvalidParents)
{
    PoseBlock block;
    EXPECT_TRUE(ncl_studio::ReadStudioReferencePose(&block, sizeof(block) - 1).has_error());
    block.bones[0].parent = 3;
    EXPECT_TRUE(ncl_studio::ReadStudioReferencePose(&block, sizeof(block)).has_error());
    block.bones[0].parent = -2;
    EXPECT_TRUE(ncl_studio::ReadStudioReferencePose(&block, sizeof(block)).has_error());
    block.bones[0].parent = 1;
    block.bones[1].parent = 0;
    EXPECT_TRUE(ncl_studio::ReadStudioReferencePose(&block, sizeof(block)).has_error());
}

TEST(StudioReferencePose, RejectsNonfiniteValuesAndOwnsCompletedTransforms)
{
    PoseBlock block;
    block.bones[0].value[3] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(ncl_studio::ReadStudioReferencePose(&block, sizeof(block)).has_error());
    block.bones[0].value[3] = 0.0f;
    block.bones[0].value[0] = 4.0f;
    ncl_utils::ResultT<std::vector<ncl_studio::StudioBoneTransform>> pose = ncl_studio::ReadStudioReferencePose(&block, sizeof(block));
    ASSERT_FALSE(pose.has_error()) << pose.error_str();
    block.bones[0].value[0] = 9.0f;
    ExpectVector((*pose)[0].Apply({}), {4.0f, 0.0f, 0.0f});
}
