#include <gtest/gtest.h>

#include <cstring>
#include <functional>
#include <chrono>
#include <thread>

#include <ncl_image/Decode.h>
#include <ncl_image/Encode.h>
#include <ncl_material/material_parse.h>

#include "MaterialOwnerFiles.h"
#include "PerformanceTestCore.h"
#include "StudioMaterialProviders.h"
#include "StudioModelTestSupport.h"
#include "graphics/render/material/MaterialEditor.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/model/StudioModelCache.h"

namespace
{
    bool WaitForWear(render::MaterialEditor& editor, char* error, int error_size)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);

        while (std::chrono::steady_clock::now() < deadline)
        {
            const render::MaterialWearStatus status = editor.PollWearGeneration(error, error_size);

            if (status != render::MaterialWearStatus::Running)
            {
                return status == render::MaterialWearStatus::Ready;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        ADD_FAILURE() << "wear generation did not finish";
        return false;
    }

    bool RunWear(render::MaterialEditor& editor, int skin, float strength, char* error, int error_size)
    {
        return editor.BeginWearGeneration(skin, strength, error, error_size) && WaitForWear(editor, error, error_size);
    }

    struct WearModelBlock
    {
        goldsrc::studiohdr_t header{};
        goldsrc::mstudiobone_t bone{};
        goldsrc::mstudiobodyparts_t body{};
        goldsrc::mstudiomodel_t submodel{};
        goldsrc::mstudiomesh_t meshes[2]{};
        goldsrc::mstudiotexture_t textures[2]{};
        short skins[2] = {0, 1};
        float positions[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        float normal[3] = {0, 0, 1};
        unsigned char vertex_bones[3]{};
        unsigned char normal_bones[1]{};
        short commands[14] = {3, 0, 0, 0, 0, 1, 0, 64, 0, 2, 0, 0, 64, 0};

        WearModelBlock()
        {
            header.version = 10;
            header.length = sizeof(*this);
            header.numbones = 1;
            header.boneindex = offsetof(WearModelBlock, bone);
            bone.parent = -1;
            header.numbodyparts = 1;
            header.bodypartindex = offsetof(WearModelBlock, body);
            body.nummodels = 1;
            body.base = 1;
            body.modelindex = offsetof(WearModelBlock, submodel);
            submodel.nummesh = 2;
            submodel.meshindex = offsetof(WearModelBlock, meshes);
            submodel.numverts = 3;
            submodel.vertindex = offsetof(WearModelBlock, positions);
            submodel.vertinfoindex = offsetof(WearModelBlock, vertex_bones);
            submodel.numnorms = 1;
            submodel.normindex = offsetof(WearModelBlock, normal);
            submodel.norminfoindex = offsetof(WearModelBlock, normal_bones);
            header.numtextures = 2;
            header.textureindex = offsetof(WearModelBlock, textures);
            header.numskinref = 2;
            header.numskinfamilies = 1;
            header.skinindex = offsetof(WearModelBlock, skins);

            for (int index = 0; index < 2; index++)
            {
                meshes[index].skinref = index;
                meshes[index].numtris = 1;
                meshes[index].triindex = offsetof(WearModelBlock, commands);
                std::strcpy(textures[index].name, index == 0 ? "base.bmp" : "alternate.bmp");
                textures[index].width = 64;
                textures[index].height = 64;
            }
        }
    };

    class WearFiles : public MaterialOwnerFiles
    {
    public:
        std::function<void()> before_read;
        std::function<void()> after_generated_write;
        bool fail_document_write{};
        int fail_generated_after = -1;
        int generated_attempts{};

        render::MaterialReadStatus ReadFile(const char* path, std::string& text, size_t max_bytes) override
        {
            if (before_read)
            {
                std::function<void()> callback = std::move(before_read);
                callback();
            }

            return MaterialOwnerFiles::ReadFile(path, text, max_bytes);
        }

        bool Write(const std::string& path, const std::string& text, std::string& error) override
        {
            if (fail_document_write)
            {
                error = "injected document write failure";
                return false;
            }

            return MaterialOwnerFiles::Write(path, text, error);
        }

        bool WriteGenerated(const std::string& path, const std::string& text, std::string& error) override
        {
            if (generated_attempts++ == fail_generated_after)
            {
                error = "injected image write failure";
                return false;
            }

            const bool result = MaterialOwnerFiles::WriteGenerated(path, text, error);

            if (after_generated_write)
            {
                std::function<void()> callback = std::move(after_generated_write);
                callback();
            }

            return result;
        }
    };

    struct WearFixture
    {
        StudioMaterialSourceFixture source;
        WearFiles files;
        render::MaterialDocumentStore documents{files};
        render::ServerMaterials server{documents, source.catalog};
        performance_test::Core performance;
        render::StudioModelCache models{source.device, documents, source.catalog, source.shaders, server, performance.monitor};
        render::MaterialEditor editor{models, server, source.shaders, source.catalog, documents, source.cache};
        studio_test::ModelBlock metadata;
        goldsrc::model_t model{};

        WearFixture()
        {
            ncl_material::ProgramManifest manifest;
            manifest.program = "weapon_finish";
            ncl_material::ParamSchema wear;
            wear.name = "uWear";
            manifest.parameters.push_back(wear);
            ncl_material::ParamSchema map;
            map.name = "uWearMap";
            map.type = ncl_material::ParamType::TextureRef;
            manifest.parameters.push_back(map);
            map.name = "uUnderlay";
            manifest.parameters.push_back(map);
            ncl_material::ParamSchema transform;
            transform.name = "uWearMapTransform";
            transform.type = ncl_material::ParamType::Vec4;
            manifest.parameters.push_back(transform);
            source.manifests[manifest.program] = manifest;
            std::strcpy(model.name, "models/wear.mdl");
            EXPECT_TRUE(models.PrepareMetadata(&model, &metadata.header, &metadata.header, &model));
            const char* selected[] = {"base.bmp", "alternate.bmp"};
            EXPECT_TRUE(editor.BeginGroup(model.name, selected, 2, 0, [] { return false; }));
            EXPECT_TRUE(editor.SetEditedProgram("weapon_finish"));
            EXPECT_TRUE(editor.SetScalar("uWear", 0.25f));
            WearModelBlock binary;
            files.files[model.name] = std::string(reinterpret_cast<const char*>(&binary), sizeof(binary));
        }

        std::string Map(const char* texture)
        {
            const ncl_material::Material* material = editor.get_session().FindOverride(model.name, texture);
            const ncl_material::ParamValue* map = material != nullptr ? material->FindParam("uWearMap") : nullptr;
            return map != nullptr ? map->texture_ref : std::string();
        }

        bool Generate()
        {
            char error[512]{};
            const bool result = RunWear(editor, 0, 0, error, sizeof(error));
            if (!result)
            {
                ADD_FAILURE() << error;
            }

            return result;
        }
    };
} // namespace

TEST(MaterialWearGeneration, AssignsDifferentMapsWithIdentityPlacementAndPreservesWear)
{
    WearFixture owned;
    const float previous_transform[4] = {4, 3, 0.5f, 0.25f};
    ASSERT_TRUE(owned.editor.SetVec4("uWearMapTransform", previous_transform));
    ASSERT_TRUE(owned.Generate());
    const std::string first = owned.Map("base.bmp");
    const std::string second = owned.Map("alternate.bmp");
    EXPECT_NE(first, second);
    EXPECT_TRUE(first.starts_with("materials/generated/"));
    EXPECT_LE(first.size(), 128u);
    ASSERT_TRUE(owned.files.files.contains(first));
    const std::string& png = owned.files.files.at(first);
    const ncl_utils::ResultT<ncl_image::RgbaImage, ncl_image::DecodeError> image =
        ncl_image::DecodeRgba8({reinterpret_cast<const uint8_t*>(png.data()), png.size()}, 1048576);
    ASSERT_FALSE(image.has_error()) << image.error_str();
    EXPECT_EQ(image->width, 1024);
    EXPECT_EQ(image->height, 1024);
    float wear = 0;
    ASSERT_TRUE(owned.editor.get_scalar("uWear", &wear));
    EXPECT_FLOAT_EQ(wear, 0.25f);
    float transform[4]{};
    ASSERT_TRUE(owned.editor.get_vec4("uWearMapTransform", transform));
    EXPECT_FLOAT_EQ(transform[0], 1);
    EXPECT_FLOAT_EQ(transform[1], 1);
    EXPECT_FLOAT_EQ(transform[2], 0);
    EXPECT_FLOAT_EQ(transform[3], 0);
}

TEST(MaterialWearGeneration, GeneratesOnlySelectedTexture)
{
    WearFixture owned;
    const char* selected[] = {"alternate.bmp"};
    ASSERT_TRUE(owned.editor.BeginGroup(owned.model.name, selected, 1, 0, [] { return false; }));
    ASSERT_TRUE(owned.editor.SetEditedProgram("weapon_finish"));
    ASSERT_TRUE(owned.Generate());
    EXPECT_EQ(owned.files.generated_attempts, 1);
    EXPECT_FALSE(owned.Map("alternate.bmp").empty());
}

TEST(MaterialWearGeneration, ReadsTexturesFromModelCompanion)
{
    WearFixture owned;
    WearModelBlock binary;
    owned.files.files["models/wearT.mdl"] = std::string(reinterpret_cast<const char*>(&binary), sizeof(binary));
    binary.header.numtextures = 0;
    binary.header.numskinref = 0;
    binary.header.numskinfamilies = 0;
    owned.files.files[owned.model.name] = std::string(reinterpret_cast<const char*>(&binary), sizeof(binary));
    ASSERT_TRUE(owned.Generate());
    EXPECT_FALSE(owned.Map("base.bmp").empty());
    EXPECT_FALSE(owned.Map("alternate.bmp").empty());
}

TEST(MaterialWearGeneration, EndRemovesUnsavedImages)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string path = owned.Map("base.bmp");
    owned.editor.End();
    EXPECT_FALSE(owned.files.files.contains(path));
}

