#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <span>
#include <string>
#include <string_view>

#include <ncl_material/MaterialPreset.h>
#include <ncl_material/ProgramManifest.h>

namespace render
{
    struct ServerMaterialBindingOwner
    {
        int entity_index{};
        uint16_t sync_entity_id{};
    };

    struct ServerMaterialBinding
    {
        std::string preset;
        std::string name;
    };

    struct ServerMaterialParam
    {
        std::string key;
        float value{};
    };

    // Input spans are copied; invalid values reject the snapshot before owner mutation.
    // An unavailable rendered index accepts the snapshot for later migration.
    bool ServerMaterials_ReplaceOwner(
        const ServerMaterialBindingOwner& owner,
        std::span<const ServerMaterialBinding> bindings,
        std::span<const ServerMaterialParam> params
    );

    // Complete replacement starts from preset defaults and synchronized effect policy.
    bool ServerMaterials_ReplaceOwnerParams(const ServerMaterialBindingOwner& owner, std::span<const ServerMaterialParam> params);
    void ServerMaterials_RemoveOwner(const ServerMaterialBindingOwner& owner);

    // Maximum distinct texture bindings admitted for one rendered entity.
    constexpr size_t kMaxBindingsPerEntity = 16;

    // What a server has dressed entities in.
    //
    // Nothing announces a preset. A binding names the file it comes from, so the file is
    // read the first time a binding needs it and kept afterwards; a server that dresses
    // nothing costs nothing. The delivery of the file itself is the ordinary download.
    void ServerMaterials_Clear();

    struct EntityMaterialQuery
    {
        int entity_index{};
        std::string_view texture_name;
    };

    // Borrowed until material mutation. Instance precedence is resolved per entity here;
    // model-level material resolution cannot select an entity's binding.
    const ncl_material::Material* ServerMaterials_BindingFor(const EntityMaterialQuery& query);
    void ServerMaterials_ApplyEffectPolicy(ncl_material::Material& material, bool server_bound);
    bool ServerMaterials_AnyBindingMatches(int entity_index, bool (*matches)(const ncl_material::Material&));

    // Replaces the console binding for the material's texture on this entity.
    void ServerMaterials_SetBinding(int entity_index, const char* preset, const char* name);

    // Console bindings take precedence; higher synchronized owner IDs win texture conflicts.
    void ServerMaterials_SetBinding(const ServerMaterialBindingOwner& owner, const char* preset, const char* name);
    void ServerMaterials_RemoveBinding(int entity_index, const char* preset, const char* name);
    void ServerMaterials_ClearBindings(int entity_index);

    // One finite instance value in the parameter's declared units. The key is the
    // parameter's name, optionally prefixed by the texture it belongs to:
    // "uWear" writes over every material the entity wears, "glock_slide.bmp/uWear" over one.
    //
    // False when nothing took it: the entity wears nothing matching, or the program does
    // not declare that name as an instance scalar.
    bool ServerMaterials_SetParam(int entity_index, const char* key, float value);

    // Writes only the bindings belonging to this synchronized owner.
    bool ServerMaterials_SetParam(const ServerMaterialBindingOwner& owner, const char* key, float value);

    // Diagnostic only, and a no-op unless r_mat_entities armed it: a binding has to be
    // aimed at an entity index, and without a server there is nothing to say which index
    // is which.
    void ServerMaterials_NoteEntity(int entity_index, std::string_view model);

    // Bumped whenever a preset is loaded or dropped, so caches that resolved against one
    // can tell without being told.
    unsigned int ServerMaterials_Generation();

    // Split because the engine refuses Cmd_AddCommand once the host is initialized.
    void ServerMaterials_RegisterCommands();
    class MaterialDocumentStore;
    class MaterialManifestCatalog;

    class ServerMaterials
    {
    private:
        struct Binding
        {
            // Where it came from, kept so the same pair can be taken off again and so a
            // reloaded preset can be resolved against.
            std::string preset;
            std::string name;

            // Which texture of the model this dresses, copied from the preset because that is
            // what every lookup asks about.
            std::string texture_name;

            // The preset's material with this entity's instance values written over it.
            // Materialized rather than applied at draw time because the preset's copy is
            // shared by every entity wearing it and must not be written to.
            ncl_material::Material resolved;

