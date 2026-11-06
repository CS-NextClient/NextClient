#include <functional>

#include <gtest/gtest.h>

#include "GameUi/MaterialEditor/MaterialTransitionController.h"

namespace
{
    using namespace material_editor;

    class SaveService : public TransitionSaveService
    {
    public:
        int calls{};
        std::vector<SaveFailure> failures;
        std::function<void()> on_save;

        std::vector<SaveFailure> Save() override
        {
            ++calls;
            if (on_save)
            {
                on_save();
            }

            return failures;
        }
    };

    DocumentSelection Current()
    {
        return {"current.mdl", "body", {"body"}};
    }

    TransitionRequest Change(TransitionKind kind)
    {
        return {kind, {kind == TransitionKind::Model ? "other.mdl" : "current.mdl", "trim", {"trim", "body"}}};
    }
} // namespace

TEST(MaterialTransitionController, CleanRequestsApplyOwnedIntent)
{
    for (TransitionKind kind : {TransitionKind::Close, TransitionKind::Model, TransitionKind::Texture, TransitionKind::TextureGroup})
    {
        TransitionController controller;
        TransitionRequest request = Change(kind);
        TransitionResult result = controller.Request(request, Current(), false);
        request.target.textures.clear();

        ASSERT_EQ(result.decision, TransitionDecision::Apply);
        ASSERT_TRUE(result.request.has_value());
        EXPECT_EQ(result.request->kind, kind);
        EXPECT_EQ(result.request->target.model, kind == TransitionKind::Model ? "other.mdl" : "current.mdl");
        EXPECT_EQ(result.request->target.textures, (std::vector<std::string>{"trim", "body"}));
        EXPECT_FALSE(controller.is_pending());
    }
}

TEST(MaterialTransitionController, DirtyRequestsConfirmAndDiscardAppliesOriginalIntent)
{
    for (TransitionKind kind : {TransitionKind::Close, TransitionKind::Model, TransitionKind::Texture, TransitionKind::TextureGroup})
    {
        TransitionController controller;
        SaveService service;
        TransitionRequest request = Change(kind);
        const TransitionResult confirmation = controller.Request(request, Current(), true);
        request.target.textures[0] = "changed";

        ASSERT_EQ(confirmation.decision, TransitionDecision::Confirm);
        EXPECT_NE(confirmation.token, 0u);
        EXPECT_FALSE(confirmation.request.has_value());
        EXPECT_TRUE(controller.is_pending());
        EXPECT_EQ(controller.Request(Change(TransitionKind::Close), Current(), false).decision, TransitionDecision::Ignored);
        EXPECT_EQ(controller.Request(Change(TransitionKind::Texture), Current(), true).decision, TransitionDecision::Ignored);

        const TransitionResult result = controller.Resolve(confirmation.token, TransitionChoice::Discard, service);
        ASSERT_EQ(result.decision, TransitionDecision::Apply);
        ASSERT_TRUE(result.request.has_value());
        EXPECT_EQ(result.request->kind, kind);
        EXPECT_EQ(result.request->target.textures, (std::vector<std::string>{"trim", "body"}));
        EXPECT_EQ(service.calls, 0);
        EXPECT_FALSE(controller.is_pending());
        EXPECT_EQ(controller.Resolve(confirmation.token, TransitionChoice::Discard, service).decision, TransitionDecision::Ignored);
    }
}

TEST(MaterialTransitionController, SuppressesOnlyEquivalentSelections)
{
    TransitionController controller;
    DocumentSelection current = Current();
    EXPECT_EQ(controller.Request({TransitionKind::Model, current}, current, true).decision, TransitionDecision::Ignored);
    DocumentSelection same_texture = current;
    same_texture.texture = "BODY";
    EXPECT_EQ(controller.Request({TransitionKind::Texture, same_texture}, current, true).decision, TransitionDecision::Ignored);
    current.textures.push_back("trim");
    EXPECT_EQ(controller.Request({TransitionKind::Texture, same_texture}, current, false).decision, TransitionDecision::Apply);
    DocumentSelection same_group = current;
    same_group.textures = {"TRIM", "BODY", "body"};
    EXPECT_EQ(controller.Request({TransitionKind::TextureGroup, same_group}, current, true).decision, TransitionDecision::Ignored);
    same_texture.model = "other.mdl";
    EXPECT_EQ(controller.Request({TransitionKind::Texture, same_texture}, current, false).decision, TransitionDecision::Apply);
    same_group.model = "other.mdl";
    EXPECT_EQ(controller.Request({TransitionKind::TextureGroup, same_group}, current, false).decision, TransitionDecision::Apply);
}

