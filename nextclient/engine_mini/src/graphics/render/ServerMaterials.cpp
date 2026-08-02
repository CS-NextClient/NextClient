#include "engine.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common/cmd.h"
#include "common/common.h"
#include "common/filesystem.h"
#include "console/console.h"
#include "graphics/render/MaterialManifests.h"
#include "graphics/render/ServerMaterials.h"

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

    // What one entity may wear at once. Not a shape of the protocol - a binding names its
    // own texture and needs no slot - but a ceiling on what one entity can cost.
    constexpr size_t kMaxBindingsPerEntity = 16;

    // Separates the texture from the parameter in an instance key. Safe as a separator
    // because a studio texture name is a file name without a path.
    constexpr char kParamScopeSeparator = '/';

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
        // Materialised rather than applied at draw time because the preset's copy is
        // shared by every entity wearing it and must not be written to.
        ncl_material::Material resolved;
    };

    struct EntityBindings
    {
        std::vector<Binding> worn;
    };

    std::unordered_map<int, EntityBindings> g_Bindings;

    struct LoadedPreset
    {
        std::string path;
        ncl_material::MaterialPreset preset;

        // A file that could not be read or parsed is remembered as a failure, so a binding
        // that names it does not have the client read it again on every entity.
        bool valid = false;
    };

    // Pointer stability is not relied on: lookups return by value or copy what they need.
    std::vector<std::unique_ptr<LoadedPreset>> g_Presets;

    unsigned int g_Generation;

    // Diagnostics for r_mat_entities; a scene with fewer than this many studio models is
    // listed in full on the next frame.
    int g_NoteBudget;
    std::vector<int> g_NotedEntities;

    bool HasPresetExtension(std::string_view path)
    {
        std::string_view suffix(kPresetExtension);

        return path.size() > suffix.size() &&
               ncl_material::EqualsIgnoreCaseAscii(path.substr(path.size() - suffix.size()), suffix);
    }

    bool ReadFile(const char* path, std::string& out)
    {
        FileHandle_t file = FS_Open(path, "rb");
        if (file == nullptr)
        {
            return false;
        }

        unsigned int size = FS_Size(file);

        // A ceiling because the file is named by the server and read inside a message
        // handler: an allocation that throws there unwinds into the engine's own dispatch,
        // and one that merely succeeds still costs three copies of the file at once.
        if (size > kMaxPresetBytes)
        {
            Con_Printf("materials: %s is %u bytes, the limit is %u\n", path, size, kMaxPresetBytes);
            FS_Close(file);
            return false;
        }

        out.resize(size);

        int read = size != 0 ? FS_Read(out.data(), static_cast<int>(size), file) : 0;
        FS_Close(file);

        return read == static_cast<int>(size);
    }

    // Materials are dropped one by one rather than rejecting the preset, unlike a budget
    // overflow: a program the client does not have is a version difference, and an older
    // client should still draw the materials it does understand instead of none of them.
    size_t DropMaterialsWithoutAMatchingProgram(ncl_material::MaterialPreset& preset, const char* path)
    {
        size_t dropped = 0;

        for (size_t i = preset.materials.size(); i > 0; i--)
        {
            ncl_material::PresetMaterial& material = preset.materials[i - 1];
            const ncl_material::ProgramManifest* manifest = render::Manifest_Get(material.material.program);

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

            Con_Printf("materials: %s: \"%s\" dropped: %s\n", path, material.name.c_str(), reason.c_str());

            preset.materials.erase(preset.materials.begin() + (i - 1));
            dropped++;
        }

        return dropped;
    }

    const LoadedPreset* FindLoaded(std::string_view path)
    {
        for (const std::unique_ptr<LoadedPreset>& loaded : g_Presets)
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
    const LoadedPreset* LoadPreset(const char* path)
    {
        if (path == nullptr || *path == '\0')
        {
            return nullptr;
        }

        if (const LoadedPreset* loaded = FindLoaded(path))
        {
            return loaded;
        }

        if (g_Presets.size() >= kMaxPresets)
        {
            Con_Printf("materials: %s not loaded, already holding %u presets\n",
                path, static_cast<unsigned>(g_Presets.size()));
            return nullptr;
        }

        std::unique_ptr<LoadedPreset> loaded = std::make_unique<LoadedPreset>();
        loaded->path = path;

        std::string json;

        if (!ncl_material::IsAdmissibleRelativePath(path, kMaxPresetPathLength) || !HasPresetExtension(path))
        {
            Con_Printf("materials: \"%s\" is not an admissible preset path\n", path);
        }
        else if (!ReadFile(path, json))
        {
            Con_Printf("materials: %s could not be read\n", path);
        }
        else
        {
            ncl_utils::ResultT<ncl_material::MaterialPreset> parsed =
                ncl_material::ParseMaterialPreset(json, ncl_material::PresetBudget {});

            if (parsed.has_error())
            {
                Con_Printf("materials: %s: %s\n", path, parsed.error_str().c_str());
            }
            else
            {
                loaded->preset = *parsed;
                loaded->valid = true;

                size_t dropped = DropMaterialsWithoutAMatchingProgram(loaded->preset, path);

                Con_Printf("materials: %s: %u materials",
                    path, static_cast<unsigned>(loaded->preset.materials.size()));

                if (dropped != 0)
                {
                    Con_Printf(", %u dropped", static_cast<unsigned>(dropped));
                }

                Con_Printf("\n");
            }
        }

        g_Presets.push_back(std::move(loaded));
        g_Generation++;

        return g_Presets.back().get();
    }

    EntityBindings* FindEntity(int entity_index)
    {
        std::unordered_map<int, EntityBindings>::iterator it = g_Bindings.find(entity_index);

        return it != g_Bindings.end() ? &it->second : nullptr;
    }

    void Bind(int entity_index, const char* preset_path, const char* name)
    {
        // The index comes off the wire, so it is bounded by what the client can actually
        // draw rather than by the read width.
        if (entity_index <= 0 || entity_index >= kMaxBoundEntities || name == nullptr || *name == '\0')
        {
            return;
        }

        const LoadedPreset* loaded = LoadPreset(preset_path);

        if (loaded == nullptr || !loaded->valid)
        {
            return;
        }

        const ncl_material::PresetMaterial* material = loaded->preset.FindByName(name);

        if (material == nullptr)
        {
            Con_Printf("materials: %s has no material \"%s\"\n", loaded->path.c_str(), name);
            return;
        }

        EntityBindings& bindings = g_Bindings[entity_index];

        // Whatever else dressed this texture is replaced rather than joined: two materials
        // on one texture is a question with no answer, and this is the one just asked for.
        for (size_t i = bindings.worn.size(); i > 0; i--)
        {
            if (ncl_material::EqualsIgnoreCaseAscii(bindings.worn[i - 1].texture_name, material->texture_name))
            {
                bindings.worn.erase(bindings.worn.begin() + (i - 1));
            }
        }

        if (bindings.worn.size() >= kMaxBindingsPerEntity)
        {
            Con_Printf("materials: entity %d already wears %u materials\n",
                entity_index, static_cast<unsigned>(bindings.worn.size()));
            return;
        }

        Binding binding;
        binding.preset = loaded->path;
        binding.name = material->name;
        binding.texture_name = material->texture_name;

        // A fresh binding starts from the material as the preset declares it. Instance
        // values are written over it, so carrying the previous one's over would apply them
        // to something that never asked for them.
        binding.resolved = material->material;

        bindings.worn.push_back(std::move(binding));
    }

    void Unbind(int entity_index, const char* preset_path, const char* name)
    {
        EntityBindings* bindings = FindEntity(entity_index);

        if (bindings == nullptr || preset_path == nullptr || name == nullptr)
        {
            return;
        }

        for (size_t i = bindings->worn.size(); i > 0; i--)
        {
            const Binding& binding = bindings->worn[i - 1];

            if (ncl_material::EqualsIgnoreCaseAscii(binding.preset, preset_path) &&
                ncl_material::EqualsIgnoreCaseAscii(binding.name, name))
            {
                bindings->worn.erase(bindings->worn.begin() + (i - 1));
            }
        }

        if (bindings->worn.empty())
        {
            g_Bindings.erase(entity_index);
        }
    }

    // The step count is what travels, not the value: the quantum lives in the client's
    // manifest, so a value off the declared grid cannot be expressed at all.
    bool WriteInstanceValue(Binding& binding, std::string_view name, int steps)
    {
        const ncl_material::ProgramManifest* manifest = render::Manifest_Get(binding.resolved.program);

        if (manifest == nullptr)
        {
            return false;
        }

        std::string param_name(name);
        const ncl_material::ParamSchema* schema = manifest->FindParam(param_name);

        // Scalars only: the quantum is what bounds how many distinct values an entity can
        // ask for, and only scalars carry one.
        if (schema == nullptr || !schema->instance ||
            schema->type != ncl_material::ParamType::Scalar || schema->quantum <= 0.0f)
        {
            return false;
        }

        ncl_material::ParamValue value;
        value.type = ncl_material::ParamType::Scalar;
        value.scalar = static_cast<float>(steps) * schema->quantum;

        for (ncl_material::Param& param : binding.resolved.params)
        {
            if (param.name == param_name)
            {
                param.value = value;
                return true;
            }
        }

        binding.resolved.params.push_back(ncl_material::Param {param_name, value});

        return true;
    }
}

