#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "PreviewTestOwners.h"
#include "StudioModelTestSupport.h"

namespace
{
    class ModelFiles final : public render::MaterialFileBackend
    {
    public:
        std::unordered_map<std::string, std::string> contents;
        std::vector<std::string> errors;

        render::MaterialReadStatus ReadFile(const char* path, std::string& text, size_t) override
        {
            auto found = contents.find(path);
            if (found == contents.end())
            {
                return render::MaterialReadStatus::NotFound;
            }
            text = found->second;
            return render::MaterialReadStatus::Ready;
        }
        bool Write(const std::string&, const std::string&, std::string&) override
        {
            return false;
        }
        void Report(const char* text, bool) override
        {
            errors.emplace_back(text);
        }
    };

    template <int Index>
    struct NativeModels
    {
        inline static NativeModels* active{};
        std::vector<goldsrc::model_t> known;
        int count{};
        int loads{};
        int data_calls{};
        bool fail_metadata{};
        bool replace_during_data{};
        bool companion_after_inspection{};
        studio_test::ModelBlock block;
        studio_test::ModelBlock replacement;
        goldsrc::r_studio_interface_t studio{};
        goldsrc::cl_entity_t* entity{};
        float blend = 1;

        NativeModels() :
            known(goldsrc::MAX_KNOWN_MODELS)
        {
            active = this;
            studio.StudioDrawModel = [](int) { return 1; };
        }
        render::preview::PreviewEngineBindings Bindings()
        {
            return {&studio, &entity, &blend, known.data(), &count, Load, Data};
        }
        static goldsrc::model_t* Load(const char* path, goldsrc::qboolean, goldsrc::qboolean)
        {
            NativeModels& self = *active;
            self.loads++;
            if (self.companion_after_inspection)
            {
                self.block.header.numtextures = 0;
            }
            for (int index = 0; index < self.count; index++)
            {
                if (std::strcmp(self.known[index].name, path) == 0)
                {
                    return &self.known[index];
                }
            }
            goldsrc::model_t& model = self.known[self.count++];
            std::strcpy(model.name, path);
            model.type = goldsrc::mod_studio;
            model.cache.data = &self.block;
            return &model;
        }
        static void* Data(goldsrc::model_t* model)
        {
            NativeModels& self = *active;
            self.data_calls++;
            void* data = model->cache.data;
            if (self.replace_during_data)
            {
                model->cache.data = &self.replacement;
            }
            if (self.fail_metadata)
            {
                self.block.header.seqindex = -1;
            }
            return data;
        }
        void AddFile(ModelFiles& files, const std::string& path)
        {
            files.contents[path] = std::string(reinterpret_cast<const char*>(&block), sizeof(block));
        }
    };

    template <int Index>
    struct ModelOwners
    {
        preview_test::Owners owners;
        ModelFiles files;
        NativeModels<Index> native;
        render::preview::PreviewEngineBindings bindings = native.Bindings();
        render::preview::EnginePreviewModelBackend backend{owners.state.models, bindings, files};
        render::preview::PreviewModelRepository repository{backend};
    };
} // namespace

TEST(PreviewModelRepository, MissingRendererDoesNotLoadAModel)
{
    ModelOwners<0> owner;
    owner.bindings.studio = nullptr;
    PreviewModelInfo info{};
    info.sequence_count = 7;
    EXPECT_EQ(owner.repository.PrepareModel("models/test.mdl", &info), PreviewModelStatus::RendererUnavailable);
    EXPECT_EQ(info.sequence_count, 0);
    EXPECT_EQ(owner.native.loads, 0);
}

TEST(PreviewModelRepository, AdmissionPreservesBothExactCapacityBoundaries)
{
    using render::preview::CanAdmitModel;
    EXPECT_TRUE(CanAdmitModel(126, 2, 2));
    EXPECT_FALSE(CanAdmitModel(127, 2, 2));
    EXPECT_FALSE(CanAdmitModel(126, 2, 1));
    EXPECT_TRUE(CanAdmitModel(128, 0, 0));
}

