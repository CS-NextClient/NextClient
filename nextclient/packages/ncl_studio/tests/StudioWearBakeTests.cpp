#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

#include <goldsrc/format/studio.h>
#include <gtest/gtest.h>
#include <ncl_studio/StudioWearBake.h>

namespace
{
    constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0f;
    // The repeated sample has weights (2/15, 31/40, 11/120) over curvatures (0, -1/sqrt(2), -1).
    constexpr uint8_t kRepeatedSampleThreshold = 112;
    constexpr std::array<short, 6> kHorizontalRepeatedTexels = {90, 20, 110, 20, 90, 80};

    struct TriangleModel
    {
        goldsrc::studiohdr_t header{};
        goldsrc::mstudiobone_t bones[3]{};
        goldsrc::mstudiobodyparts_t body{};
        goldsrc::mstudiomodel_t submodel{};
        goldsrc::mstudiomesh_t meshes[2]{};
        float positions[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        float normals[3][3] = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}};
        uint8_t position_bones[3]{};
        uint8_t normal_bones[3]{};
        short commands[14] = {3, 0, 0, 10, 10, 1, 1, 85, 10, 2, 2, 10, 85, 0};

        TriangleModel()
        {
            header.version = 10;
            header.length = sizeof(*this);
            header.numbones = 2;
            header.boneindex = offsetof(TriangleModel, bones);
            header.numbodyparts = 1;
            header.bodypartindex = offsetof(TriangleModel, body);
            bones[0].parent = -1;
            bones[1].parent = -1;
            body.nummodels = 1;
            body.base = 1;
            body.modelindex = offsetof(TriangleModel, submodel);
            submodel.nummesh = 1;
            submodel.meshindex = offsetof(TriangleModel, meshes);
            submodel.numverts = 3;
            submodel.vertindex = offsetof(TriangleModel, positions);
            submodel.vertinfoindex = offsetof(TriangleModel, position_bones);
            submodel.numnorms = 3;
            submodel.normindex = offsetof(TriangleModel, normals);
            submodel.norminfoindex = offsetof(TriangleModel, normal_bones);
            meshes[0].numtris = 1;
            meshes[0].numnorms = 3;
            meshes[0].triindex = offsetof(TriangleModel, commands);
            meshes[1] = meshes[0];
            meshes[1].skinref = 1;
        }
    };

    ncl_studio::StudioTextures Textures()
    {
        ncl_studio::StudioTextures textures;
        textures.textures = {{"first.bmp", 0, 100, 100}, {"second.bmp", 0, 200, 100}};
        textures.skins.families = 2;
        textures.skins.references = 1;
        textures.skins.values = {0, 1};
        return textures;
    }

    void SetTriangleTexels(TriangleModel& model, const std::array<short, 6>& texels)
    {
        for (size_t corner = 0; corner < 3; corner++)
        {
            model.commands[3 + corner * 4] = texels[corner * 2];
            model.commands[4 + corner * 4] = texels[corner * 2 + 1];
        }
    }
} // namespace

TEST(StudioWearBake, SelectsUsedTextureAndDimensionsFromSkinFamily)
{
    TriangleModel model;
    ncl_studio::WearBakeOptions options;
    options.skin_family = 1;
    options.size = 128;
    auto result = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options);
    ASSERT_FALSE(result.has_error()) << result.error_str();
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0].texture_index, 1);
    EXPECT_EQ((*result)[0].width, 128);
    EXPECT_EQ((*result)[0].height, 64);
    EXPECT_EQ((*result)[0].pixels.size(), 128u * 64u);
}

TEST(StudioWearBake, AppliesReferencePoseToIndependentPositionBones)
{
    TriangleModel identity;
    TriangleModel translated;
    translated.positions[1][0] = -9;
    translated.position_bones[1] = 1;
    translated.bones[1].value[0] = 10;
    auto expected = ncl_studio::BakeStudioWearMaps(&identity, sizeof(identity), Textures(), {});
    auto result = ncl_studio::BakeStudioWearMaps(&translated, sizeof(translated), Textures(), {});
    ASSERT_FALSE(expected.has_error()) << expected.error_str();
    ASSERT_FALSE(result.has_error()) << result.error_str();
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0].pixels, (*expected)[0].pixels);
}