TEST(MaterialWearGeneration, SuccessfulSiblingSaveRetainsImagesAfterEnd)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string first = owned.Map("base.bmp");
    const std::string second = owned.Map("alternate.bmp");
    char error[512]{};
    ASSERT_TRUE(owned.editor.Save("models/related.mdl", error, sizeof(error))) << error;
    owned.editor.End();
    EXPECT_TRUE(owned.files.files.contains(first));
    EXPECT_TRUE(owned.files.files.contains(second));
    const ncl_utils::ResultT<ncl_material::MaterialDoc> doc = ncl_material::ParseMaterialDoc(owned.files.files.at("models/related.mat"));
    ASSERT_FALSE(doc.has_error());
    ASSERT_EQ(doc->entries.size(), 2u);
    ASSERT_NE(doc->FindByTexture("base.bmp"), nullptr);
    EXPECT_EQ(doc->FindByTexture("base.bmp")->FindParam("uWearMap")->texture_ref, first);
    ASSERT_NE(doc->FindByTexture("alternate.bmp"), nullptr);
    EXPECT_EQ(doc->FindByTexture("alternate.bmp")->FindParam("uWearMap")->texture_ref, second);
}

TEST(MaterialWearGeneration, RegenerationDoesNotOverwriteSavedMap)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string saved = owned.Map("base.bmp");
    char error[512]{};
    ASSERT_TRUE(owned.editor.Save(nullptr, error, sizeof(error))) << error;
    ASSERT_TRUE(owned.Generate());
    const std::string unsaved = owned.Map("base.bmp");
    EXPECT_NE(saved, unsaved);
    owned.editor.End();
    EXPECT_TRUE(owned.files.files.contains(saved));
    EXPECT_FALSE(owned.files.files.contains(unsaved));
}