TEST(PreviewModelRepository, ReturnsRealPreparedMetadataAndOperationIdentity)
{
    ModelOwners<0> owner;
    owner.native.AddFile(owner.files, "models/test.mdl");
    PreviewModelInfo info{};
    render::preview::PreparedPreviewModel prepared{};
    ASSERT_EQ(owner.repository.PrepareModel("models/test.mdl", &info, &prepared), PreviewModelStatus::Ready);
    EXPECT_EQ(info.sequence_count, 3);
    EXPECT_EQ(info.skin_family_count, 2);
    EXPECT_EQ(info.body_count, 2);
    EXPECT_EQ(owner.repository.get_loaded_entries(), 1);
    EXPECT_EQ(prepared.model, &owner.native.known[0]);
    EXPECT_EQ(prepared.header, &owner.native.block.header);
    EXPECT_EQ(owner.native.loads, 1);
    EXPECT_EQ(owner.native.data_calls, 1);
}

TEST(PreviewModelRepository, InvalidFileDoesNotConsumeTheBudgetOrLoad)
{
    ModelOwners<0> owner;
    owner.files.contents["models/test.mdl"] = "invalid model";
    PreviewModelInfo info{};
    EXPECT_EQ(owner.repository.PrepareModel("models/test.mdl", &info), PreviewModelStatus::InvalidModel);
    EXPECT_EQ(owner.repository.get_loaded_entries(), 0);
    EXPECT_EQ(owner.native.loads, 0);
}

TEST(PreviewModelRepository, SharedBudgetAllowsKnownModelsAfterExhaustion)
{
    ModelOwners<0> owner;
    PreviewModelInfo info{};
    for (int index = 0; index < 128; index++)
    {
        std::string path = "models/test" + std::to_string(index) + ".mdl";
        owner.native.AddFile(owner.files, path);
        ASSERT_EQ(owner.repository.PrepareModel(path.c_str(), &info), PreviewModelStatus::Ready);
    }
    owner.native.AddFile(owner.files, "models/another.mdl");
    EXPECT_EQ(owner.repository.get_loaded_entries(), 128);
    EXPECT_EQ(owner.repository.PrepareModel("models/another.mdl", &info), PreviewModelStatus::CapacityExceeded);
    EXPECT_EQ(owner.repository.PrepareModel("models/test0.mdl", &info), PreviewModelStatus::Ready);
    EXPECT_EQ(owner.repository.get_loaded_entries(), 128);
}

TEST(PreviewModelRepository, FailedMetadataPreparationRetainsConsumedEngineCapacity)
{
    ModelOwners<0> owner;
    owner.native.AddFile(owner.files, "models/test.mdl");
    owner.native.fail_metadata = true;
    PreviewModelInfo info{};
    render::preview::PreparedPreviewModel prepared{};
    EXPECT_EQ(owner.repository.PrepareModel("models/test.mdl", &info, &prepared), PreviewModelStatus::InvalidModel);
    EXPECT_EQ(owner.repository.get_loaded_entries(), 1);
    EXPECT_EQ(info.sequence_count, 0);
    EXPECT_EQ(prepared.model, nullptr);
}

TEST(PreviewModelRepository, ReplacementDuringNativeResolutionDoesNotPublishBorrowedIdentity)
{
    ModelOwners<0> owner;
    owner.native.AddFile(owner.files, "models/test.mdl");
    owner.native.replace_during_data = true;
    PreviewModelInfo info{};
    render::preview::PreparedPreviewModel prepared{};
    EXPECT_EQ(owner.repository.PrepareModel("models/test.mdl", &info, &prepared), PreviewModelStatus::InvalidModel);
    EXPECT_EQ(prepared.model, nullptr);
}