TEST(MaterialTransitionController, GroupEquivalenceRequiresBothSetsAndPreservesNewOrder)
{
    TransitionController controller;
    const DocumentSelection current{"current.mdl", "body", {"body", "trim"}};
    const DocumentSelection subset{"current.mdl", "body", {"body"}};
    const DocumentSelection superset{"current.mdl", "other", {"other", "trim", "body"}};

    const TransitionResult smaller = controller.Request({TransitionKind::TextureGroup, subset}, current, false);
    EXPECT_EQ(smaller.decision, TransitionDecision::Apply);
    const TransitionResult larger = controller.Request({TransitionKind::TextureGroup, superset}, current, false);
    ASSERT_EQ(larger.decision, TransitionDecision::Apply);
    ASSERT_TRUE(larger.request.has_value());
    EXPECT_EQ(larger.request->target.texture, "other");
    EXPECT_EQ(larger.request->target.textures, superset.textures);
}

TEST(MaterialTransitionController, RejectsInvalidGroupsAndAcceptsMaximumGroup)
{
    TransitionController controller;
    TransitionRequest request = Change(TransitionKind::TextureGroup);
    request.target.textures.clear();
    EXPECT_EQ(controller.Request(request, Current(), true).decision, TransitionDecision::Ignored);

    for (int index = 0; index < 65; index++)
    {
        request.target.textures.push_back("texture" + std::to_string(index));
    }

    EXPECT_EQ(controller.Request(request, Current(), true).decision, TransitionDecision::Ignored);
    request.target.textures.resize(64);
    EXPECT_EQ(controller.Request(request, Current(), false).decision, TransitionDecision::Apply);
    EXPECT_FALSE(controller.is_pending());
}

TEST(MaterialTransitionController, CancelConsumesIntentWithoutSaving)
{
    TransitionController controller;
    SaveService service;
    const TransitionResult confirmation = controller.Request(Change(TransitionKind::Close), Current(), true);
    const TransitionResult result = controller.Resolve(confirmation.token, TransitionChoice::Cancel, service);

    EXPECT_EQ(result.decision, TransitionDecision::Retain);
    EXPECT_FALSE(result.request.has_value());
    EXPECT_FALSE(controller.is_pending());
    EXPECT_EQ(service.calls, 0);
}

TEST(MaterialTransitionController, SuccessfulSaveAppliesOnce)
{
    TransitionController controller;
    SaveService service;
    const TransitionResult confirmation = controller.Request(Change(TransitionKind::Model), Current(), true);
    const TransitionResult result = controller.Resolve(confirmation.token, TransitionChoice::Save, service);

    EXPECT_EQ(result.decision, TransitionDecision::Apply);
    ASSERT_TRUE(result.request.has_value());
    EXPECT_EQ(result.request->target.model, "other.mdl");
    EXPECT_EQ(service.calls, 1);
    EXPECT_FALSE(controller.is_pending());
    EXPECT_EQ(controller.Resolve(confirmation.token, TransitionChoice::Save, service).decision, TransitionDecision::Ignored);
    EXPECT_EQ(service.calls, 1);
}

TEST(MaterialTransitionController, FailedSaveRetainsOwnedFailuresAndConsumesIntent)
{
    TransitionController controller;
    SaveService service;
    service.failures = {{"related.mdl", SaveFailureReason::SaveFailed, "write failure"}};
    const TransitionResult confirmation = controller.Request(Change(TransitionKind::Close), Current(), true);
    const TransitionResult result = controller.Resolve(confirmation.token, TransitionChoice::Save, service);
    service.failures.clear();

    EXPECT_EQ(result.decision, TransitionDecision::Retain);
    EXPECT_FALSE(result.request.has_value());
    ASSERT_EQ(result.failures.size(), 1u);
    EXPECT_EQ(result.failures[0].model, "related.mdl");
    EXPECT_EQ(result.failures[0].error, "write failure");
    EXPECT_EQ(result.failures[0].reason, SaveFailureReason::SaveFailed);
    EXPECT_FALSE(controller.is_pending());
    EXPECT_EQ(controller.Resolve(confirmation.token, TransitionChoice::Save, service).decision, TransitionDecision::Ignored);
    EXPECT_EQ(service.calls, 1);
}

