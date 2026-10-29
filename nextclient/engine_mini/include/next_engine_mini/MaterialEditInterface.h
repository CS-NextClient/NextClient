#pragma once

namespace render
{
    // Returned text belongs to the engine; callers retaining it across mutations must copy it.

    enum class MaterialEditParamType
    {
        Scalar,
        Vec4,
        Texture
    };

    enum class MaterialEditParamWidget
    {
        Default,
        Color,
        Choice
    };

    enum class MaterialEditSource
    {
        None,
        Instance,
        ModelDocument,
        Implicit
    };

    struct MaterialEditParam
    {
        const char* name;

        // A leading # identifies a localization token. Label is nonempty; hint may be empty.
        const char* label;
        const char* hint;

        MaterialEditParamType type;
        MaterialEditParamWidget widget;

        // Instance scalars may be changed by the server within the declared range.
        bool instance;

        bool has_range;
        float min_value;
        float max_value;

        bool has_default;
        float default_scalar;
        float default_vec4[4];
        const char* default_texture;
    };

    class MaterialEditInterface
    {
    public:
        virtual ~MaterialEditInterface() = default;

        // Refreshes the list from available shaders and manifests.
        virtual int GetProgramCount() = 0;
        virtual const char* get_program_name(int index) = 0;

        virtual int GetParamCount(const char* program) = 0;
        virtual bool GetParam(const char* program, int index, MaterialEditParam* out) = 0;

        virtual int GetGroupCount(const char* program) = 0;
        virtual const char* GetGroupLabel(const char* program, int group) = 0;
        virtual int GetGroupParamCount(const char* program, int group) = 0;
        virtual int GetGroupParam(const char* program, int group, int index) = 0;

        // Rebuilds and sorts the available-model snapshot.
        virtual int get_model_count() = 0;
        virtual const char* GetModelName(int index) = 0;

        virtual int get_texture_count(const char* model) = 0;
        virtual const char* get_texture_name(const char* model, int index) = 0;

        // Entity index zero resolves the model resource without instance bindings.
        virtual MaterialEditSource GetMaterialSource(const char* model, const char* texture_name, int entity_index = 0) = 0;

        virtual int GetSkinFamilyCount(const char* model) = 0;
        virtual int GetBodyCount(const char* model) = 0;
        virtual bool GetBodyName(const char* model, int body, char* out, int out_len) = 0;

        // Copies the resolved source into the session and enables the editable material layer.
        virtual bool Begin(const char* model, const char* texture_name, int entity_index = 0) = 0;
        virtual void End() = 0;
        virtual bool is_editing() = 0;

        virtual int get_edited_entity_index() = 0;
        virtual const char* get_edited_model() = 0;
        virtual const char* get_edited_texture() = 0;

        virtual const char* get_edited_program() = 0;

        virtual bool SetEditedProgram(const char* program) = 0;

        virtual bool get_scalar(const char* name, float* out) = 0;
        virtual bool get_vec4(const char* name, float* out) = 0;
        virtual const char* get_texture(const char* name) = 0;

        virtual bool SetScalar(const char* name, float value) = 0;
        virtual bool SetVec4(const char* name, const float* value) = 0;
        virtual bool SetTexture(const char* name, const char* path) = 0;
        virtual bool ClearParam(const char* name) = 0;

        virtual const char* Validate() = 0;

        // Updates the selected texture entries in the destination model document.
        virtual bool Save(const char* model, char* error_out, int error_out_len) = 0;

        // Builds a preset entry from the current session.
        virtual const char* GetPresetStub() = 0;

        virtual void Reload() = 0;

        virtual bool BeginGroup(const char* model, const char* const* textures, int count, int entity_index = 0) = 0;
        virtual int get_selection_count() = 0;
        virtual const char* get_selected_texture(int index) = 0;
        virtual bool has_mixed_programs() = 0;
        virtual bool is_param_mixed(const char* name) = 0;

        virtual int GetChoiceCount(const char* program, const char* param) = 0;
        virtual const char* GetChoiceLabel(const char* program, const char* param, int index) = 0;

        // The snapshot preserves this parameter independently for each selected texture.
        virtual bool BeginParamPreview(const char* name) = 0;
        virtual void EndParamPreview(bool accept) = 0;
    };
} // namespace render