TEST(StudioWearBake, ZeroScratchStrengthPreservesEdgeMapAndIgnoresScratchInput)
{
    TriangleModel model;
    auto expected = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), {});
    ncl_studio::WearBakeOptions options;
    options.scratch_width = -1;
    auto result = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options);
    ASSERT_FALSE(expected.has_error()) << expected.error_str();
    ASSERT_FALSE(result.has_error()) << result.error_str();
    EXPECT_EQ((*result)[0].pixels, (*expected)[0].pixels);
}

TEST(StudioWearBake, ScratchStrengthDarkensCoveredSurfaceUsingRedChannel)
{
    TriangleModel model;
    const std::array<uint8_t, 4> scratch = {0, 255, 255, 255};
    ncl_studio::WearBakeOptions options;
    options.scratch_width = 1;
    options.scratch_height = 1;
    options.scratch_rgba = scratch;
    auto base = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options);
    options.scratch_strength = 0.5f;
    auto half = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options);
    options.scratch_strength = 1;
    auto full = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options);
    ASSERT_FALSE(base.has_error()) << base.error_str();
    ASSERT_FALSE(half.has_error()) << half.error_str();
    ASSERT_FALSE(full.has_error()) << full.error_str();
    const size_t surface = 20u * 100u + 20u;
    EXPECT_GT((*base)[0].pixels[surface], (*half)[0].pixels[surface]);
    EXPECT_GT((*half)[0].pixels[surface], (*full)[0].pixels[surface]);
    EXPECT_EQ((*full)[0].pixels[surface], 0);
    EXPECT_EQ((*full)[0].pixels[0], 128);
}

TEST(StudioWearBake, RejectsInvalidFamilyStrengthAndScratchExtent)
{
    TriangleModel model;
    ncl_studio::WearBakeOptions options;
    options.skin_family = 2;
    EXPECT_TRUE(ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options).has_error());
    options.skin_family = 0;
    for (float strength : {-1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN()})
    {
        options.scratch_strength = strength;
        EXPECT_TRUE(ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options).has_error());
    }
    options.scratch_strength = 1;
    options.scratch_width = std::numeric_limits<int>::max();
    options.scratch_height = std::numeric_limits<int>::max();
    EXPECT_TRUE(ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options).has_error());
}

TEST(StudioWearBake, RejectsTruncatedGeometryWithoutProducingMaps)
{
    TriangleModel model;
    EXPECT_TRUE(ncl_studio::BakeStudioWearMaps(&model, sizeof(model) - sizeof(model.commands), Textures(), {}).has_error());
}

TEST(StudioWearBake, RejectsGeometryWhoseCurvatureArithmeticOverflows)
{
    TriangleModel model;
    model.positions[0][0] = -std::numeric_limits<float>::max();
    model.positions[1][0] = std::numeric_limits<float>::max();
    EXPECT_TRUE(ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), {}).has_error());
}

TEST(StudioWearBake, SelectionSkipsOtherUsedTextureMaps)
{
    TriangleModel model;
    model.submodel.nummesh = 2;
    ncl_studio::StudioTextures textures = Textures();
    textures.skins.families = 1;
    textures.skins.references = 2;
    const std::array<int, 1> selected = {1};
    ncl_studio::WearBakeOptions options;
    options.texture_indices = selected;
    auto result = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), textures, options);
    ASSERT_FALSE(result.has_error()) << result.error_str();
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0].texture_index, 1);
}