TEST(PreviewModelRepository, LoadedHeaderCannotOverflowTheCompanionPathAfterInspection)
{
    ModelOwners<0> owner;
    std::string path = "models/" + std::string(goldsrc::MAX_MODEL_NAME - 12, 'a') + ".mdl";
    ASSERT_EQ(path.size(), goldsrc::MAX_MODEL_NAME - 1);
    owner.native.AddFile(owner.files, path);
    owner.native.companion_after_inspection = true;
    PreviewModelInfo info{};
    render::preview::PreparedPreviewModel prepared{};

    EXPECT_EQ(owner.repository.PrepareModel(path.c_str(), &info, &prepared), PreviewModelStatus::InvalidModel);
    EXPECT_EQ(owner.native.loads, 1);
    EXPECT_EQ(owner.repository.get_loaded_entries(), 1);
    EXPECT_EQ(prepared.model, nullptr);
    EXPECT_EQ(prepared.header, nullptr);
    EXPECT_EQ(info.sequence_count, 0);
}

TEST(PreviewModelRepository, IndependentOwnersKeepMetadataAndAdmissionSeparateAcrossSessionReset)
{
    ModelOwners<0> first;
    ModelOwners<1> second;
    first.native.AddFile(first.files, "models/test.mdl");
    second.native.AddFile(second.files, "models/test.mdl");
    second.native.block.header.numseq = 2;
    PreviewModelInfo info{};
    ASSERT_EQ(first.repository.PrepareModel("models/test.mdl", &info), PreviewModelStatus::Ready);
    EXPECT_EQ(info.sequence_count, 3);
    ASSERT_EQ(second.repository.PrepareModel("models/test.mdl", &info), PreviewModelStatus::Ready);
    EXPECT_EQ(info.sequence_count, 2);
    first.owners.state.models.ForgetSessionMetadata();
    first.repository.Reset();
    first.native.count = 0;
    first.native.block.header.numseq = 1;
    ASSERT_EQ(first.repository.PrepareModel("models/test.mdl", &info), PreviewModelStatus::Ready);
    EXPECT_EQ(info.sequence_count, 1);
    ASSERT_EQ(second.repository.PrepareModel("models/test.mdl", &info), PreviewModelStatus::Ready);
    EXPECT_EQ(info.sequence_count, 2);
    EXPECT_EQ(first.repository.get_loaded_entries(), 1);
    EXPECT_EQ(second.repository.get_loaded_entries(), 1);
}

namespace
{
    char* FileReadPrint(const char*, ...)
    {
        return nullptr;
    }

    class FileReadConsoleHook : public studio_test::Hook<char*, const char*>
    {
    public:
        uint32_t GetOriginalFunctionAddress() const override
        {
            return reinterpret_cast<uint32_t>(FileReadPrint);
        }
    };

    class NativeFileReadTest : public testing::Test
    {
    private:
        nitroapi::NitroApiInterface* saved_engine_{};
        studio_test::FileReadProbe* saved_probe_{};
        studio_test::ModelEngine engine_;
        studio_test::Chain<char*, const char*> console_chain_;
        FileReadConsoleHook console_;

    protected:
        preview_test::Owners owners_;
        render::MaterialGameFiles files_;
        NativeModels<0> native_;
        render::preview::PreviewEngineBindings bindings_ = native_.Bindings();
        render::preview::EnginePreviewModelBackend backend_{owners_.state.models, bindings_, files_};
        render::preview::PreviewModelRepository repository_{backend_};
        studio_test::FileReadProbe probe_;

        void SetUp() override
        {
            saved_engine_ = g_NitroApi;
            saved_probe_ = studio_test::g_FileReadProbe;
            engine_.GetEngineData()->Con_Printf.SetChain(&console_chain_);
            engine_.GetEngineData()->Con_Printf.SetHook(&console_);
            g_NitroApi = &engine_;
            probe_.path = "models/native_io.mdl";
            studio_test::g_Files[probe_.path] = std::string(reinterpret_cast<const char*>(&native_.block), sizeof(native_.block));
            studio_test::g_FileReadProbe = &probe_;
        }

        void TearDown() override
        {
            studio_test::g_FileReadProbe = saved_probe_;
            studio_test::g_Files.erase(probe_.path);
            g_NitroApi = saved_engine_;
        }

