#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include <goldsrc/format/studio.h>

#include "GameUi/MaterialEditor/MaterialModelThumbnail.h"

using material_editor::MaterialModelThumbnail;

namespace
{
    template <class T>
    T& At(std::vector<uint8_t>& data, size_t offset)
    {
        return *reinterpret_cast<T*>(data.data() + offset);
    }

    std::vector<uint8_t> Model(bool masked = false)
    {
        std::vector<uint8_t> data(4096);
        goldsrc::studiohdr_t& header = At<goldsrc::studiohdr_t>(data, 0);
        header.id = 0x54534449;
        header.version = 10;
        header.length = static_cast<int>(data.size());
        header.numbones = 2;
        header.boneindex = 256;
        header.numbodyparts = 1;
        header.bodypartindex = 512;
        header.numtextures = 1;
        header.textureindex = 1024;
        header.texturedataindex = 2048;
        header.numskinref = 1;
        header.numskinfamilies = 1;
        header.skinindex = 1200;
        goldsrc::mstudiobone_t& root = At<goldsrc::mstudiobone_t>(data, 256);
        root.parent = -1;
        goldsrc::mstudiobone_t& child = At<goldsrc::mstudiobone_t>(data, 256 + sizeof(root));
        child.parent = 0;
        child.value[0] = 100;
        goldsrc::mstudiobodyparts_t& body = At<goldsrc::mstudiobodyparts_t>(data, 512);
        body.nummodels = 1;
        body.modelindex = 600;
        goldsrc::mstudiomodel_t& model = At<goldsrc::mstudiomodel_t>(data, 600);
        model.nummesh = 1;
        model.meshindex = 720;
        model.numverts = 3;
        model.vertindex = 800;
        model.vertinfoindex = 840;
        model.numnorms = 1;
        model.normindex = 852;
        model.norminfoindex = 868;
        goldsrc::mstudiomesh_t& mesh = At<goldsrc::mstudiomesh_t>(data, 720);
        mesh.numtris = 1;
        mesh.triindex = 900;
        const float vertices[9] = {0, -8, -8, 0, 8, -8, 0, 0, 8};
        std::memcpy(data.data() + 800, vertices, sizeof(vertices));
        data[840] = data[841] = data[842] = 1;
        At<float>(data, 852) = 1;
        const short commands[] = {3, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0};
        std::memcpy(data.data() + 900, commands, sizeof(commands));
        goldsrc::mstudiotexture_t& texture = At<goldsrc::mstudiotexture_t>(data, 1024);
        std::strcpy(texture.name, "base.bmp");
        texture.width = texture.height = 1;
        texture.index = 2048;
        texture.flags = masked ? goldsrc::STUDIO_NF_MASKED : 0;
        data[2048] = masked ? 255 : 1;
        data[2049 + 3] = 255;
        return data;
    }

    int Visible(const MaterialModelThumbnail& image)
    {
        int count = 0;

        for (size_t index = 3; index < image.rgba.size(); index += 4)
        {
            count += image.rgba[index] != 0;
        }

        return count;
    }
} // namespace

TEST(MaterialModelThumbnail, RendersTexturePaletteColors)
{
    for (int channel = 0; channel < 3; channel++)
    {
        SCOPED_TRACE(channel);
        std::vector<uint8_t> model = Model();
        model[2052] = model[2053] = model[2054] = 0;
        model[2052 + channel] = 255;
        ncl_utils::ResultT<MaterialModelThumbnail> image = material_editor::RenderModelThumbnail(model, {});
        ASSERT_FALSE(image.has_error()) << image.error_str();
        ASSERT_EQ(image->rgba.size(), 128u * 128u * 4u);
        EXPECT_GT(Visible(*image), 1000);

        int color_totals[3]{};

        for (size_t pixel = 0; pixel < image->rgba.size(); pixel += 4)
        {
            if (image->rgba[pixel + 3] != 0)
            {
                for (int component = 0; component < 3; component++)
                {
                    color_totals[component] += image->rgba[pixel + component];
                }
            }
        }

        for (int component = 0; component < 3; component++)
        {
            if (component == channel)
            {
                EXPECT_GT(color_totals[component], 0);
            }
            else
            {
                EXPECT_EQ(color_totals[component], 0);
            }
        }
    }
}