void render::ServerMaterials_Clear()
{
    g_Bindings.clear();

    if (g_Presets.empty())
    {
        return;
    }

    g_Presets.clear();
    g_Generation++;
}

void render::ServerMaterials_SetBinding(int entity_index, const char* preset, const char* name)
{
    Bind(entity_index, preset, name);
}

void render::ServerMaterials_RemoveBinding(int entity_index, const char* preset, const char* name)
{
    Unbind(entity_index, preset, name);
}

void render::ServerMaterials_ClearBindings(int entity_index)
{
    g_Bindings.erase(entity_index);
}

bool render::ServerMaterials_SetParam(int entity_index, const char* key, int steps)
{
    EntityBindings* bindings = FindEntity(entity_index);

    if (bindings == nullptr || key == nullptr || *key == '\0')
    {
        return false;
    }

    std::string_view view(key);
    size_t separator = view.find(kParamScopeSeparator);

    std::string_view texture = separator != std::string_view::npos ? view.substr(0, separator) : std::string_view();
    std::string_view name = separator != std::string_view::npos ? view.substr(separator + 1) : view;

    if (name.empty())
    {
        return false;
    }

    bool written = false;

    for (Binding& binding : bindings->worn)
    {
        // No texture in the key means every material this entity wears that declares the
        // parameter; a plugin saying "this weapon is worn" should not have to name each.
        if (!texture.empty() && !ncl_material::EqualsIgnoreCaseAscii(binding.texture_name, texture))
        {
            continue;
        }

        written = WriteInstanceValue(binding, name, steps) || written;
    }

    return written;
}