TEST(MaterialWearGeneration, MissingScratchCollectionLeavesMaterialUnchanged)
{
    WearFixture owned;
    char error[512]{};
    EXPECT_FALSE(RunWear(owned.editor, 0, 0.5f, error, sizeof(error)));
    EXPECT_FALSE(std::string(error).empty());
    EXPECT_TRUE(owned.Map("base.bmp").empty());
    EXPECT_TRUE(owned.files.writes.empty());
}

TEST(MaterialWearGeneration, FailedSaveKeepsPreviewForRetry)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string path = owned.Map("base.bmp");
    owned.files.fail_document_write = true;
    char error[512]{};
    EXPECT_FALSE(owned.editor.Save(nullptr, error, sizeof(error)));
    EXPECT_TRUE(owned.files.files.contains(path));
    EXPECT_EQ(owned.Map("base.bmp"), path);
    owned.files.fail_document_write = false;
    ASSERT_TRUE(owned.editor.Save(nullptr, error, sizeof(error))) << error;
    owned.editor.End();
    EXPECT_TRUE(owned.files.files.contains(path));
}

TEST(MaterialWearGeneration, FailedSaveDoesNotRetainUnsavedMaps)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string path = owned.Map("base.bmp");
    owned.files.fail_document_write = true;
    char error[512]{};
    EXPECT_FALSE(owned.editor.Save(nullptr, error, sizeof(error)));
    owned.editor.End();
    EXPECT_FALSE(owned.files.files.contains(path));
}

