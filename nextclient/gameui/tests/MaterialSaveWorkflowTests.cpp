#include <functional>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <tier1/strtools.h>

#include "GameUi/MaterialEditor/MaterialSaveWorkflow.h"

namespace
{
    struct ModelResult
    {
        PreviewModelStatus status = PreviewModelStatus::Ready;
        std::vector<std::string> textures;
        bool saved = true;
        std::string error;
    };

    class WorkflowBackend : public material_editor::SaveBackend
    {
    private:
        std::string texture_name_;

    public:
        std::map<std::string, ModelResult> models;
        std::vector<std::string> calls;
        std::vector<std::string> writes;
        std::vector<int> error_sizes;
        std::function<void()> on_prepare;
        std::function<void()> on_save;

        PreviewModelStatus PrepareModel(const std::string& model) override
        {
            calls.push_back("prepare " + model);
            texture_name_.clear();

            if (on_prepare)
            {
                on_prepare();
            }

            return models.at(model).status;
        }

        int get_texture_count(const std::string& model) override
        {
            calls.push_back("count " + model);
            texture_name_.clear();

            return static_cast<int>(models.at(model).textures.size());
        }

        const char* get_texture_name(const std::string& model, int index) override
        {
            calls.push_back("texture " + model + " " + std::to_string(index));
            texture_name_ = models.at(model).textures.at(index);

            return texture_name_.c_str();
        }

        bool Save(const std::string& model, char* error, int error_size) override
        {
            calls.push_back("save " + model);
            writes.push_back(model);
            error_sizes.push_back(error_size);
            texture_name_.clear();

            if (on_save)
            {
                on_save();
            }

            const ModelResult& result = models.at(model);
            V_strncpy(error, result.error.c_str(), error_size);

            return result.saved;
        }
    };
} // namespace

TEST(MaterialSaveWorkflow, SavesCurrentAndRelatedTargetsInInputOrder)
{
    WorkflowBackend backend;
    backend.models["models/current.mdl"].textures = {"body"};
    backend.models["models/w_related.mdl"].textures = {"body"};
    backend.models["models/p_related.mdl"].textures = {"body"};
    const std::vector<std::string> targets = {"models/current.mdl", "models/w_related.mdl", "models/p_related.mdl"};
    const std::vector<std::string> selected = {"body"};

    EXPECT_TRUE(material_editor::SaveMaterialCopies(backend, targets, selected).empty());
    EXPECT_EQ(backend.writes, targets);
}

TEST(MaterialSaveWorkflow, PreparesMatchesAndSavesEachTargetBeforeTheNextPreparation)
{
    WorkflowBackend backend;
    backend.models["current"].textures = {"body"};
    backend.models["related"].textures = {"body"};
    const std::vector<std::string> targets = {"current", "related"};
    const std::vector<std::string> selected = {"body"};

    EXPECT_TRUE(material_editor::SaveMaterialCopies(backend, targets, selected).empty());
    EXPECT_EQ(
        backend.calls,
        (std::vector<std::string>{
            "prepare current",
            "count current",
            "texture current 0",
            "save current",
            "prepare related",
            "count related",
            "texture related 0",
            "save related",
        })
    );
}

TEST(MaterialSaveWorkflow, ContinuesAfterPreparationMatchingAndWriteFailures)
{
    WorkflowBackend backend;
    backend.models["capacity"].status = PreviewModelStatus::CapacityExceeded;
    backend.models["missing"].textures = {"other"};
    backend.models["write"].textures = {"body"};
    backend.models["write"].saved = false;
    backend.models["write"].error = "write failed";
    backend.models["success"].textures = {"body", "trim"};
    const std::vector<std::string> targets = {"capacity", "missing", "write", "success"};
    const std::vector<std::string> selected = {"body"};

    const std::vector<material_editor::SaveFailure> failures = material_editor::SaveMaterialCopies(backend, targets, selected);

    ASSERT_EQ(failures.size(), 3u);
    EXPECT_EQ(failures[0].model, "capacity");
    EXPECT_EQ(failures[0].reason, material_editor::SaveFailureReason::ModelCapacity);
    EXPECT_TRUE(failures[0].error.empty());
    EXPECT_EQ(failures[1].model, "missing");
    EXPECT_EQ(failures[1].reason, material_editor::SaveFailureReason::NoMatchingTexture);
    EXPECT_EQ(failures[2].model, "write");
    EXPECT_EQ(failures[2].reason, material_editor::SaveFailureReason::SaveFailed);
    EXPECT_EQ(failures[2].error, "write failed");
    EXPECT_EQ(backend.writes, (std::vector<std::string>{"write", "success"}));
    EXPECT_EQ(
        backend.calls,
        (std::vector<std::string>{
            "prepare capacity",
            "prepare missing",
            "count missing",
            "texture missing 0",
            "count missing",
            "prepare write",
            "count write",
            "texture write 0",
            "save write",
            "prepare success",
            "count success",
            "texture success 0",
            "save success",
        })
    );
}