void render::ServerMaterials_NoteEntity(int entity_index, std::string_view model)
{
    if (g_NoteBudget <= 0)
    {
        return;
    }

    g_NoteBudget--;

    if (std::find(g_NotedEntities.begin(), g_NotedEntities.end(), entity_index) != g_NotedEntities.end())
    {
        return;
    }

    g_NotedEntities.push_back(entity_index);

    Con_Printf("  entity %d: %.*s\n", entity_index, static_cast<int>(model.size()), model.data());
}

const ncl_material::Material* render::ServerMaterials_BindingFor(const EntityMaterialQuery& query)
{
    if (g_Bindings.empty() || query.entity_index <= 0)
    {
        return nullptr;
    }

    std::unordered_map<int, EntityBindings>::const_iterator it = g_Bindings.find(query.entity_index);

    if (it == g_Bindings.end())
    {
        return nullptr;
    }

    for (const Binding& binding : it->second.worn)
    {
        if (ncl_material::EqualsIgnoreCaseAscii(binding.texture_name, query.texture_name))
        {
            return &binding.resolved;
        }
    }

    return nullptr;
}

unsigned int render::ServerMaterials_Generation()
{
    return g_Generation;
}

namespace
{
    void MaterialEntities_f()
    {
        g_NoteBudget = 64;
        g_NotedEntities.clear();

        Con_Printf("r_mat_entities: studio entities drawn from here\n");
    }

    void MaterialPresets_f()
    {
        if (Cmd_Argc() >= 2)
        {
            if (V_stricmp(Cmd_Argv(1), "clear") == 0)
            {
                render::ServerMaterials_Clear();
                Con_Printf("materials: presets cleared\n");
                return;
            }

            LoadPreset(Cmd_Argv(1));
            return;
        }

        Con_Printf("materials: %u presets loaded\n", static_cast<unsigned>(g_Presets.size()));

        for (const std::unique_ptr<LoadedPreset>& loaded : g_Presets)
        {
            if (!loaded->valid)
            {
                Con_Printf("  %s (failed)\n", loaded->path.c_str());
                continue;
            }

            Con_Printf("  %s\n", loaded->path.c_str());

            for (const ncl_material::PresetMaterial& material : loaded->preset.materials)
            {
                Con_Printf("    %-24s %s -> %s\n",
                    material.name.c_str(), material.texture_name.c_str(), material.material.program.c_str());
            }
        }
    }

    void MaterialBind_f()
    {
        if (Cmd_Argc() >= 4)
        {
            Bind(atoi(Cmd_Argv(1)), Cmd_Argv(2), Cmd_Argv(3));
            return;
        }

        if (Cmd_Argc() == 2)
        {
            render::ServerMaterials_ClearBindings(atoi(Cmd_Argv(1)));
            return;
        }

        Con_Printf("r_mat_bind <entity> <preset> <material>, or <entity> alone to undress it\n");

        for (const std::pair<const int, EntityBindings>& entity : g_Bindings)
        {
            for (const Binding& binding : entity.second.worn)
            {
                Con_Printf("  entity %d: %s %s on %s\n",
                    entity.first, binding.preset.c_str(), binding.name.c_str(), binding.texture_name.c_str());
            }
        }
    }

    void MaterialParam_f()
    {
        if (Cmd_Argc() < 4)
        {
            Con_Printf("r_mat_param <entity> <name|texture/name> <steps>\n");
            return;
        }

        if (!render::ServerMaterials_SetParam(atoi(Cmd_Argv(1)), Cmd_Argv(2), atoi(Cmd_Argv(3))))
        {
            Con_Printf("r_mat_param: nothing took it; the entity wears nothing matching, or "
                       "\"%s\" is not an instance scalar of its program\n", Cmd_Argv(2));
        }
    }
}

void render::ServerMaterials_RegisterCommands()
{
    Cmd_AddCommand("r_mat_presets", MaterialPresets_f);
    Cmd_AddCommand("r_mat_bind", MaterialBind_f);
    Cmd_AddCommand("r_mat_entities", MaterialEntities_f);
    Cmd_AddCommand("r_mat_param", MaterialParam_f);
}