TEST(MaterialWearGeneration, ReplacingSelectionRemovesUnsavedMaps)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string path = owned.Map("base.bmp");
    const char* selected[] = {"base.bmp"};
    ASSERT_TRUE(owned.editor.BeginGroup(owned.model.name, selected, 1, 0, [] { return false; }));
    EXPECT_FALSE(owned.files.files.contains(path));
}

TEST(MaterialWearGeneration, UnusedSelectedTextureRejectsWholeGroup)
{
    WearFixture owned;
    WearModelBlock binary;
    binary.submodel.nummesh = 1;
    owned.files.files[owned.model.name] = std::string(reinterpret_cast<const char*>(&binary), sizeof(binary));
    char error[512]{};
    EXPECT_FALSE(RunWear(owned.editor, 0, 0, error, sizeof(error)));
    EXPECT_FALSE(std::string(error).empty());
    EXPECT_TRUE(owned.Map("base.bmp").empty());
    EXPECT_TRUE(owned.files.writes.empty());
}

TEST(MaterialWearGeneration, UnsupportedProgramRejectsWithoutChangingMaterial)
{
    WearFixture owned;
    ASSERT_TRUE(owned.editor.SetEditedProgram("studio_legacy"));
    char error[512]{};
    EXPECT_FALSE(RunWear(owned.editor, 0, 0, error, sizeof(error)));
    EXPECT_FALSE(std::string(error).empty());
    EXPECT_STREQ(owned.editor.get_edited_program(), "studio_legacy");
    EXPECT_TRUE(owned.files.writes.empty());
}

TEST(MaterialWearGeneration, ReadCancellationDoesNotPublishMapsOrRestoreEndedSession)
{
    WearFixture owned;
    owned.files.before_read = [&] { owned.editor.End(); };
    char error[512]{};
    EXPECT_FALSE(RunWear(owned.editor, 0, 0, error, sizeof(error)));
    EXPECT_FALSE(owned.editor.is_editing());
    EXPECT_TRUE(owned.files.writes.empty());
}

TEST(MaterialWearGeneration, PartialImageWriteFailureRollsBackCreatedFilesAndKeepsPreviousMap)
{
    WearFixture owned;
    ASSERT_TRUE(owned.editor.SetTexture("uWearMap", "materials/previous.png"));
    owned.files.fail_generated_after = 1;
    char error[512]{};
    EXPECT_FALSE(RunWear(owned.editor, 0, 0, error, sizeof(error)));
    EXPECT_EQ(owned.Map("base.bmp"), "materials/previous.png");
    EXPECT_EQ(owned.Map("alternate.bmp"), "materials/previous.png");
    EXPECT_EQ(owned.files.files.size(), 1u);
    EXPECT_EQ(owned.files.generated_attempts, 2);
}

