#include "GameUi/MaterialEditor/MaterialSaveWorkflow.h"

#include <next_engine_mini/MaterialEditInterface.h>
#include <tier1/strtools.h>

namespace material_editor
{
    namespace
    {
        constexpr int kSaveErrorSize = 512;

        class EngineSaveBackend : public SaveBackend
        {
        private:
            render::MaterialEditInterface& editor_;
            ScenePreviewInterface& preview_;

        public:
            EngineSaveBackend(render::MaterialEditInterface& editor, ScenePreviewInterface& preview) :
                editor_(editor),
                preview_(preview)
            {}

            PreviewModelStatus PrepareModel(const std::string& model) override
            {
                PreviewModelInfo info{};

                return preview_.PrepareModel(model.c_str(), &info);
            }

            int get_texture_count(const std::string& model) override
            {
                return editor_.get_texture_count(model.c_str());
            }

            const char* get_texture_name(const std::string& model, int index) override
            {
                return editor_.get_texture_name(model.c_str(), index);
            }

            bool Save(const std::string& model, char* error, int error_size) override
            {
                return editor_.Save(model.c_str(), error, error_size);
            }
        };

        bool MatchSelectedTextures(SaveBackend& backend, const std::string& target, const std::vector<std::string>& selected_textures)
        {
            for (const std::string& selected : selected_textures)
            {
                bool found = false;

                for (int texture = 0; texture < backend.get_texture_count(target); texture++)
                {
                    if (V_stricmp(backend.get_texture_name(target, texture), selected.c_str()) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return false;
                }
            }

            return true;
        }
    } // namespace

    std::vector<SaveFailure> SaveMaterialCopies(
        SaveBackend& backend,
        std::span<const std::string> targets,
        std::span<const std::string> selected_textures
    )
    {
        const std::vector<std::string> target_snapshot(targets.begin(), targets.end());
        const std::vector<std::string> selected_snapshot(selected_textures.begin(), selected_textures.end());
        std::vector<SaveFailure> failures;

        for (const std::string& target : target_snapshot)
        {
            const PreviewModelStatus status = backend.PrepareModel(target);

            if (status != PreviewModelStatus::Ready)
            {
                const SaveFailureReason reason = status == PreviewModelStatus::CapacityExceeded ? SaveFailureReason::ModelCapacity
                                                 : status == PreviewModelStatus::RendererUnavailable
                                                     ? SaveFailureReason::RendererUnavailable
                                                     : SaveFailureReason::ModelLoadFailed;
                failures.push_back({target, reason, {}});
                continue;
            }

            if (!MatchSelectedTextures(backend, target, selected_snapshot))
            {
                failures.push_back({target, SaveFailureReason::NoMatchingTexture, {}});
                continue;
            }

            char error[kSaveErrorSize]{};

            if (!backend.Save(target, error, sizeof(error)))
            {
                failures.push_back({target, SaveFailureReason::SaveFailed, error});
            }
        }

        return failures;
    }

    std::vector<SaveFailure> SaveMaterialCopies(
        render::MaterialEditInterface& editor,
        ScenePreviewInterface& preview,
        std::span<const std::string> targets,
        std::span<const std::string> selected_textures
    )
    {
        EngineSaveBackend backend(editor, preview);

        return SaveMaterialCopies(backend, targets, selected_textures);
    }
} // namespace material_editor