TEST(MaterialTransitionController, UnavailableSaveServiceRetainsDocument)
{
    TransitionController controller;
    SaveService service;
    service.failures = {{"current.mdl", SaveFailureReason::EditorUnavailable, {}}};
    const TransitionResult confirmation = controller.Request(Change(TransitionKind::Texture), Current(), true);
    const TransitionResult result = controller.Resolve(confirmation.token, TransitionChoice::Save, service);

    EXPECT_EQ(result.decision, TransitionDecision::Retain);
    EXPECT_FALSE(result.request.has_value());
    ASSERT_EQ(result.failures.size(), 1u);
    EXPECT_EQ(result.failures[0].model, "current.mdl");
    EXPECT_EQ(result.failures[0].reason, SaveFailureReason::EditorUnavailable);
    EXPECT_EQ(service.calls, 1);
    EXPECT_FALSE(controller.is_pending());
}

TEST(MaterialTransitionController, ResetNeverReusesConfirmationIdentity)
{
    TransitionController controller;
    SaveService service;
    const TransitionResult old = controller.Request(Change(TransitionKind::Close), Current(), true);
    controller.Reset();
    EXPECT_FALSE(controller.is_pending());
    const TransitionResult next = controller.Request(Change(TransitionKind::Model), Current(), true);

    EXPECT_GT(next.token, old.token);
    EXPECT_EQ(controller.Resolve(old.token, TransitionChoice::Save, service).decision, TransitionDecision::Ignored);
    EXPECT_EQ(controller.Resolve(0, TransitionChoice::Discard, service).decision, TransitionDecision::Ignored);
    EXPECT_TRUE(controller.is_pending());
    EXPECT_EQ(service.calls, 0);
    EXPECT_EQ(controller.Resolve(next.token, TransitionChoice::Discard, service).decision, TransitionDecision::Apply);
}

TEST(MaterialTransitionController, ReentrantSaveCannotReplaceOrResolveIntent)
{
    TransitionController controller;
    SaveService service;
    const TransitionResult confirmation = controller.Request(Change(TransitionKind::Close), Current(), true);
    service.on_save = [&]() {
        EXPECT_TRUE(controller.is_pending());
        EXPECT_EQ(controller.Request(Change(TransitionKind::Model), Current(), false).decision, TransitionDecision::Ignored);
        EXPECT_EQ(controller.Resolve(confirmation.token, TransitionChoice::Save, service).decision, TransitionDecision::Ignored);
    };
    const TransitionResult result = controller.Resolve(confirmation.token, TransitionChoice::Save, service);

    ASSERT_EQ(result.decision, TransitionDecision::Apply);
    ASSERT_TRUE(result.request.has_value());
    EXPECT_EQ(result.request->kind, TransitionKind::Close);
    EXPECT_EQ(service.calls, 1);
    EXPECT_FALSE(controller.is_pending());
}

TEST(MaterialTransitionController, ResetDuringSaveInvalidatesIntentAndKeepsReentrantRequestsBlocked)
{
    TransitionController controller;
    SaveService service;
    const TransitionResult confirmation = controller.Request(Change(TransitionKind::Close), Current(), true);
    service.on_save = [&]() {
        controller.Reset();
        EXPECT_TRUE(controller.is_pending());
        EXPECT_EQ(controller.Request(Change(TransitionKind::Model), Current(), false).decision, TransitionDecision::Ignored);
        EXPECT_EQ(controller.Resolve(confirmation.token, TransitionChoice::Save, service).decision, TransitionDecision::Ignored);
    };
    const TransitionResult result = controller.Resolve(confirmation.token, TransitionChoice::Save, service);

    EXPECT_EQ(result.decision, TransitionDecision::Ignored);
    EXPECT_FALSE(result.request.has_value());
    EXPECT_EQ(service.calls, 1);
    EXPECT_FALSE(controller.is_pending());
    const TransitionResult next = controller.Request(Change(TransitionKind::Texture), Current(), true);
    EXPECT_EQ(next.decision, TransitionDecision::Confirm);
    EXPECT_GT(next.token, confirmation.token);
}