TEST(MaterialWearGeneration, WriteCancellationRemovesNewlyCreatedImage)
{
    WearFixture owned;
    owned.files.after_generated_write = [&] { owned.editor.End(); };
    char error[512]{};
    EXPECT_FALSE(RunWear(owned.editor, 0, 0, error, sizeof(error)));
    EXPECT_FALSE(owned.editor.is_editing());
    EXPECT_EQ(owned.files.files.size(), 1u);
}

TEST(MaterialWearGeneration, SaveReadCancellationDoesNotPersistMissingImages)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string path = owned.Map("base.bmp");
    owned.files.before_read = [&] { owned.editor.End(); };
    char error[512]{};
    EXPECT_FALSE(owned.editor.Save(nullptr, error, sizeof(error)));
    EXPECT_FALSE(owned.files.files.contains("models/wear.mat"));
    EXPECT_FALSE(owned.files.files.contains(path));
}

TEST(MaterialWearGeneration, SaveRetainsOnlyImagesReferencedByTheDocument)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string unused = owned.Map("base.bmp");
    ASSERT_TRUE(owned.editor.ClearParam("uWearMap"));
    char error[512]{};
    ASSERT_TRUE(owned.editor.Save(nullptr, error, sizeof(error))) << error;
    owned.editor.End();
    EXPECT_FALSE(owned.files.files.contains(unused));
}

TEST(MaterialWearGeneration, ScratchIntensityLowersThresholdWithoutChangingWear)
{
    WearFixture owned;
    const std::vector<uint8_t> black = {0, 0, 0, 255};
    const ncl_utils::ResultT<std::vector<uint8_t>> encoded = ncl_image::EncodePng(1, 1, 4, black);
    ASSERT_FALSE(encoded.has_error());
    owned.files.files["materials/masks/scratches/fine_random.png"] =
        std::string(reinterpret_cast<const char*>(encoded->data()), encoded->size());
    char error[512]{};
    ASSERT_TRUE(RunWear(owned.editor, 0, 0.5f, error, sizeof(error))) << error;
    const std::string& png = owned.files.files.at(owned.Map("base.bmp"));
    const ncl_utils::ResultT<ncl_image::RgbaImage, ncl_image::DecodeError> image =
        ncl_image::DecodeRgba8({reinterpret_cast<const uint8_t*>(png.data()), png.size()}, 1048576);
    ASSERT_FALSE(image.has_error());
    EXPECT_NEAR(image->pixels.get()[0], 64, 1);
    float wear = 0;
    ASSERT_TRUE(owned.editor.get_scalar("uWear", &wear));
    EXPECT_FLOAT_EQ(wear, 0.25f);
}

