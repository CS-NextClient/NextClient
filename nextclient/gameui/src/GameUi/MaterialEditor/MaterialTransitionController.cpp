#include "GameUi/MaterialEditor/MaterialTransitionController.h"

#include <algorithm>
#include <utility>

#include <ncl_material/MaterialDoc.h>

namespace material_editor
{
    namespace
    {
        class ResolvingScope
        {
        private:
            bool& resolving_;

        public:
            explicit ResolvingScope(bool& resolving) :
                resolving_(resolving)
            {
                resolving_ = true;
            }

            ~ResolvingScope()
            {
                resolving_ = false;
            }
        };

        bool ContainsTextures(const std::vector<std::string>& textures, const std::vector<std::string>& selected)
        {
            return std::all_of(selected.begin(), selected.end(), [&](const std::string& name) {
                return std::any_of(textures.begin(), textures.end(), [&](const std::string& texture) {
                    return ncl_material::EqualsIgnoreCaseAscii(texture, name);
                });
            });
        }

        bool IsSameSelection(const TransitionRequest& request, const DocumentSelection& current)
        {
            if (request.kind == TransitionKind::Close || request.target.model != current.model)
            {
                return false;
            }

            if (request.kind == TransitionKind::Model)
            {
                return true;
            }

            if (request.kind == TransitionKind::Texture)
            {
                return current.textures.size() <= 1 && ncl_material::EqualsIgnoreCaseAscii(request.target.texture, current.texture);
            }

            return ContainsTextures(current.textures, request.target.textures) &&
                   ContainsTextures(request.target.textures, current.textures);
        }
    } // namespace

    TransitionResult TransitionController::Request(TransitionRequest request, const DocumentSelection& current, bool material_dirty)
    {
        if (is_pending() ||
            (request.kind == TransitionKind::TextureGroup &&
             (request.target.textures.empty() || request.target.textures.size() > ncl_material::kMaxEntries)) ||
            IsSameSelection(request, current))
        {
            return {};
        }

        if (!material_dirty)
        {
            return {TransitionDecision::Apply, 0, std::move(request), {}};
        }

        pending_ = std::move(request);
        pending_token_ = ++next_token_;

        return {TransitionDecision::Confirm, pending_token_, std::nullopt, {}};
    }

    TransitionResult TransitionController::Resolve(TransitionToken_t token, TransitionChoice choice, TransitionSaveService& save_service)
    {
        if (resolving_ || !pending_.has_value() || token != pending_token_)
        {
            return {};
        }

        ResolvingScope resolving(resolving_);
        TransitionRequest request = std::move(*pending_);
        pending_.reset();
        std::vector<SaveFailure> failures;

        if (choice == TransitionChoice::Save)
        {
            failures = save_service.Save();
        }

        if (pending_token_ != token)
        {
            return {};
        }

        pending_token_ = 0;

        if (choice == TransitionChoice::Cancel || !failures.empty())
        {
            return {TransitionDecision::Retain, token, std::nullopt, std::move(failures)};
        }

        return {TransitionDecision::Apply, token, std::move(request), {}};
    }

    void TransitionController::Reset()
    {
        // A save callback can invalidate its intent without reopening admission before Resolve returns.
        pending_.reset();
        pending_token_ = 0;
    }

    bool TransitionController::is_pending() const
    {
        return resolving_ || pending_.has_value();
    }
} // namespace material_editor