            // Presets remain immutable until the binding cache is cleared first.
            const ncl_material::Material* preset_material{};
        };

        struct OwnerBindings
        {
            std::vector<ServerMaterialBinding> requested;
            std::vector<ServerMaterialParam> params;
            std::vector<Binding> candidates;
        };

        struct EntityBindings
        {
            std::vector<Binding> console;
            std::map<uint16_t, OwnerBindings, std::greater<uint16_t>> owners;
            std::vector<std::string> admitted_textures;

            // Whether the renderer has ever asked this index about a texture. A binding aimed at
            // an index nothing draws is the one failure of this path with nothing to see: no
            // message, no wrong color, just the model as it always was.
            bool drawn{};
        };

        struct LoadedPreset
        {
            std::string path;
            ncl_material::MaterialPreset preset;

            // A file that could not be read or parsed is remembered as a failure, so a binding
            // that names it does not have the client read it again on every entity.
            bool valid{};
        };

        MaterialDocumentStore& documents_;
        MaterialManifestCatalog& manifests_;
        std::unordered_map<int, EntityBindings> bindings_;
        std::vector<std::unique_ptr<LoadedPreset>> presets_;
        unsigned int generation_{};
        int note_budget_{};
        std::vector<int> noted_entities_;

    public:
        ServerMaterials(MaterialDocumentStore& documents, MaterialManifestCatalog& manifests);
        ServerMaterials(const ServerMaterials&) = delete;
        ServerMaterials& operator=(const ServerMaterials&) = delete;
        void PreloadPreset(const char* path);
        bool ReplaceOwner(
            const ServerMaterialBindingOwner& owner,
            std::span<const ServerMaterialBinding> bindings,
            std::span<const ServerMaterialParam> params
        );
        bool ReplaceOwnerParams(const ServerMaterialBindingOwner& owner, std::span<const ServerMaterialParam> params);
        void RemoveOwner(const ServerMaterialBindingOwner& owner);
        void Clear();
        void SetBinding(int entity_index, const char* preset, const char* name, bool network_session);
        void SetBinding(const ServerMaterialBindingOwner& owner, const char* preset, const char* name);
        void RemoveBinding(int entity_index, const char* preset, const char* name, bool network_session);
        void ClearBindings(int entity_index);
        bool SetParam(int entity_index, const char* key, float value, bool network_session);
        bool SetParam(const ServerMaterialBindingOwner& owner, const char* key, float value);
        void NoteEntity(int entity_index, std::string_view model);
        const ncl_material::Material* BindingFor(const EntityMaterialQuery& query, bool network_session);
        bool AnyBindingMatches(int entity_index, bool (*matches)(void*, const ncl_material::Material&), bool network_session, void* user);
        void ReportPresets();
        void ReportBindings();
        void ArmDiagnostics();
        unsigned int get_generation() const
        {
            return generation_;
        }

    private:
        bool HasPresetExtension(std::string_view path);
        size_t DropMaterialsWithoutAMatchingProgram(ncl_material::MaterialPreset& preset, const char* path);
        const LoadedPreset* FindLoaded(std::string_view path);
        const LoadedPreset* LoadPreset(const char* path);
        EntityBindings* FindEntity(int entity_index);
        bool IsValidEntityIndex(int entity_index);
        bool IsAdmitted(const EntityBindings& bindings, std::string_view texture);
        void AdmitTexture(EntityBindings& bindings, const std::string& texture);
        void ReconcileAdmission(EntityBindings& bindings);
        bool ResolveBinding(const char* preset_path, const char* name, bool synchronized, Binding& binding);
        void StoreCandidate(OwnerBindings& owner, Binding binding);
        void Bind(int entity_index, std::optional<uint16_t> sync_owner, const char* preset_path, const char* name);
        void Unbind(int entity_index, const char* preset_path, const char* name);
        const ncl_material::ParamSchema* InstanceSchema(const Binding& binding, std::string_view key, std::string_view& name);
        bool IsWriteValid(const Binding& binding, std::string_view key, float value);
        bool AreParamsValid(std::span<const Binding> bindings, std::span<const ServerMaterialParam> params);
        bool WriteParam(Binding& binding, std::string_view key, float scalar);
        void ApplyParams(OwnerBindings& owner);
        bool SetParam(int entity_index, std::optional<uint16_t> sync_owner, const char* key, float value);
    };

} // namespace render