TEST(MaterialWearGeneration, BeginCopiesInputAndLeavesPublicationToPolling)
{
    WearFixture owned;
    char error[512]{};
    ASSERT_TRUE(owned.editor.BeginWearGeneration(0, 0, error, sizeof(error))) << error;
    EXPECT_TRUE(owned.Map("base.bmp").empty());
    EXPECT_TRUE(owned.files.writes.empty());
    owned.files.files.erase(owned.model.name);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    render::MaterialWearStatus status = render::MaterialWearStatus::Running;

    while (status == render::MaterialWearStatus::Running && std::chrono::steady_clock::now() < deadline)
    {
        status = owned.editor.PollWearGeneration(error, sizeof(error));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_EQ(status, render::MaterialWearStatus::Ready) << error;
    EXPECT_FALSE(owned.Map("base.bmp").empty());
}

TEST(MaterialWearGeneration, CancelDropsBackgroundResultAndReturnsIdle)
{
    WearFixture owned;
    char error[512]{};
    ASSERT_TRUE(owned.editor.BeginWearGeneration(0, 0, error, sizeof(error))) << error;
    owned.editor.CancelWearGeneration();
    EXPECT_EQ(owned.editor.PollWearGeneration(error, sizeof(error)), render::MaterialWearStatus::Idle);
    EXPECT_TRUE(owned.Map("base.bmp").empty());
    EXPECT_TRUE(owned.files.writes.empty());
}

TEST(MaterialWearGeneration, EditingAfterBeginRejectsObsoleteResult)
{
    WearFixture owned;
    char error[512]{};
    ASSERT_TRUE(owned.editor.BeginWearGeneration(0, 0, error, sizeof(error))) << error;
    ASSERT_TRUE(owned.editor.SetScalar("uWear", 0.75f));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    render::MaterialWearStatus status = render::MaterialWearStatus::Running;

    while (status == render::MaterialWearStatus::Running && std::chrono::steady_clock::now() < deadline)
    {
        status = owned.editor.PollWearGeneration(error, sizeof(error));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_EQ(status, render::MaterialWearStatus::Failed);
    EXPECT_TRUE(owned.Map("base.bmp").empty());
    float wear = 0;
    ASSERT_TRUE(owned.editor.get_scalar("uWear", &wear));
    EXPECT_FLOAT_EQ(wear, 0.75f);
    EXPECT_TRUE(owned.files.writes.empty());
}

TEST(MaterialWearGeneration, LatestBackgroundRequestReplacesCanceledWork)
{
    WearFixture owned;
    const std::vector<uint8_t> black = {0, 0, 0, 255};
    const ncl_utils::ResultT<std::vector<uint8_t>> encoded = ncl_image::EncodePng(1, 1, 4, black);
    ASSERT_FALSE(encoded.has_error());
    owned.files.files["materials/masks/scratches/fine_random.png"] =
        std::string(reinterpret_cast<const char*>(encoded->data()), encoded->size());
    char error[512]{};
    ASSERT_TRUE(owned.editor.BeginWearGeneration(0, 0, error, sizeof(error))) << error;
    ASSERT_TRUE(owned.editor.BeginWearGeneration(0, 0.25f, error, sizeof(error))) << error;
    ASSERT_TRUE(RunWear(owned.editor, 0, 0.5f, error, sizeof(error))) << error;
    EXPECT_EQ(owned.files.generated_attempts, 2);
    const std::string& png = owned.files.files.at(owned.Map("base.bmp"));
    const ncl_utils::ResultT<ncl_image::RgbaImage, ncl_image::DecodeError> image =
        ncl_image::DecodeRgba8({reinterpret_cast<const uint8_t*>(png.data()), png.size()}, 1048576);
    ASSERT_FALSE(image.has_error());
    EXPECT_NEAR(image->pixels.get()[0], 64, 1);
}

TEST(MaterialWearGeneration, CancelDuringInputReadStopsTheRequest)
{
    WearFixture owned;
    owned.files.before_read = [&] { owned.editor.CancelWearGeneration(); };
    char error[512]{};
    EXPECT_FALSE(owned.editor.BeginWearGeneration(0, 0, error, sizeof(error)));
    EXPECT_EQ(owned.editor.PollWearGeneration(error, sizeof(error)), render::MaterialWearStatus::Idle);
    EXPECT_TRUE(owned.Map("base.bmp").empty());
    EXPECT_TRUE(owned.files.writes.empty());
}

TEST(MaterialWearGeneration, NewRequestDuringInputReadSupersedesTheOuterRequest)
{
    WearFixture owned;
    const std::vector<uint8_t> black = {0, 0, 0, 255};
    const ncl_utils::ResultT<std::vector<uint8_t>> encoded = ncl_image::EncodePng(1, 1, 4, black);
    ASSERT_FALSE(encoded.has_error());
    owned.files.files["materials/masks/scratches/fine_random.png"] =
        std::string(reinterpret_cast<const char*>(encoded->data()), encoded->size());
    owned.files.before_read = [&] {
        char nested_error[512]{};
        EXPECT_TRUE(owned.editor.BeginWearGeneration(0, 0.5f, nested_error, sizeof(nested_error))) << nested_error;
    };
    char error[512]{};
    EXPECT_FALSE(owned.editor.BeginWearGeneration(0, 0, error, sizeof(error)));
    ASSERT_TRUE(WaitForWear(owned.editor, error, sizeof(error))) << error;
    const std::string& png = owned.files.files.at(owned.Map("base.bmp"));
    const ncl_utils::ResultT<ncl_image::RgbaImage, ncl_image::DecodeError> image =
        ncl_image::DecodeRgba8({reinterpret_cast<const uint8_t*>(png.data()), png.size()}, 1048576);
    ASSERT_FALSE(image.has_error());
    EXPECT_NEAR(image->pixels.get()[0], 64, 1);
}

TEST(MaterialWearGeneration, CancelDuringImageWriteDoesNotPublishCanceledMaps)
{
    WearFixture owned;
    owned.files.after_generated_write = [&] { owned.editor.CancelWearGeneration(); };
    char error[512]{};
    EXPECT_FALSE(RunWear(owned.editor, 0, 0, error, sizeof(error)));
    EXPECT_EQ(owned.editor.PollWearGeneration(error, sizeof(error)), render::MaterialWearStatus::Idle);
    EXPECT_TRUE(owned.Map("base.bmp").empty());
    EXPECT_EQ(owned.files.files.size(), 1u);
}

TEST(MaterialWearGeneration, RecalculationRetiresOldUnsavedMapsAndPreservesSavedImages)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string saved = owned.Map("base.bmp");
    char error[512]{};
    ASSERT_TRUE(owned.editor.Save(nullptr, error, sizeof(error))) << error;
    ASSERT_TRUE(owned.Generate());
    const std::string obsolete = owned.Map("base.bmp");
    ASSERT_TRUE(owned.editor.SetTexture("uWearMap", saved.c_str()));
    int barriers = 0;
    owned.editor.SetImageRetirementBarrier([&] {
        ++barriers;
        if (barriers == 1)
        {
            EXPECT_TRUE(owned.files.files.contains(obsolete));
        }
    });
    ASSERT_TRUE(owned.Generate());
    EXPECT_FALSE(owned.files.files.contains(obsolete));
    EXPECT_TRUE(owned.files.files.contains(saved));
    EXPECT_EQ(barriers, 1);
    const std::string current = owned.Map("base.bmp");
    ASSERT_TRUE(owned.Generate());
    EXPECT_FALSE(owned.files.files.contains(current));
    EXPECT_TRUE(owned.files.files.contains(saved));
    EXPECT_EQ(owned.files.files.size(), 6u);
    float wear = 0;
    ASSERT_TRUE(owned.editor.get_scalar("uWear", &wear));
    EXPECT_FLOAT_EQ(wear, 0.25f);
}

TEST(MaterialWearGeneration, SaveRetainsGeneratedImageReferencedWithDifferentPathCase)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string path = owned.Map("base.bmp");
    std::string alias = path;
    alias.replace(0, 9, "MATERIALS");
    ASSERT_TRUE(owned.editor.SetTexture("uWearMap", alias.c_str()));
    char error[512]{};
    ASSERT_TRUE(owned.editor.Save(nullptr, error, sizeof(error))) << error;
    owned.editor.End();
    EXPECT_TRUE(owned.files.files.contains(path));
}