TEST(MaterialModelThumbnail, LaterParentMatchesOrdinaryBoneOrder)
{
    std::vector<uint8_t> ordinary = Model();
    goldsrc::mstudiobone_t& root = At<goldsrc::mstudiobone_t>(ordinary, 256);
    root.value[3] = 0.25f;
    root.value[4] = -0.15f;
    root.value[5] = 0.4f;
    ordinary[840] = 0;
    ordinary[868] = 1;
    std::vector<uint8_t> reordered = ordinary;
    goldsrc::mstudiobone_t first = At<goldsrc::mstudiobone_t>(ordinary, 256);
    goldsrc::mstudiobone_t second = At<goldsrc::mstudiobone_t>(ordinary, 256 + sizeof(first));
    second.parent = 1;
    At<goldsrc::mstudiobone_t>(reordered, 256) = second;
    At<goldsrc::mstudiobone_t>(reordered, 256 + sizeof(first)) = first;

    for (size_t index = 840; index < 843; index++)
    {
        reordered[index] = 1 - ordinary[index];
    }

    reordered[868] = 1 - ordinary[868];
    ncl_utils::ResultT<MaterialModelThumbnail> expected = material_editor::RenderModelThumbnail(ordinary, {});
    ncl_utils::ResultT<MaterialModelThumbnail> actual = material_editor::RenderModelThumbnail(reordered, {});
    ASSERT_FALSE(expected.has_error()) << expected.error_str();
    ASSERT_FALSE(actual.has_error()) << actual.error_str();
    EXPECT_GT(Visible(*expected), 100);
    EXPECT_EQ(actual->rgba, expected->rgba);
}

TEST(MaterialModelThumbnail, RelativeBoneTranslationMatchesBakedGeometry)
{
    std::vector<uint8_t> posed = Model();
    posed[840] = 0;
    std::vector<uint8_t> baked = posed;
    At<goldsrc::mstudiobone_t>(baked, 256 + sizeof(goldsrc::mstudiobone_t)).value[0] = 0;
    const float vertices[9] = {0, -8, -8, 100, 8, -8, 100, 0, 8};
    std::memcpy(baked.data() + 800, vertices, sizeof(vertices));
    baked[840] = baked[841] = baked[842] = 0;

    ncl_utils::ResultT<MaterialModelThumbnail> actual = material_editor::RenderModelThumbnail(posed, {});
    ncl_utils::ResultT<MaterialModelThumbnail> expected = material_editor::RenderModelThumbnail(baked, {});
    ASSERT_FALSE(actual.has_error()) << actual.error_str();
    ASSERT_FALSE(expected.has_error()) << expected.error_str();
    EXPECT_GT(Visible(*expected), 100);
    EXPECT_EQ(actual->rgba, expected->rgba);
}

TEST(MaterialModelThumbnail, BoneNormalRotationMatchesBakedNormals)
{
    std::vector<uint8_t> posed = Model();
    posed[840] = posed[841] = posed[842] = 0;
    posed[868] = 1;
    goldsrc::mstudiobone_t& child = At<goldsrc::mstudiobone_t>(posed, 256 + sizeof(goldsrc::mstudiobone_t));
    child.value[0] = 0;
    child.value[5] = 1.5707963267948966f;
    std::vector<uint8_t> baked = posed;
    At<goldsrc::mstudiobone_t>(baked, 256 + sizeof(goldsrc::mstudiobone_t)).value[5] = 0;
    baked[868] = 0;
    const float normal[3] = {0, 1, 0};
    std::memcpy(baked.data() + 852, normal, sizeof(normal));

    ncl_utils::ResultT<MaterialModelThumbnail> actual = material_editor::RenderModelThumbnail(posed, {});
    ncl_utils::ResultT<MaterialModelThumbnail> expected = material_editor::RenderModelThumbnail(baked, {});
    ASSERT_FALSE(actual.has_error()) << actual.error_str();
    ASSERT_FALSE(expected.has_error()) << expected.error_str();
    EXPECT_GT(Visible(*expected), 1000);
    EXPECT_EQ(actual->rgba, expected->rgba);
}