        void ExpectNoModelLoad(PreviewModelStatus expected, int sizes, int reads, int closes)
        {
            PreviewModelInfo info{};
            EXPECT_EQ(repository_.PrepareModel(probe_.path.c_str(), &info), expected);
            EXPECT_EQ(native_.loads, 0);
            EXPECT_EQ(repository_.get_loaded_entries(), 0);
            EXPECT_EQ(probe_.opens, 1);
            EXPECT_EQ(probe_.sizes, sizes);
            EXPECT_EQ(probe_.reads, reads);
            EXPECT_EQ(probe_.closes, closes);
        }
    };
} // namespace

TEST_F(NativeFileReadTest, MissingModelRemainsNotFoundWithoutReadingOrClosingAFile)
{
    studio_test::g_Files.erase(probe_.path);
    ExpectNoModelLoad(PreviewModelStatus::NotFound, 0, 0, 0);
}

TEST_F(NativeFileReadTest, FailedOpenOfExistingDocumentRemainsReadFailedAndDoesNotReadOrClose)
{
    probe_.open_failed = true;
    std::string text = "unchanged";
    EXPECT_EQ(files_.ReadFile(probe_.path.c_str(), text, 0), render::MaterialReadStatus::ReadFailed);
    EXPECT_EQ(text, "unchanged");
    EXPECT_EQ(probe_.opens, 1);
    EXPECT_EQ(probe_.sizes, 0);
    EXPECT_EQ(probe_.reads, 0);
    EXPECT_EQ(probe_.closes, 0);
}

TEST_F(NativeFileReadTest, OversizedModelRemainsInvalidWithoutReadingItsPayload)
{
    probe_.size_override = static_cast<unsigned int>(std::numeric_limits<int>::max()) + 1u;
    ExpectNoModelLoad(PreviewModelStatus::InvalidModel, 1, 0, 1);
}

TEST_F(NativeFileReadTest, ShortModelReadRemainsInvalidAndClosesTheSingleOpenedFile)
{
    probe_.size_override = static_cast<unsigned int>(studio_test::g_Files[probe_.path].size()) + 1u;
    ExpectNoModelLoad(PreviewModelStatus::InvalidModel, 1, 1, 1);
}

TEST_F(NativeFileReadTest, NativeStatusReportsReadFailureWithoutCollapsingItIntoNotFound)
{
    probe_.size_override = static_cast<unsigned int>(studio_test::g_Files[probe_.path].size()) + 1u;
    std::string text;
    EXPECT_EQ(files_.ReadFile(probe_.path.c_str(), text, 0), render::MaterialReadStatus::ReadFailed);
    EXPECT_EQ(probe_.opens, 1);
    EXPECT_EQ(probe_.sizes, 1);
    EXPECT_EQ(probe_.reads, 1);
    EXPECT_EQ(probe_.closes, 1);
}

TEST_F(NativeFileReadTest, UnrepresentableSizeIsRejectedWithoutADocumentByteLimit)
{
    probe_.size_override = static_cast<unsigned int>(std::numeric_limits<int>::max()) + 1u;
    std::string text = "unchanged";
    EXPECT_EQ(files_.ReadFile(probe_.path.c_str(), text, 0), render::MaterialReadStatus::TooLarge);
    EXPECT_EQ(text, "unchanged");
    EXPECT_EQ(probe_.opens, 1);
    EXPECT_EQ(probe_.sizes, 1);
    EXPECT_EQ(probe_.reads, 0);
    EXPECT_EQ(probe_.closes, 1);
}

TEST_F(NativeFileReadTest, DocumentReadRetainsItsBooleanContractAndCallerLimit)
{
    studio_test::g_Files[probe_.path] = "document";
    render::MaterialDocumentStore documents(files_);
    std::string text;
    EXPECT_TRUE(documents.Read(probe_.path.c_str(), text));
    EXPECT_EQ(text, "document");
    EXPECT_FALSE(documents.Read(probe_.path.c_str(), text, 1));
    studio_test::g_Files.erase(probe_.path);
    EXPECT_FALSE(documents.Read(probe_.path.c_str(), text));
    EXPECT_EQ(probe_.opens, 3);
    EXPECT_EQ(probe_.sizes, 2);
    EXPECT_EQ(probe_.reads, 1);
    EXPECT_EQ(probe_.closes, 2);
}