TEST(MaterialWearGeneration, RecalculationPreservesGeneratedImagesReferencedByOtherParameters)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string retained = owned.Map("base.bmp");
    std::string alias = retained;
    alias.replace(0, 9, "MATERIALS");
    ASSERT_TRUE(owned.editor.SetTexture("uUnderlay", alias.c_str()));
    ASSERT_TRUE(owned.Generate());
    EXPECT_TRUE(owned.files.files.contains(retained));
    EXPECT_STREQ(owned.editor.get_texture("uUnderlay"), alias.c_str());
    owned.editor.End();
    EXPECT_FALSE(owned.files.files.contains(retained));
}

TEST(MaterialWearGeneration, SaveDuringRetirementPreservesThePublishedDocumentImages)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string first = owned.Map("base.bmp");
    const std::string second = owned.Map("alternate.bmp");
    owned.editor.SetImageRetirementBarrier([&] {
        char error[512]{};
        ASSERT_TRUE(owned.editor.Save(nullptr, error, sizeof(error))) << error;
    });
    ASSERT_TRUE(owned.Generate());
    EXPECT_TRUE(owned.files.files.contains(first));
    EXPECT_TRUE(owned.files.files.contains(second));
    EXPECT_NE(owned.Map("base.bmp"), first);
    const ncl_utils::ResultT<ncl_material::MaterialDoc> doc = ncl_material::ParseMaterialDoc(owned.files.files.at("models/wear.mat"));
    ASSERT_FALSE(doc.has_error());
    ASSERT_NE(doc->FindByTexture("base.bmp"), nullptr);
    EXPECT_EQ(doc->FindByTexture("base.bmp")->FindParam("uWearMap")->texture_ref, first);
}

