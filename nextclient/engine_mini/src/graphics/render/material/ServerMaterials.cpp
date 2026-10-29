#include "graphics/render/material/ServerMaterials.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "graphics/render/material/MaterialManifestCatalog.h"
#include "graphics/render/material/StudioProgramContract.h"
#include "graphics/render/material/MaterialDocumentStore.h"

namespace render
{
    namespace
    {
        constexpr const char* kPresetExtension = ".mat";
        constexpr size_t kMaxPresetPathLength = 128;

        // The parser holds the text, taocpp copies it and the DOM is a third copy, so the file
        // costs several times its own size in a 32-bit address space.
        constexpr unsigned int kMaxPresetBytes = 1u << 20;

        // A server naming a new file for every binding would otherwise have the client keep
        // every one of them for the session.
        constexpr size_t kMaxPresets = 32;

        // The engine's live edict ceiling, not the SDK's MAX_EDICTS: an index at or above this
        // cannot name an entity this client will ever draw.
        constexpr int kMaxBoundEntities = 4096;

        // Separates the texture from the parameter in an instance key. Safe as a separator
        // because a studio texture name is a file name without a path.
        constexpr char kParamScopeSeparator = '/';

    } // namespace

    ServerMaterials::ServerMaterials(MaterialDocumentStore& documents, MaterialManifestCatalog& manifests) :
        documents_(documents),
        manifests_(manifests)
    {}

    bool ServerMaterials::HasPresetExtension(std::string_view path)
    {
        std::string_view suffix(kPresetExtension);

        return path.size() > suffix.size() && ncl_material::EqualsIgnoreCaseAscii(path.substr(path.size() - suffix.size()), suffix);
    }

    // Materials are dropped one by one rather than rejecting the preset, unlike a budget
    // overflow: a program the client does not have is a version difference, and an older
    // client should still draw the materials it does understand instead of none of them.
    size_t ServerMaterials::DropMaterialsWithoutAMatchingProgram(ncl_material::MaterialPreset& preset, const char* path)
    {
        size_t dropped = 0;

        for (size_t i = preset.materials.size(); i > 0; i--)
        {
            ncl_material::PresetMaterial& material = preset.materials[i - 1];
            const ncl_material::ProgramManifest* manifest = manifests_.Get(material.material.program);

            std::string reason;

            if (manifest == nullptr)
            {
                reason = "no program \"" + material.material.program + "\"";
            }
            else
            {
                ncl_utils::Result<> valid = ncl_material::Validate(material.material, *manifest);

                if (valid.has_error())
                {
                    reason = valid.error_str();
                }
                else
                {
                    ncl_material::ApplyDefaults(material.material, *manifest);
                }
            }

            if (reason.empty())
            {
                continue;
            }

            documents_.Report("materials: %s: \"%s\" dropped: %s\n", path, material.name.c_str(), reason.c_str());

            preset.materials.erase(preset.materials.begin() + (i - 1));
            dropped++;
        }

        return dropped;
    }

    const ServerMaterials::LoadedPreset* ServerMaterials::FindLoaded(std::string_view path)
    {
        for (const std::unique_ptr<LoadedPreset>& loaded : presets_)
        {
            if (ncl_material::EqualsIgnoreCaseAscii(loaded->path, path))
            {
                return loaded.get();
            }
        }

        return nullptr;
    }

    // Read on first use rather than on an announcement: the binding that needs it names it,
    // and a preset nobody binds is a file nobody reads.
    void ServerMaterials::PreloadPreset(const char* path)
    {
        LoadPreset(path);
    }

    const ServerMaterials::LoadedPreset* ServerMaterials::LoadPreset(const char* path)
    {
        if (path == nullptr || *path == '\0')
        {
            return nullptr;
        }

        if (const LoadedPreset* loaded = FindLoaded(path))
        {
            return loaded;
        }

        if (presets_.size() >= kMaxPresets)
        {
            documents_.Report("materials: %s not loaded, already holding %u presets\n", path, static_cast<unsigned>(presets_.size()));

            return nullptr;
        }

        std::unique_ptr<LoadedPreset> loaded = std::make_unique<LoadedPreset>();
        loaded->path = path;

        std::string json;

        if (!ncl_material::IsAdmissibleRelativePath(path, kMaxPresetPathLength) || !HasPresetExtension(path))
        {
            documents_.Report("materials: \"%s\" is not an admissible preset path\n", path);
        }
        else if (!documents_.Read(path, json, kMaxPresetBytes))
        {
            documents_.Report("materials: %s could not be read\n", path);
        }
        else
        {
            ncl_utils::ResultT<ncl_material::MaterialPreset> parsed = ncl_material::ParseMaterialPreset(json, ncl_material::PresetBudget{});

            if (parsed.has_error())
            {
                documents_.Report("materials: %s: %s\n", path, parsed.error_str().c_str());
            }
            else
            {
                loaded->preset = *parsed;
                loaded->valid = true;

                size_t dropped = DropMaterialsWithoutAMatchingProgram(loaded->preset, path);

                documents_.Report("materials: %s: %u materials", path, static_cast<unsigned>(loaded->preset.materials.size()));

                if (dropped != 0)
                {
                    documents_.Report(", %u dropped", static_cast<unsigned>(dropped));
                }

                documents_.Report("\n");
            }
        }

        presets_.push_back(std::move(loaded));
        generation_++;

        return presets_.back().get();
    }