TEST(MaterialModelThumbnail, ResolvesCompanionTexturesAndMaskedHoles)
{
    std::vector<uint8_t> model = Model();
    std::vector<uint8_t> companion = Model();
    auto& header = At<goldsrc::studiohdr_t>(model, 0);
    header.numtextures = header.textureindex = header.texturedataindex = 0;
    auto image = material_editor::RenderModelThumbnail(model, companion);
    ASSERT_FALSE(image.has_error()) << image.error_str();
    EXPECT_GT(Visible(*image), 1000);
    EXPECT_TRUE(material_editor::RenderModelThumbnail(model, {}).has_error());
    image = material_editor::RenderModelThumbnail(Model(true), {});
    ASSERT_FALSE(image.has_error()) << image.error_str();
    EXPECT_EQ(Visible(*image), 0);
}

TEST(MaterialModelThumbnail, RejectsTruncatedDataAndCyclicBones)
{
    std::vector<uint8_t> model = Model();
    model.resize(2100);
    EXPECT_TRUE(material_editor::RenderModelThumbnail(model, {}).has_error());
    model = Model();
    At<goldsrc::mstudiobone_t>(model, 256).parent = 1;
    EXPECT_TRUE(material_editor::RenderModelThumbnail(model, {}).has_error());
}

TEST(MaterialModelThumbnail, FramesOnlyTheDefaultBodySubmodels)
{
    std::vector<uint8_t> model = Model();
    goldsrc::mstudiomodel_t first = At<goldsrc::mstudiomodel_t>(model, 600);
    goldsrc::mstudiomodel_t second = first;
    second.vertindex = 1600;
    second.meshindex = 1540;
    At<goldsrc::mstudiomodel_t>(model, 1300) = first;
    At<goldsrc::mstudiomodel_t>(model, 1300 + sizeof(first)) = second;
    At<goldsrc::mstudiomesh_t>(model, 1540) = At<goldsrc::mstudiomesh_t>(model, 720);
    const float hidden[9] = {0, 4992, 4992, 0, 5008, 4992, 0, 5000, 5008};
    std::memcpy(model.data() + 1600, hidden, sizeof(hidden));
    goldsrc::mstudiobodyparts_t& body = At<goldsrc::mstudiobodyparts_t>(model, 512);
    body.modelindex = 1300;
    body.nummodels = 2;
    auto image = material_editor::RenderModelThumbnail(model, {});
    ASSERT_FALSE(image.has_error()) << image.error_str();
    EXPECT_GT(Visible(*image), 1000);
}

TEST(MaterialModelThumbnail, RejectsTexturePixelsWithoutAValidatedTextureTable)
{
    std::vector<uint8_t> invalid = Model();
    At<goldsrc::studiohdr_t>(invalid, 0).textureindex = 0;
    At<int>(invalid, 68) = 1;
    At<int>(invalid, 76) = 0x7ffffff0;
    EXPECT_TRUE(material_editor::RenderModelThumbnail(invalid, {}).has_error());
    std::vector<uint8_t> model = Model();
    auto& header = At<goldsrc::studiohdr_t>(model, 0);
    header.numtextures = header.textureindex = header.texturedataindex = 0;
    EXPECT_TRUE(material_editor::RenderModelThumbnail(model, invalid).has_error());
}

TEST(MaterialModelThumbnail, BoundsWorkForOverlappingGeometry)
{
    std::vector<uint8_t> model = Model();
    const goldsrc::mstudiomesh_t mesh = At<goldsrc::mstudiomesh_t>(model, 720);
    constexpr int kMeshes = 800;
    constexpr int kTextureOffset = 4096 + kMeshes * sizeof(goldsrc::mstudiomesh_t) + 256;
    model.resize(kTextureOffset + 769);
    std::memcpy(model.data() + kTextureOffset, model.data() + 2048, 769);
    auto& header = At<goldsrc::studiohdr_t>(model, 0);
    header.length = static_cast<int>(model.size());
    header.texturedataindex = kTextureOffset;
    At<goldsrc::mstudiotexture_t>(model, 1024).index = kTextureOffset;
    auto& submodel = At<goldsrc::mstudiomodel_t>(model, 600);
    submodel.nummesh = kMeshes;
    submodel.meshindex = 4096;

    for (int index = 0; index < kMeshes; index++)
    {
        At<goldsrc::mstudiomesh_t>(model, 4096 + index * sizeof(mesh)) = mesh;
    }

    auto image = material_editor::RenderModelThumbnail(model, {});
    ASSERT_TRUE(image.has_error());
    EXPECT_NE(image.error_str().find("raster budget"), std::string::npos);
}