TEST(StudioWearBake, RejectsSelectedTextureUnusedBySkinAndOutOfRangeIndices)
{
    TriangleModel model;
    for (int texture : {-1, 1, 2})
    {
        const std::array<int, 1> selected = {texture};
        ncl_studio::WearBakeOptions options;
        options.texture_indices = selected;
        EXPECT_TRUE(ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), options).has_error()) << texture;
    }
}

TEST(StudioWearBake, CanceledOperationReturnsBeforeReadingTheModel)
{
    std::stop_source cancellation;
    cancellation.request_stop();
    ncl_studio::WearBakeOptions options;
    options.stop_token = cancellation.get_token();
    auto result = ncl_studio::BakeStudioWearMaps(nullptr, 0, Textures(), options);
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error_str(), "wear generation canceled");
}

TEST(StudioWearBake, WrapsThresholdInterpolationAcrossTextureRepeats)
{
    struct RepeatCase
    {
        std::array<short, 6> texels;
        int sample_x;
        int sample_y;
    };

    const std::array<RepeatCase, 4> cases = {{
        {kHorizontalRepeatedTexels, 5, 25},
        {{20, 90, 20, 110, 80, 90}, 25, 5},
        {{-10, 20, 10, 20, -10, 80}, 5, 25},
        {{390, -180, 410, -180, 390, -120}, 5, 25},
    }};
    std::array<std::vector<uint8_t>, cases.size()> images;

    for (size_t index = 0; index < cases.size(); index++)
    {
        SCOPED_TRACE(index);
        TriangleModel model;
        SetTriangleTexels(model, cases[index].texels);
        auto result = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), {});
        ASSERT_FALSE(result.has_error()) << result.error_str();
        ASSERT_EQ(result->size(), 1u);

        const ncl_studio::StudioWearMap& image = (*result)[0];
        ASSERT_EQ(image.width, 100);
        ASSERT_EQ(image.height, 100);
        const size_t sample = static_cast<size_t>(cases[index].sample_y) * image.width + cases[index].sample_x;
        EXPECT_EQ(image.pixels[sample], kRepeatedSampleThreshold);
        images[index] = image.pixels;
    }

    EXPECT_EQ(images[0], images[2]);
    EXPECT_EQ(images[0], images[3]);
}

TEST(StudioWearBake, AppliesParentTransformsAndIndependentNormalBones)
{
    std::array<TriangleModel, 2> cases;
    TriangleModel& hierarchy = cases[0];
    hierarchy.header.numbones = 3;
    hierarchy.bones[1].value[0] = 10;
    hierarchy.bones[1].value[5] = kHalfPi;
    hierarchy.bones[2].parent = 1;
    hierarchy.bones[2].value[1] = 9;
    hierarchy.positions[1][0] = 0;
    hierarchy.position_bones[1] = 2;

    TriangleModel& independent = cases[1];
    independent.header.numbones = 3;
    independent.bones[1].value[0] = 10;
    independent.bones[2].parent = -1;
    independent.bones[2].value[0] = 80;
    independent.bones[2].value[1] = 90;
    independent.bones[2].value[2] = 100;
    independent.bones[2].value[5] = kHalfPi;
    independent.positions[1][0] = -9;
    independent.normals[1][0] = 0;
    independent.normals[1][1] = -1;
    independent.position_bones[1] = 1;
    independent.normal_bones[1] = 2;

    for (size_t index = 0; index < cases.size(); index++)
    {
        SCOPED_TRACE(index);
        TriangleModel& model = cases[index];
        SetTriangleTexels(model, kHorizontalRepeatedTexels);
        auto result = ncl_studio::BakeStudioWearMaps(&model, sizeof(model), Textures(), {});
        ASSERT_FALSE(result.has_error()) << result.error_str();
        ASSERT_EQ(result->size(), 1u);

        const ncl_studio::StudioWearMap& image = (*result)[0];
        ASSERT_EQ(image.width, 100);
        ASSERT_EQ(image.height, 100);
        EXPECT_EQ(image.pixels[25 * 100 + 5], kRepeatedSampleThreshold);
    }
}
