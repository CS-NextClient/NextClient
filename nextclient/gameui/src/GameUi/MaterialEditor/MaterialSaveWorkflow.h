#pragma once

#include <span>
#include <string>
#include <vector>

#include <next_engine_mini/ScenePreviewInterface.h>

namespace render
{
    class MaterialEditInterface;
} // namespace render

namespace material_editor
{
    enum class SaveFailureReason
    {
        ModelCapacity,
        RendererUnavailable,
        ModelLoadFailed,
        NoMatchingTexture,
        SaveFailed,
        EditorUnavailable
    };

    struct SaveFailure
    {
        std::string model;
        SaveFailureReason reason{};
        std::string error;
    };

    class SaveBackend
    {
    public:
        virtual ~SaveBackend() = default;
        virtual PreviewModelStatus PrepareModel(const std::string& model) = 0;
        virtual int get_texture_count(const std::string& model) = 0;
        // Returned storage is valid only until the next backend call.
        virtual const char* get_texture_name(const std::string& model, int index) = 0;
        virtual bool Save(const std::string& model, char* error, int error_size) = 0;
    };

    std::vector<SaveFailure> SaveMaterialCopies(
        SaveBackend& backend,
        std::span<const std::string> targets,
        std::span<const std::string> selected_textures
    );
    std::vector<SaveFailure> SaveMaterialCopies(
        render::MaterialEditInterface& editor,
        ScenePreviewInterface& preview,
        std::span<const std::string> targets,
        std::span<const std::string> selected_textures
    );
} // namespace material_editor