TEST(MaterialWearGeneration, SelectionReplacementKeepsTheNewerSessionStartedByRetirement)
{
    WearFixture owned;
    ASSERT_TRUE(owned.Generate());
    const std::string discarded = owned.Map("base.bmp");
    bool entered = false;
    owned.editor.SetImageRetirementBarrier([&] {
        if (entered)
        {
            return;
        }

        entered = true;
        const char* nested[] = {"alternate.bmp"};
        ASSERT_TRUE(owned.editor.BeginGroup(owned.model.name, nested, 1, 0, [] { return false; }));
        ASSERT_TRUE(owned.editor.SetEditedProgram("weapon_finish"));
        ASSERT_TRUE(owned.editor.SetScalar("uWear", 0.8f));
    });
    const char* outer[] = {"base.bmp"};
    EXPECT_FALSE(owned.editor.BeginGroup(owned.model.name, outer, 1, 0, [] { return false; }));
    EXPECT_TRUE(entered);
    ASSERT_EQ(owned.editor.get_selection_count(), 1);
    EXPECT_STREQ(owned.editor.get_selected_texture(0), "alternate.bmp");
    float wear = 0;
    ASSERT_TRUE(owned.editor.get_scalar("uWear", &wear));
    EXPECT_FLOAT_EQ(wear, 0.8f);
    EXPECT_FALSE(owned.files.files.contains(discarded));
}

TEST(MaterialWearGeneration, NewRequestDuringImageWriteKeepsItsRunningStatus)
{
    WearFixture owned;
    const std::vector<uint8_t> black = {0, 0, 0, 255};
    const ncl_utils::ResultT<std::vector<uint8_t>> encoded = ncl_image::EncodePng(1, 1, 4, black);
    ASSERT_FALSE(encoded.has_error());
    owned.files.files["materials/masks/scratches/fine_random.png"] =
        std::string(reinterpret_cast<const char*>(encoded->data()), encoded->size());
    owned.files.after_generated_write = [&] {
        char nested_error[512]{};
        EXPECT_TRUE(owned.editor.BeginWearGeneration(0, 0.5f, nested_error, sizeof(nested_error))) << nested_error;
    };
    char error[512]{};
    ASSERT_TRUE(RunWear(owned.editor, 0, 0, error, sizeof(error))) << error;
    EXPECT_EQ(owned.files.generated_attempts, 3);
    EXPECT_EQ(owned.files.files.size(), 4u);
    const std::string& png = owned.files.files.at(owned.Map("base.bmp"));
    const ncl_utils::ResultT<ncl_image::RgbaImage, ncl_image::DecodeError> image =
        ncl_image::DecodeRgba8({reinterpret_cast<const uint8_t*>(png.data()), png.size()}, 1048576);
    ASSERT_FALSE(image.has_error());
    EXPECT_NEAR(image->pixels.get()[0], 64, 1);
}