TEST(MaterialSaveWorkflow, MatchesEverySelectedTextureIgnoringCase)
{
    WorkflowBackend backend;
    backend.models["complete"].textures = {"TRIM", "Body"};
    backend.models["incomplete"].textures = {"Body"};
    const std::vector<std::string> targets = {"complete", "incomplete"};
    const std::vector<std::string> selected = {"BODY", "trim"};

    const std::vector<material_editor::SaveFailure> failures = material_editor::SaveMaterialCopies(backend, targets, selected);

    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].model, "incomplete");
    EXPECT_EQ(failures[0].reason, material_editor::SaveFailureReason::NoMatchingTexture);
    EXPECT_EQ(backend.writes, (std::vector<std::string>{"complete"}));
}

TEST(MaterialSaveWorkflow, StopsMatchingAtTheFirstMissingSelection)
{
    WorkflowBackend backend;
    backend.models["target"].textures = {"body"};
    const std::vector<std::string> targets = {"target"};
    const std::vector<std::string> selected = {"missing", "body"};

    const std::vector<material_editor::SaveFailure> failures = material_editor::SaveMaterialCopies(backend, targets, selected);

    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].reason, material_editor::SaveFailureReason::NoMatchingTexture);
    EXPECT_TRUE(backend.writes.empty());
    EXPECT_EQ(backend.calls, (std::vector<std::string>{"prepare target", "count target", "texture target 0", "count target"}));
}

TEST(MaterialSaveWorkflow, ClassifiesEveryPreviewStatus)
{
    WorkflowBackend backend;
    backend.models["ready"].status = PreviewModelStatus::Ready;
    backend.models["notfound"].status = PreviewModelStatus::NotFound;
    backend.models["invalid"].status = PreviewModelStatus::InvalidModel;
    backend.models["capacity"].status = PreviewModelStatus::CapacityExceeded;
    backend.models["unavailable"].status = PreviewModelStatus::RendererUnavailable;
    const std::vector<std::string> targets = {"ready", "notfound", "invalid", "capacity", "unavailable"};
    const std::vector<std::string> selected;

    const std::vector<material_editor::SaveFailure> failures = material_editor::SaveMaterialCopies(backend, targets, selected);

    ASSERT_EQ(failures.size(), 4u);
    EXPECT_EQ(failures[0].model, "notfound");
    EXPECT_EQ(failures[0].reason, material_editor::SaveFailureReason::ModelLoadFailed);
    EXPECT_EQ(failures[1].model, "invalid");
    EXPECT_EQ(failures[1].reason, material_editor::SaveFailureReason::ModelLoadFailed);
    EXPECT_EQ(failures[2].model, "capacity");
    EXPECT_EQ(failures[2].reason, material_editor::SaveFailureReason::ModelCapacity);
    EXPECT_EQ(failures[3].model, "unavailable");
    EXPECT_EQ(failures[3].reason, material_editor::SaveFailureReason::RendererUnavailable);
    EXPECT_EQ(backend.writes, (std::vector<std::string>{"ready"}));
    EXPECT_EQ(
        backend.calls,
        (std::vector<std::string>{
            "prepare ready", "save ready", "prepare notfound", "prepare invalid", "prepare capacity", "prepare unavailable"
        })
    );
}