    ServerMaterials::EntityBindings* ServerMaterials::FindEntity(int entity_index)
    {
        std::unordered_map<int, EntityBindings>::iterator it = bindings_.find(entity_index);

        return it != bindings_.end() ? &it->second : nullptr;
    }

    bool ServerMaterials::IsValidEntityIndex(int entity_index)
    {
        return entity_index > 0 && entity_index < kMaxBoundEntities;
    }

    bool ServerMaterials::IsAdmitted(const EntityBindings& bindings, std::string_view texture)
    {
        return std::any_of(bindings.admitted_textures.begin(), bindings.admitted_textures.end(), [&](const std::string& admitted) {
            return ncl_material::EqualsIgnoreCaseAscii(admitted, texture);
        });
    }

    void ServerMaterials::AdmitTexture(EntityBindings& bindings, const std::string& texture)
    {
        if (bindings.admitted_textures.size() < kMaxBindingsPerEntity && !IsAdmitted(bindings, texture))
        {
            bindings.admitted_textures.push_back(texture);
        }
    }

    void ServerMaterials::ReconcileAdmission(EntityBindings& bindings)
    {
        bindings.admitted_textures.clear();

        for (const Binding& binding : bindings.console)
        {
            AdmitTexture(bindings, binding.texture_name);
        }

        // Console textures reserve slots before descending owner IDs and request order.
        for (const auto& [owner_id, owner] : bindings.owners)
        {
            for (const Binding& binding : owner.candidates)
            {
                AdmitTexture(bindings, binding.texture_name);
            }
        }
    }

    bool ServerMaterials::ResolveBinding(const char* preset_path, const char* name, bool synchronized, Binding& binding)
    {
        if (name == nullptr || *name == '\0')
        {
            return false;
        }

        const LoadedPreset* loaded = LoadPreset(preset_path);

        if (loaded == nullptr || !loaded->valid)
        {
            return false;
        }

        const ncl_material::PresetMaterial* material = loaded->preset.FindByName(name);

        if (material == nullptr)
        {
            documents_.Report("materials: %s has no material \"%s\"\n", loaded->path.c_str(), name);
            return false;
        }

        binding.preset = loaded->path;
        binding.name = material->name;
        binding.texture_name = material->texture_name;
        binding.preset_material = &material->material;
        binding.resolved = material->material;

        if (synchronized)
        {
            StudioContract_DisableRestrictedEffects(binding.resolved, manifests_.Get(binding.resolved.program));
        }

        return true;
    }

    void ServerMaterials::StoreCandidate(OwnerBindings& owner, Binding binding)
    {
        // Reassignment changes the value, while the first valid request reserves its texture's order.
        for (Binding& candidate : owner.candidates)
        {
            if (ncl_material::EqualsIgnoreCaseAscii(candidate.texture_name, binding.texture_name))
            {
                candidate = std::move(binding);
                return;
            }
        }

        owner.candidates.push_back(std::move(binding));
    }

    void ServerMaterials::Bind(int entity_index, std::optional<uint16_t> sync_owner, const char* preset_path, const char* name)
    {
        if (!IsValidEntityIndex(entity_index) || preset_path == nullptr || name == nullptr)
        {
            return;
        }

        const ServerMaterialBinding request{preset_path, name};
        Binding binding;

        if (!ResolveBinding(request.preset.c_str(), request.name.c_str(), sync_owner.has_value(), binding))
        {
            return;
        }

        EntityBindings& bindings = bindings_[entity_index];

        if (sync_owner.has_value())
        {
            OwnerBindings& owner = bindings.owners[*sync_owner];
            owner.requested.push_back(request);
            StoreCandidate(owner, std::move(binding));
        }
        else
        {
            if (!IsAdmitted(bindings, binding.texture_name) && bindings.admitted_textures.size() >= kMaxBindingsPerEntity)
            {
                documents_.Report(
                    "materials: entity %d already wears %u materials\n", entity_index, static_cast<unsigned>(kMaxBindingsPerEntity)
                );
                return;
            }

            std::erase_if(bindings.console, [&](const Binding& previous) {
                return ncl_material::EqualsIgnoreCaseAscii(previous.texture_name, binding.texture_name);
            });
            bindings.console.push_back(std::move(binding));
        }

        ReconcileAdmission(bindings);
    }

