#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "GameUi/MaterialEditor/MaterialSaveWorkflow.h"

namespace material_editor
{
    enum class TransitionKind
    {
        Close,
        Model,
        Texture,
        TextureGroup
    };

    enum class TransitionChoice
    {
        Save,
        Discard,
        Cancel
    };

    enum class TransitionDecision
    {
        Ignored,
        Confirm,
        Apply,
        Retain
    };
    using TransitionToken_t = uint64_t;

    struct DocumentSelection
    {
        std::string model;
        std::string texture;
        std::vector<std::string> textures;
    };

    struct TransitionRequest
    {
        TransitionKind kind{};
        DocumentSelection target;
    };

    struct TransitionResult
    {
        TransitionDecision decision{};
        TransitionToken_t token{};
        std::optional<TransitionRequest> request;
        std::vector<SaveFailure> failures;
    };

    class TransitionSaveService
    {
    public:
        virtual ~TransitionSaveService() = default;
        virtual std::vector<SaveFailure> Save() = 0;
    };

    class TransitionController
    {
    private:
        std::optional<TransitionRequest> pending_;
        TransitionToken_t next_token_{};
        TransitionToken_t pending_token_{};
        bool resolving_{};

    public:
        TransitionResult Request(TransitionRequest request, const DocumentSelection& current, bool material_dirty);
        TransitionResult Resolve(TransitionToken_t token, TransitionChoice choice, TransitionSaveService& save_service);
        void Reset();
        bool is_pending() const;
    };
} // namespace material_editor
