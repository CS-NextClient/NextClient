#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <next_engine_mini/MaterialEditInterface.h>
#include <ncl_material/ProgramManifest.h>
#include <ncl_material/MaterialResolve.h>

#include "graphics/render/material/MaterialEditSession.h"

namespace render
{
    class StudioModelCache;
    class ServerMaterials;
    class ShaderManager;
    class MaterialManifestCatalog;
    class MaterialDocumentStore;
    class MaterialTextureCache;

    class MaterialEditor final : public MaterialEditInterface
    {
    private:
        struct WearGenerationJob;

    private:
        StudioModelCache& models_;
        ServerMaterials& server_materials_;
        ShaderManager& shaders_;
        MaterialManifestCatalog& manifests_;
        MaterialDocumentStore& documents_;
        MaterialTextureCache& textures_;
        std::function<void()> image_retirement_barrier_;
        MaterialEditSession session_;
        uint64_t edit_revision_{};
        uint64_t wear_request_revision_{};
        uint64_t generated_sequence_{};
        std::vector<std::string> generated_images_;
        std::shared_ptr<WearGenerationJob> wear_job_;
        std::shared_ptr<WearGenerationJob> pending_wear_job_;
        MaterialWearStatus wear_status_{};
        std::string wear_error_;

        // Each getter retains separate storage for simultaneously borrowed results.
        std::vector<std::string> programs_;
        std::string validation_text_;
        std::string preset_stub_;

    public:
        MaterialEditor(
            StudioModelCache& models,
            ServerMaterials& server_materials,
            ShaderManager& shaders,
            MaterialManifestCatalog& manifests,
            MaterialDocumentStore& documents,
            MaterialTextureCache& textures
        );
        ~MaterialEditor() override;
        MaterialEditor(const MaterialEditor&) = delete;
        MaterialEditor& operator=(const MaterialEditor&) = delete;
        int GetProgramCount() override;
        const char* get_program_name(int index) override;
        int GetParamCount(const char* program) override;
        bool GetParam(const char* program, int index, MaterialEditParam* out) override;
        int GetChoiceCount(const char* program, const char* param) override;
        const char* GetChoiceLabel(const char* program, const char* param, int index) override;
        int GetGroupCount(const char* program) override;
        const char* GetGroupLabel(const char* program, int group) override;
        int GetGroupParamCount(const char* program, int group) override;
        int GetGroupParam(const char* program, int group, int index) override;
        int get_model_count() override;
        const char* GetModelName(int index) override;
        int get_texture_count(const char* model) override;
        const char* get_texture_name(const char* model, int index) override;
        MaterialEditSource GetMaterialSource(const char* model, const char* texture_name, int entity_index = 0) override;
        int GetSkinFamilyCount(const char* model) override;
        int GetBodyCount(const char* model) override;
        bool GetBodyName(const char* model, int body, char* out, int out_len) override;
        bool Begin(const char* model, const char* texture_name, int entity_index = 0) override;
        bool BeginGroup(const char* model, const char* const* textures, int count, int entity_index = 0) override;
        int get_selection_count() override;
        const char* get_selected_texture(int index) override;
        bool has_mixed_programs() override;
        bool is_param_mixed(const char* name) override;
        void End() override;
        bool is_editing() override;
        int get_edited_entity_index() override;
        const char* get_edited_model() override;
        const char* get_edited_texture() override;
        const char* get_edited_program() override;
        bool SetEditedProgram(const char* program) override;
        bool get_scalar(const char* name, float* out) override;
        bool get_vec4(const char* name, float* out) override;
        const char* get_texture(const char* name) override;
        bool BeginParamPreview(const char* name) override;
        void EndParamPreview(bool accept) override;
        bool SetScalar(const char* name, float value) override;
        bool SetVec4(const char* name, const float* value) override;
        bool SetTexture(const char* name, const char* path) override;
        bool ClearParam(const char* name) override;
        const char* Validate() override;
        bool Save(const char* model, char* error_out, int error_out_len) override;
        const char* GetPresetStub() override;
        void Reload() override;
        bool BeginWearGeneration(int skin_family, float scratch_strength, char* error_out, int error_out_len) override;
        MaterialWearStatus PollWearGeneration(char* error_out, int error_out_len) override;
        void CancelWearGeneration() override;
        void SetImageRetirementBarrier(std::function<void()> barrier);
        bool BeginGroup(const char* model, const char* const* textures, int count, int entity_index, bool (*network_session)());
        MaterialEditSource GetMaterialSource(const char* model, const char* texture_name, int entity_index, bool (*network_session)());

        const MaterialEditSession& get_session() const
        {
            return session_;
        }

    private:
        const ncl_material::ProgramManifest* ManifestOf(const char* program);
        const ncl_material::ParamGroup* GroupOf(const char* program, int group);
        const ncl_material::ParamSchema* ChoiceOf(const char* program, const char* param);
        const ncl_material::Material* DrawnMaterial(
            const char* model,
            const char* texture_name,
            int entity_index,
            ncl_material::MaterialOrigin* origin,
            bool (*network_session)()
        );
        void RebuildPrograms();
        ncl_material::Param* FindParam(const char* name);
        const ncl_material::ParamSchema* SchemaOf(const char* name);
        void SetParam(const char* name, const ncl_material::ParamValue& value);
        bool IsWearJobCurrent(const WearGenerationJob& job) const;
        bool StartWearJob(std::shared_ptr<WearGenerationJob> job);
        bool PublishWearImages(WearGenerationJob& job, std::string& error);
        void DiscardGeneratedImages();
        void RemoveGeneratedImages(const std::vector<std::string>& images);
        bool SaveTo(const std::string& model, std::string& error);
        static bool ReferencesImage(const std::vector<ncl_material::MaterialEntry>& entries, const std::string& image);
        static MaterialEditParamType TypeOf(ncl_material::ParamType type);
    };
} // namespace render