TEST(MaterialSaveWorkflow, ZeroSelectedTexturesStillSavePreparedTargets)
{
    WorkflowBackend backend;
    backend.models["empty"] = ModelResult{};
    const std::vector<std::string> targets = {"empty"};
    const std::vector<std::string> selected;

    EXPECT_TRUE(material_editor::SaveMaterialCopies(backend, targets, selected).empty());
    EXPECT_EQ(backend.calls, (std::vector<std::string>{"prepare empty", "save empty"}));
}

TEST(MaterialSaveWorkflow, PreservesDuplicateTargetRequests)
{
    WorkflowBackend backend;
    backend.models["same"].saved = false;
    backend.models["same"].error = "denied";
    const std::vector<std::string> targets = {"same", "same"};
    const std::vector<std::string> selected;

    const std::vector<material_editor::SaveFailure> failures = material_editor::SaveMaterialCopies(backend, targets, selected);

    ASSERT_EQ(failures.size(), 2u);
    EXPECT_EQ(failures[0].model, "same");
    EXPECT_EQ(failures[1].model, "same");
    EXPECT_EQ(backend.writes, targets);
    EXPECT_EQ(backend.calls, (std::vector<std::string>{"prepare same", "save same", "prepare same", "save same"}));
}

TEST(MaterialSaveWorkflow, CopiesInputsBeforePreparationCanReplaceSessionStrings)
{
    WorkflowBackend backend;
    backend.models["current"].textures = {"body", "trim"};
    backend.models["related"].textures = {"body", "trim"};
    std::vector<std::string> targets = {"current", "related"};
    std::vector<std::string> selected = {"body", "trim"};
    backend.on_prepare = [&targets, &selected]() {
        targets[0] = "replacement current";
        targets[1] = "replacement related";
        selected[0] = "replacement body";
        selected[1] = "replacement trim";
    };

    EXPECT_TRUE(material_editor::SaveMaterialCopies(backend, targets, selected).empty());
    EXPECT_EQ(backend.writes, (std::vector<std::string>{"current", "related"}));
}

TEST(MaterialSaveWorkflow, CopiesErrorsAndPathsAcrossLaterBackendMutations)
{
    WorkflowBackend backend;
    backend.models["first"].saved = false;
    backend.models["first"].error = "first failure";
    backend.models["second"].saved = false;
    backend.models["second"].error = "second failure";
    std::vector<std::string> targets = {"first", "second"};
    const std::vector<std::string> selected;
    backend.on_save = [&targets]() {
        targets[0] = "changed first";
        targets[1] = "changed second";
    };

    const std::vector<material_editor::SaveFailure> failures = material_editor::SaveMaterialCopies(backend, targets, selected);
    backend.models.clear();
    targets.clear();

    ASSERT_EQ(failures.size(), 2u);
    EXPECT_EQ(failures[0].model, "first");
    EXPECT_EQ(failures[0].error, "first failure");
    EXPECT_EQ(failures[1].model, "second");
    EXPECT_EQ(failures[1].error, "second failure");
}

TEST(MaterialSaveWorkflow, RetainsTheBoundedSaveErrorBuffer)
{
    WorkflowBackend backend;
    backend.models["target"].saved = false;
    backend.models["target"].error.assign(700, 'e');
    const std::vector<std::string> targets = {"target"};
    const std::vector<std::string> selected;

    const std::vector<material_editor::SaveFailure> failures = material_editor::SaveMaterialCopies(backend, targets, selected);

    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].error, std::string(511, 'e'));
    EXPECT_EQ(backend.error_sizes, (std::vector<int>{512}));
}
TEST(MaterialSaveWorkflow, KeepsSelectedNamesAcrossSuccessfulDocumentReload)
{
    WorkflowBackend backend;
    backend.models["current"].textures = {"body", "trim"};
    backend.models["related"].textures = {"body", "trim"};
    std::vector<std::string> targets = {"current", "related"};
    std::vector<std::string> selected = {"body", "trim"};
    backend.on_save = [&targets, &selected]() {
        targets[0] = "reloaded current";
        targets[1] = "reloaded related";
        selected[0] = "reloaded body";
        selected[1] = "reloaded trim";
    };

    EXPECT_TRUE(material_editor::SaveMaterialCopies(backend, targets, selected).empty());
    EXPECT_EQ(backend.writes, (std::vector<std::string>{"current", "related"}));
}