    void ServerMaterials::Unbind(int entity_index, const char* preset_path, const char* name)
    {
        EntityBindings* bindings = FindEntity(entity_index);

        if (bindings == nullptr || preset_path == nullptr || name == nullptr)
        {
            return;
        }

        const std::string preset_snapshot(preset_path);
        const std::string name_snapshot(name);
        auto matches = [&](const Binding& binding) {
            return ncl_material::EqualsIgnoreCaseAscii(binding.preset, preset_snapshot) &&
                   ncl_material::EqualsIgnoreCaseAscii(binding.name, name_snapshot);
        };

        std::erase_if(bindings->console, matches);

        for (auto& [owner_id, owner] : bindings->owners)
        {
            std::erase_if(owner.candidates, matches);
            std::erase_if(owner.requested, [&](const ServerMaterialBinding& request) {
                return ncl_material::EqualsIgnoreCaseAscii(request.preset, preset_snapshot) &&
                       ncl_material::EqualsIgnoreCaseAscii(request.name, name_snapshot);
            });
        }

        ReconcileAdmission(*bindings);
    }

    const ncl_material::ParamSchema* ServerMaterials::InstanceSchema(const Binding& binding, std::string_view key, std::string_view& name)
    {
        const size_t separator = key.find(kParamScopeSeparator);
        const std::string_view texture = separator != std::string_view::npos ? key.substr(0, separator) : std::string_view();
        name = separator != std::string_view::npos ? key.substr(separator + 1) : key;

        if (name.empty() || (!texture.empty() && !ncl_material::EqualsIgnoreCaseAscii(binding.texture_name, texture)))
        {
            return nullptr;
        }

        const ncl_material::ProgramManifest* manifest = manifests_.Get(binding.resolved.program);
        const ncl_material::ParamSchema* schema = manifest != nullptr ? manifest->FindParam(name) : nullptr;

        return schema != nullptr && schema->instance && schema->type == ncl_material::ParamType::Scalar ? schema : nullptr;
    }

    bool ServerMaterials::IsWriteValid(const Binding& binding, std::string_view key, float value)
    {
        std::string_view name;
        const ncl_material::ParamSchema* schema = InstanceSchema(binding, key, name);

        return schema == nullptr || schema->widget != ncl_material::ParamWidget::Choice || value == std::trunc(value);
    }

    bool ServerMaterials::AreParamsValid(std::span<const Binding> bindings, std::span<const ServerMaterialParam> params)
    {
        for (const ServerMaterialParam& param : params)
        {
            if (!std::isfinite(param.value))
            {
                return false;
            }

            for (const Binding& binding : bindings)
            {
                if (!IsWriteValid(binding, param.key, param.value))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool ServerMaterials::WriteParam(Binding& binding, std::string_view key, float scalar)
    {
        std::string_view name;
        const ncl_material::ParamSchema* schema = InstanceSchema(binding, key, name);

        if (schema == nullptr)
        {
            return false;
        }

        ncl_material::ParamValue value;
        value.type = ncl_material::ParamType::Scalar;
        value.scalar = schema->has_range ? std::clamp(scalar, schema->min_value, schema->max_value) : scalar;

        for (ncl_material::Param& param : binding.resolved.params)
        {
            if (param.name == name)
            {
                param.value = value;
                return true;
            }
        }

        binding.resolved.params.push_back(ncl_material::Param{std::string(name), value});
        return true;
    }

    void ServerMaterials::ApplyParams(OwnerBindings& owner)
    {
        for (Binding& binding : owner.candidates)
        {
            binding.resolved = *binding.preset_material;
            StudioContract_DisableRestrictedEffects(binding.resolved, manifests_.Get(binding.resolved.program));

            for (const ServerMaterialParam& param : owner.params)
            {
                WriteParam(binding, param.key, param.value);
            }
        }
    }

    bool ServerMaterials::SetParam(int entity_index, std::optional<uint16_t> sync_owner, const char* key, float value)
    {
        EntityBindings* bindings = FindEntity(entity_index);

        if (bindings == nullptr || key == nullptr || *key == '\0' || !std::isfinite(value))
        {
            return false;
        }

        const std::string key_snapshot(key);
        const ServerMaterialParam param{key_snapshot, value};
        if (!sync_owner.has_value() && !AreParamsValid(bindings->console, std::span(&param, 1)))
        {
            return false;
        }

        for (const auto& [owner_id, owner] : bindings->owners)
        {
            if ((!sync_owner.has_value() || owner_id == *sync_owner) && !AreParamsValid(owner.candidates, std::span(&param, 1)))
            {
                return false;
            }
        }

        bool written = false;

        if (!sync_owner.has_value())
        {
            for (Binding& binding : bindings->console)
            {
                written = WriteParam(binding, key_snapshot, value) || written;
            }
        }

        for (auto& [owner_id, owner] : bindings->owners)
        {
            if (sync_owner.has_value() && owner_id != *sync_owner)
            {
                continue;
            }

            for (Binding& binding : owner.candidates)
            {
                if (sync_owner.has_value() || IsAdmitted(*bindings, binding.texture_name))
                {
                    written = WriteParam(binding, key_snapshot, value) || written;
                }
            }
        }

        return written;
    }
    bool ServerMaterials::ReplaceOwner(
        const ServerMaterialBindingOwner& owner,
        std::span<const ServerMaterialBinding> bindings,
        std::span<const ServerMaterialParam> params
    )
    {
        const ServerMaterialBindingOwner owner_snapshot = owner;
        OwnerBindings replacement;
        replacement.requested.assign(bindings.begin(), bindings.end());
        replacement.params.assign(params.begin(), params.end());

        if (!AreParamsValid({}, replacement.params))
        {
            return false;
        }

        if (!IsValidEntityIndex(owner_snapshot.entity_index))
        {
            return true;
        }

        for (const ServerMaterialBinding& request : replacement.requested)
        {
            Binding binding;

            if (ResolveBinding(request.preset.c_str(), request.name.c_str(), true, binding))
            {
                StoreCandidate(replacement, std::move(binding));
            }
        }

        if (!AreParamsValid(replacement.candidates, replacement.params))
        {
            return false;
        }

        ApplyParams(replacement);

        EntityBindings& entity = bindings_[owner_snapshot.entity_index];
        entity.owners[owner_snapshot.sync_entity_id] = std::move(replacement);
        ReconcileAdmission(entity);
        return true;
    }

    bool ServerMaterials::ReplaceOwnerParams(const ServerMaterialBindingOwner& owner, std::span<const ServerMaterialParam> params)
    {
        const ServerMaterialBindingOwner owner_snapshot = owner;
        const std::vector<ServerMaterialParam> params_snapshot(params.begin(), params.end());
        if (!AreParamsValid({}, params_snapshot))
        {
            return false;
        }

        EntityBindings* entity = FindEntity(owner_snapshot.entity_index);

        if (entity == nullptr)
        {
            return true;
        }

        auto found = entity->owners.find(owner_snapshot.sync_entity_id);

        if (found == entity->owners.end())
        {
            return true;
        }

        if (!AreParamsValid(found->second.candidates, params_snapshot))
        {
            return false;
        }

        found->second.params = params_snapshot;
        ApplyParams(found->second);
        return true;
    }

    void ServerMaterials::RemoveOwner(const ServerMaterialBindingOwner& owner)
    {
        const ServerMaterialBindingOwner owner_snapshot = owner;
        EntityBindings* entity = FindEntity(owner_snapshot.entity_index);

        if (entity == nullptr || entity->owners.erase(owner_snapshot.sync_entity_id) == 0)
        {
            return;
        }

        ReconcileAdmission(*entity);

        if (entity->owners.empty() && entity->console.empty())
        {
            bindings_.erase(owner_snapshot.entity_index);
        }
    }

    void ServerMaterials::Clear()
    {
        bindings_.clear();

        if (presets_.empty())
        {
            return;
        }

        presets_.clear();
        generation_++;
    }

    void ServerMaterials::SetBinding(int entity_index, const char* preset, const char* name, bool network_session)
    {
        if (!network_session)
        {
            Bind(entity_index, std::nullopt, preset, name);
        }
    }

    void ServerMaterials::SetBinding(const ServerMaterialBindingOwner& owner, const char* preset, const char* name)
    {
        const ServerMaterialBindingOwner owner_snapshot = owner;
        Bind(owner_snapshot.entity_index, owner_snapshot.sync_entity_id, preset, name);
    }

    void ServerMaterials::RemoveBinding(int entity_index, const char* preset, const char* name, bool network_session)
    {
        if (!network_session)
        {
            Unbind(entity_index, preset, name);
        }
    }

    void ServerMaterials::ClearBindings(int entity_index)
    {
        bindings_.erase(entity_index);
    }

    bool ServerMaterials::SetParam(int entity_index, const char* key, float value, bool network_session)
    {
        return !network_session && SetParam(entity_index, std::nullopt, key, value);
    }

    bool ServerMaterials::SetParam(const ServerMaterialBindingOwner& owner, const char* key, float value)
    {
        const ServerMaterialBindingOwner owner_snapshot = owner;
        return SetParam(owner_snapshot.entity_index, owner_snapshot.sync_entity_id, key, value);
    }

    void ServerMaterials::NoteEntity(int entity_index, std::string_view model)
    {
        if (note_budget_ <= 0)
        {
            return;
        }

        note_budget_--;

        if (std::find(noted_entities_.begin(), noted_entities_.end(), entity_index) != noted_entities_.end())
        {
            return;
        }

        noted_entities_.push_back(entity_index);

        documents_.Report("  entity %d: %.*s\n", entity_index, static_cast<int>(model.size()), model.data());
    }

    const ncl_material::Material* ServerMaterials::BindingFor(const EntityMaterialQuery& query, bool network_session)
    {
        if (bindings_.empty() || query.entity_index <= 0)
        {
            return nullptr;
        }

        std::unordered_map<int, EntityBindings>::iterator it = bindings_.find(query.entity_index);

        if (it == bindings_.end())
        {
            return nullptr;
        }

        it->second.drawn = true;

        const EntityBindings& entity = it->second;

        if (!IsAdmitted(entity, query.texture_name))
        {
            return nullptr;
        }

        if (!network_session)
        {
            for (const Binding& binding : entity.console)
            {
                if (ncl_material::EqualsIgnoreCaseAscii(binding.texture_name, query.texture_name))
                {
                    return &binding.resolved;
                }
            }
        }

        for (const auto& [owner_id, owner] : entity.owners)
        {
            for (const Binding& binding : owner.candidates)
            {
                if (ncl_material::EqualsIgnoreCaseAscii(binding.texture_name, query.texture_name))
                {
                    return &binding.resolved;
                }
            }
        }

        return nullptr;
    }

    bool ServerMaterials::AnyBindingMatches(
        int entity_index,
        bool (*matches)(void*, const ncl_material::Material&),
        bool network_session,
        void* user
    )
    {
        const EntityBindings* entity = FindEntity(entity_index);

        if (entity == nullptr)
        {
            return false;
        }

        if (!network_session && std::any_of(entity->console.begin(), entity->console.end(), [&](const Binding& binding) {
                return matches(user, binding.resolved);
            }))
        {
            return true;
        }

        for (const auto& [owner_id, owner] : entity->owners)
        {
            for (const Binding& binding : owner.candidates)
            {
                if (IsAdmitted(*entity, binding.texture_name) && matches(user, binding.resolved))
                {
                    return true;
                }
            }
        }

        return false;
    }
    void ServerMaterials::ReportPresets()
    {
        documents_.Report("materials: %u presets loaded\n", static_cast<unsigned>(presets_.size()));

        for (const std::unique_ptr<LoadedPreset>& loaded : presets_)
        {
            if (!loaded->valid)
            {
                documents_.Report("  %s (failed)\n", loaded->path.c_str());
                continue;
            }

            documents_.Report("  %s\n", loaded->path.c_str());

            for (const ncl_material::PresetMaterial& material : loaded->preset.materials)
            {
                documents_.Report(
                    "    %-24s %s -> %s\n", material.name.c_str(), material.texture_name.c_str(), material.material.program.c_str()
                );
            }
        }
    }

    void ServerMaterials::ReportBindings()
    {
        for (const std::pair<const int, EntityBindings>& entity : bindings_)
        {
            auto report = [&](const Binding& binding) {
                if (!IsAdmitted(entity.second, binding.texture_name))
                {
                    return;
                }

                documents_.Report(
                    "  entity %d: %s %s on %s%s\n",
                    entity.first,
                    binding.preset.c_str(),
                    binding.name.c_str(),
                    binding.texture_name.c_str(),
                    entity.second.drawn ? "" : "  (never drawn)"
                );
            };

            for (const Binding& binding : entity.second.console)
            {
                report(binding);
            }

            for (const auto& [owner_id, owner] : entity.second.owners)
            {
                for (const Binding& binding : owner.candidates)
                {
                    report(binding);
                }
            }
        }
    }

    void ServerMaterials::ArmDiagnostics()
    {
        note_budget_ = 64;
        noted_entities_.clear();
    }
} // namespace render
