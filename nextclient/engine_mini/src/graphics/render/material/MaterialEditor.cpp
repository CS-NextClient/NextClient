#include "graphics/render/material/MaterialEditor.h"

#include <algorithm>
#include <cmath>

#include <ncl_material/material_parse.h>
#include <ncl_material/MaterialResolve.h>
#include <ncl_material/MaterialPreset.h>
#include <tier1/strtools.h>

#include "graphics/render/material/MaterialDocumentStore.h"
#include "graphics/render/material/MaterialManifestCatalog.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/model/StudioModelCache.h"

namespace render
{
    namespace
    {
        const char* const kEmpty = "";
        constexpr size_t kMaxTexturePathLength = 128;

        std::string DocumentPathOf(const std::string& model)
        {
            return model.size() > 4 ? model.substr(0, model.size() - 4) + ".mat" : std::string();
        }

    } // namespace

    MaterialEditor::MaterialEditor(
        StudioModelCache& models,
        ServerMaterials& server_materials,
        ShaderManager& shaders,
        MaterialManifestCatalog& manifests,
        MaterialDocumentStore& documents
    ) :
        models_(models),
        server_materials_(server_materials),
        shaders_(shaders),
        manifests_(manifests),
        documents_(documents)
    {}

    const ncl_material::Material* MaterialEditSession::FindOverride(std::string_view model_name, std::string_view texture_name) const
    {
        if (!active || model != model_name)
        {
            return nullptr;
        }

        for (const ncl_material::MaterialEntry& entry : entries)
        {
            if (ncl_material::EqualsIgnoreCaseAscii(entry.texture_name, texture_name))
            {
                return &entry.material;
            }
        }

        return nullptr;
    }

    const ncl_material::ProgramManifest* MaterialEditor::ManifestOf(const char* program)
    {
        if (program == nullptr || *program == '\0')
        {
            return nullptr;
        }

        return manifests_.Get(program);
    }

    const ncl_material::ParamGroup* MaterialEditor::GroupOf(const char* program, int group)
    {
        const ncl_material::ProgramManifest* manifest = ManifestOf(program);

        if (manifest == nullptr || group < 0 || group >= static_cast<int>(manifest->groups.size()))
        {
            return nullptr;
        }

        return &manifest->groups[group];
    }

    const ncl_material::ParamSchema* MaterialEditor::ChoiceOf(const char* program, const char* param)
    {
        const ncl_material::ProgramManifest* manifest = ManifestOf(program);
        const ncl_material::ParamSchema* schema = manifest != nullptr && param != nullptr ? manifest->FindParam(param) : nullptr;

        return schema != nullptr && schema->widget == ncl_material::ParamWidget::Choice ? schema : nullptr;
    }

    const ncl_material::Material* MaterialEditor::DrawnMaterial(
        const char* model,
        const char* texture_name,
        int entity_index,
        ncl_material::MaterialOrigin* origin,
        bool (*network_session)()
    )
    {
        if (entity_index > 0 && model != nullptr && texture_name != nullptr)
        {
            EntityMaterialQuery query;
            query.entity_index = entity_index;
            query.texture_name = texture_name;

            if (const ncl_material::Material* bound = server_materials_.BindingFor(query, network_session()))
            {
                if (origin != nullptr)
                {
                    *origin = ncl_material::MaterialOrigin::Instance;
                }

                return bound;
            }
        }

        return models_.ResolveMaterialByName(model, texture_name, origin);
    }

    int MaterialEditor::GetProgramCount()
    {
        RebuildPrograms();

        return static_cast<int>(programs_.size());
    }

    const char* MaterialEditor::get_program_name(int index)
    {
        return index >= 0 && index < static_cast<int>(programs_.size()) ? programs_[index].c_str() : nullptr;
    }

    int MaterialEditor::GetParamCount(const char* program)
    {
        const ncl_material::ProgramManifest* manifest = ManifestOf(program);

        return manifest != nullptr ? static_cast<int>(manifest->parameters.size()) : 0;
    }

    bool MaterialEditor::GetParam(const char* program, int index, MaterialEditParam* out)
    {
        const ncl_material::ProgramManifest* manifest = ManifestOf(program);

        if (manifest == nullptr || out == nullptr || index < 0 || index >= static_cast<int>(manifest->parameters.size()))
        {
            return false;
        }

        const ncl_material::ParamSchema& schema = manifest->parameters[index];

        out->name = schema.name.c_str();
        out->label = schema.get_label_or_name().c_str();
        out->hint = schema.hint.c_str();
        out->type = TypeOf(schema.type);
        switch (schema.widget)
        {
            case ncl_material::ParamWidget::Color:
                out->widget = MaterialEditParamWidget::Color;
                break;
            case ncl_material::ParamWidget::Choice:
                out->widget = MaterialEditParamWidget::Choice;
                break;
            default:
                out->widget = MaterialEditParamWidget::Default;
                break;
        }
        out->instance = schema.instance;
        out->has_range = schema.has_range;
        out->min_value = schema.min_value;
        out->max_value = schema.max_value;
        out->has_default = schema.has_default;
        out->default_scalar = schema.default_value.scalar;
        out->default_texture = schema.default_value.texture_ref.c_str();

        for (size_t i = 0; i < schema.default_value.vec4.size(); i++)
        {
            out->default_vec4[i] = schema.default_value.vec4[i];
        }

        return true;
    }

    int MaterialEditor::GetChoiceCount(const char* program, const char* param)
    {
        const ncl_material::ParamSchema* schema = ChoiceOf(program, param);

        return schema != nullptr ? static_cast<int>(schema->choices.size()) : 0;
    }

    const char* MaterialEditor::GetChoiceLabel(const char* program, const char* param, int index)
    {
        const ncl_material::ParamSchema* schema = ChoiceOf(program, param);

        return schema != nullptr && index >= 0 && index < static_cast<int>(schema->choices.size()) ? schema->choices[index].c_str()
                                                                                                   : kEmpty;
    }

    int MaterialEditor::GetGroupCount(const char* program)
    {
        const ncl_material::ProgramManifest* manifest = ManifestOf(program);

        return manifest != nullptr ? static_cast<int>(manifest->groups.size()) : 0;
    }

    const char* MaterialEditor::GetGroupLabel(const char* program, int group)
    {
        const ncl_material::ParamGroup* found = GroupOf(program, group);

        return found != nullptr ? found->get_label_or_name().c_str() : kEmpty;
    }

    int MaterialEditor::GetGroupParamCount(const char* program, int group)
    {
        const ncl_material::ParamGroup* found = GroupOf(program, group);

        return found != nullptr ? static_cast<int>(found->params.size()) : 0;
    }

    int MaterialEditor::GetGroupParam(const char* program, int group, int index)
    {
        const ncl_material::ParamGroup* found = GroupOf(program, group);

        if (found == nullptr || index < 0 || index >= static_cast<int>(found->params.size()))
        {
            return -1;
        }

        return static_cast<int>(found->params[index]);
    }

    int MaterialEditor::get_model_count()
    {
        return models_.ModelCount();
    }

    const char* MaterialEditor::GetModelName(int index)
    {
        return models_.ModelName(index);
    }

    int MaterialEditor::get_texture_count(const char* model)
    {
        return models_.TextureCount(model);
    }

    const char* MaterialEditor::get_texture_name(const char* model, int index)
    {
        return models_.TextureName(model, index);
    }

    MaterialEditSource MaterialEditor::GetMaterialSource(
        const char* model,
        const char* texture_name,
        int entity_index,
        bool (*network_session)()
    )
    {
        ncl_material::MaterialOrigin origin = ncl_material::MaterialOrigin::Implicit;

        if (DrawnMaterial(model, texture_name, entity_index, &origin, network_session) == nullptr)
        {
            return MaterialEditSource::None;
        }

        switch (origin)
        {
            case ncl_material::MaterialOrigin::Instance:
                return MaterialEditSource::Instance;
            case ncl_material::MaterialOrigin::ModelDocument:
                return MaterialEditSource::ModelDocument;
            default:
                return MaterialEditSource::Implicit;
        }
    }

    int MaterialEditor::GetSkinFamilyCount(const char* model)
    {
        return models_.SkinFamilyCount(model);
    }

    int MaterialEditor::GetBodyCount(const char* model)
    {
        return models_.BodyCount(model);
    }

    bool MaterialEditor::GetBodyName(const char* model, int body, char* out, int out_len)
    {
        return models_.BodyName(model, body, out, out_len);
    }

    bool MaterialEditor::BeginGroup(const char* model, const char* const* textures, int count, int entity_index, bool (*network_session)())
    {
        if (model == nullptr || textures == nullptr || count <= 0 || count > static_cast<int>(ncl_material::kMaxEntries))
        {
            return false;
        }

        MaterialEditSession replacement;
        replacement.model = model;
        replacement.entity_index = entity_index;

        for (int index = 0; index < count; index++)
        {
            if (textures[index] == nullptr || *textures[index] == '\0')
            {
                return false;
            }

            const bool duplicate =
                std::any_of(replacement.entries.begin(), replacement.entries.end(), [&](const ncl_material::MaterialEntry& entry) {
                    return ncl_material::EqualsIgnoreCaseAscii(entry.texture_name, textures[index]);
                });

            if (duplicate)
            {
                continue;
            }

            const ncl_material::Material* current = DrawnMaterial(model, textures[index], entity_index, nullptr, network_session);

            if (current == nullptr)
            {
                return false;
            }

            replacement.entries.push_back({textures[index], *current});
        }

        replacement.active = true;
        session_ = std::move(replacement);

        return true;
    }

    int MaterialEditor::get_selection_count()
    {
        return static_cast<int>(session_.entries.size());
    }

    const char* MaterialEditor::get_selected_texture(int index)
    {
        return index >= 0 && index < get_selection_count() ? session_.entries[index].texture_name.c_str() : kEmpty;
    }

    bool MaterialEditor::has_mixed_programs()
    {
        return session_.active &&
               std::any_of(session_.entries.begin(), session_.entries.end(), [this](const ncl_material::MaterialEntry& entry) {
                   return entry.material.program != session_.entries.front().material.program;
               });
    }

    bool MaterialEditor::is_param_mixed(const char* name)
    {
        if (!session_.active || name == nullptr)
        {
            return false;
        }

        const ncl_material::ParamValue* primary = session_.entries.front().material.FindParam(name);

        for (const ncl_material::MaterialEntry& entry : session_.entries)
        {
            const ncl_material::ParamValue* value = entry.material.FindParam(name);

            if ((primary == nullptr) != (value == nullptr))
            {
                return true;
            }

            if (primary != nullptr && (primary->type != value->type || primary->scalar != value->scalar || primary->vec4 != value->vec4 ||
                                       primary->texture_ref != value->texture_ref))
            {
                return true;
            }
        }

        return false;
    }

    void MaterialEditor::End()
    {
        session_ = MaterialEditSession{};
    }

    bool MaterialEditor::is_editing()
    {
        return session_.active;
    }

    int MaterialEditor::get_edited_entity_index()
    {
        return session_.active ? session_.entity_index : 0;
    }

    const char* MaterialEditor::get_edited_model()
    {
        return session_.active ? session_.model.c_str() : kEmpty;
    }

    const char* MaterialEditor::get_edited_texture()
    {
        return session_.active ? session_.entries.front().texture_name.c_str() : kEmpty;
    }

    const char* MaterialEditor::get_edited_program()
    {
        return session_.active && !has_mixed_programs() ? session_.entries.front().material.program.c_str() : kEmpty;
    }

    bool MaterialEditor::SetEditedProgram(const char* program)
    {
        if (!session_.active || program == nullptr || *program == '\0')
        {
            return false;
        }

        // The legacy program is what a model with no material of its own resolves to,
        // so naming it is how an author says the material should do nothing.
        const ncl_material::ProgramManifest* manifest = ManifestOf(program);
        bool legacy = program == std::string_view(ncl_material::kLegacyProgram);

        if (manifest == nullptr && !legacy)
        {
            return false;
        }

        for (ncl_material::MaterialEntry& entry : session_.entries)
        {
            entry.material.program = program;

            std::vector<ncl_material::Param> kept;

            for (ncl_material::Param& param : entry.material.params)
            {
                const ncl_material::ParamSchema* schema = manifest != nullptr ? manifest->FindParam(param.name) : nullptr;

                if (schema != nullptr && schema->type == param.value.type)
                {
                    kept.push_back(std::move(param));
                }
            }

            entry.material.params = std::move(kept);

            if (manifest != nullptr)
            {
                ncl_material::ApplyDefaults(entry.material, *manifest);
            }
        }

        return true;
    }

    bool MaterialEditor::get_scalar(const char* name, float* out)
    {
        const ncl_material::Param* param = FindParam(name);

        if (param == nullptr || out == nullptr || param->value.type != ncl_material::ParamType::Scalar)
        {
            return false;
        }

        *out = param->value.scalar;

        return true;
    }

    bool MaterialEditor::get_vec4(const char* name, float* out)
    {
        const ncl_material::Param* param = FindParam(name);

        if (param == nullptr || out == nullptr || param->value.type != ncl_material::ParamType::Vec4)
        {
            return false;
        }

        for (size_t i = 0; i < param->value.vec4.size(); i++)
        {
            out[i] = param->value.vec4[i];
        }

        return true;
    }

    const char* MaterialEditor::get_texture(const char* name)
    {
        const ncl_material::Param* param = FindParam(name);

        if (param == nullptr || param->value.type != ncl_material::ParamType::TextureRef)
        {
            return kEmpty;
        }

        return param->value.texture_ref.c_str();
    }

    bool MaterialEditor::BeginParamPreview(const char* name)
    {
        if (SchemaOf(name) == nullptr || !session_.preview_values.empty())
        {
            return false;
        }

        session_.preview_param = name;

        for (const ncl_material::MaterialEntry& entry : session_.entries)
        {
            MaterialEditSession::ParamSnapshot snapshot;

            for (size_t index = 0; index < entry.material.params.size(); index++)
            {
                const ncl_material::Param& param = entry.material.params[index];

                if (param.name == name)
                {
                    snapshot.present = true;
                    snapshot.index = index;
                    snapshot.value = param.value;
                    break;
                }
            }

            session_.preview_values.push_back(std::move(snapshot));
        }

        return true;
    }

    void MaterialEditor::EndParamPreview(bool accept)
    {
        if (!accept)
        {
            for (size_t index = 0; index < session_.preview_values.size(); index++)
            {
                std::vector<ncl_material::Param>& params = session_.entries[index].material.params;
                const MaterialEditSession::ParamSnapshot& snapshot = session_.preview_values[index];

                std::erase_if(params, [this](const ncl_material::Param& param) { return param.name == session_.preview_param; });

                if (snapshot.present)
                {
                    params.insert(params.begin() + std::min(snapshot.index, params.size()), {session_.preview_param, snapshot.value});
                }
            }
        }

        session_.preview_param.clear();
        session_.preview_values.clear();
    }

    bool MaterialEditor::SetScalar(const char* name, float value)
    {
        const ncl_material::ParamSchema* schema = SchemaOf(name);

        if (schema == nullptr || schema->type != ncl_material::ParamType::Scalar || !std::isfinite(value))
        {
            return false;
        }

        ncl_material::ParamValue stored;
        stored.type = ncl_material::ParamType::Scalar;

        if (schema->widget == ncl_material::ParamWidget::Choice && value != std::trunc(value))
        {
            return false;
        }

        stored.scalar = value;

        if (schema->has_range)
        {
            stored.scalar = std::clamp(stored.scalar, schema->min_value, schema->max_value);
        }

        SetParam(name, stored);

        return true;
    }

    bool MaterialEditor::SetVec4(const char* name, const float* value)
    {
        const ncl_material::ParamSchema* schema = SchemaOf(name);

        if (schema == nullptr || value == nullptr || schema->type != ncl_material::ParamType::Vec4)
        {
            return false;
        }

        ncl_material::ParamValue stored;
        stored.type = ncl_material::ParamType::Vec4;

        for (size_t i = 0; i < stored.vec4.size(); i++)
        {
            if (!std::isfinite(value[i]))
            {
                return false;
            }

            // Held inside the declared range rather than refused, as a scalar is: a
            // typed number outside it would otherwise be kept until saving and then
            // rejected, with the model in the meantime showing a value the program
            // never allowed.
            stored.vec4[i] = schema->has_range ? std::clamp(value[i], schema->min_value, schema->max_value) : value[i];
        }

        SetParam(name, stored);

        return true;
    }

    bool MaterialEditor::SetTexture(const char* name, const char* path)
    {
        const ncl_material::ParamSchema* schema = SchemaOf(name);

        if (schema == nullptr || path == nullptr || schema->type != ncl_material::ParamType::TextureRef)
        {
            return false;
        }

        // An empty path clears the reference instead of writing one nothing can open.
        if (*path == '\0')
        {
            return ClearParam(name);
        }

        // Checked here so the panel never has to know the rule: the same one the
        // renderer applies before it opens a material's image, refused at the point
        // where an author can still see why.
        if (!ncl_material::IsAdmissibleTexturePath(path, kMaxTexturePathLength))
        {
            return false;
        }

        ncl_material::ParamValue stored;
        stored.type = ncl_material::ParamType::TextureRef;
        stored.texture_ref = path;

        SetParam(name, stored);

        return true;
    }

    bool MaterialEditor::ClearParam(const char* name)
    {
        if (!session_.active || name == nullptr)
        {
            return false;
        }

        bool removed = false;
        for (ncl_material::MaterialEntry& entry : session_.entries)
        {
            const size_t count =
                std::erase_if(entry.material.params, [name](const ncl_material::Param& param) { return param.name == name; });
            removed = count > 0 || removed;
        }
        return removed;
    }

    const char* MaterialEditor::Validate()
    {
        validation_text_.clear();

        if (!session_.active)
        {
            return kEmpty;
        }

        for (const ncl_material::MaterialEntry& entry : session_.entries)
        {
            if (entry.material.program == ncl_material::kLegacyProgram)
            {
                continue;
            }

            const ncl_material::ProgramManifest* manifest = ManifestOf(entry.material.program.c_str());

            if (manifest == nullptr)
            {
                validation_text_ = entry.texture_name + ": no program \"" + entry.material.program + "\"";
                break;
            }

            ncl_utils::Result<> result = ncl_material::Validate(entry.material, *manifest);

            if (result.has_error())
            {
                validation_text_ = entry.texture_name + ": " + result.error_str();
                break;
            }
        }

        return validation_text_.c_str();
    }

    bool MaterialEditor::Save(const char* model, char* error_out, int error_out_len)
    {
        std::string error;
        bool ok = SaveTo(model != nullptr ? model : "", error);

        if (!ok && error_out != nullptr && error_out_len > 0)
        {
            V_strncpy(error_out, error.c_str(), error_out_len);
        }

        return ok;
    }

    const char* MaterialEditor::GetPresetStub()
    {
        preset_stub_.clear();

        if (!session_.active)
        {
            return kEmpty;
        }

        if (session_.entries.size() > 1)
        {
            preset_stub_ = "{\"version\":1,\"materials\":[";
        }

        for (const ncl_material::MaterialEntry& entry : session_.entries)
        {
            ncl_material::PresetMaterial material;
            material.name = entry.texture_name;
            material.texture_name = entry.texture_name;
            material.material = entry.material;

            if (&entry != &session_.entries.front())
            {
                preset_stub_ += ",";
            }

            preset_stub_ += ncl_material::WritePresetMaterial(material);
        }

        if (session_.entries.size() > 1)
        {
            preset_stub_ += "]}";
        }

        return preset_stub_.c_str();
    }

    void MaterialEditor::Reload()
    {
        models_.ReloadDocuments();
    }

    void MaterialEditor::RebuildPrograms()
    {
        programs_.clear();

        for (std::string& name : shaders_.ListPrograms())
        {
            if (manifests_.Get(name) != nullptr)
            {
                programs_.push_back(std::move(name));
            }
        }
    }

    ncl_material::Param* MaterialEditor::FindParam(const char* name)
    {
        if (!session_.active || name == nullptr)
        {
            return nullptr;
        }

        for (ncl_material::Param& param : session_.entries.front().material.params)
        {
            if (param.name == name)
            {
                return &param;
            }
        }

        return nullptr;
    }

    const ncl_material::ParamSchema* MaterialEditor::SchemaOf(const char* name)
    {
        if (!session_.active ||
            std::any_of(session_.entries.begin(), session_.entries.end(), [this](const ncl_material::MaterialEntry& entry) {
                return entry.material.program != session_.entries.front().material.program;
            }))
        {
            return nullptr;
        }

        const ncl_material::ProgramManifest* manifest = ManifestOf(session_.entries.front().material.program.c_str());

        return manifest != nullptr && name != nullptr ? manifest->FindParam(name) : nullptr;
    }

    void MaterialEditor::SetParam(const char* name, const ncl_material::ParamValue& value)
    {
        for (ncl_material::MaterialEntry& entry : session_.entries)
        {
            auto found = std::find_if(entry.material.params.begin(), entry.material.params.end(), [name](const ncl_material::Param& param) {
                return param.name == name;
            });

            if (found != entry.material.params.end())
            {
                found->value = value;
            }
            else
            {
                entry.material.params.push_back({name, value});
            }
        }
    }

    bool MaterialEditor::SaveTo(const std::string& model, std::string& error)
    {
        if (!session_.active)
        {
            error = "nothing is being edited";

            return false;
        }

        std::string target = model.empty() ? session_.model : model;
        std::string path = DocumentPathOf(target);

        if (path.empty())
        {
            error = "\"" + target + "\" is not a model path";

            return false;
        }

        // Read first: the document describes every texture of the model, and rewriting
        // it from the one texture under edit would drop the others.
        ncl_material::MaterialDoc doc;
        std::string existing;

        const MaterialReadStatus read_status = documents_.ReadFile(path.c_str(), existing);

        if (read_status != MaterialReadStatus::Ready && read_status != MaterialReadStatus::NotFound)
        {
            error = "cannot read " + path;
            return false;
        }

        if (read_status == MaterialReadStatus::Ready)
        {
            ncl_utils::ResultT<ncl_material::MaterialDoc> parsed = ncl_material::ParseMaterialDoc(existing);

            if (parsed.has_error())
            {
                // Refused rather than overwritten: whatever is in there was authored by
                // somebody, and a save is not the moment to discard it.
                error = path + ": " + parsed.error_str();

                return false;
            }

            doc = *parsed;
        }

        for (const ncl_material::MaterialEntry& selected : session_.entries)
        {
            auto found = std::find_if(doc.entries.begin(), doc.entries.end(), [&](const ncl_material::MaterialEntry& entry) {
                return ncl_material::EqualsIgnoreCaseAscii(entry.texture_name, selected.texture_name);
            });

            if (found != doc.entries.end())
            {
                found->material = selected.material;
            }
            else
            {
                doc.entries.push_back(selected);
            }
        }

        const std::string serialized = ncl_material::WriteMaterialDoc(doc);
        const ncl_utils::ResultT<ncl_material::MaterialDoc> checked = ncl_material::ParseMaterialDoc(serialized);
        const std::string validation = Validate();

        if (checked.has_error() || !validation.empty())
        {
            error = checked.has_error() ? checked.error_str() : validation;
            return false;
        }

        if (!documents_.Write(path, serialized, error))
        {
            return false;
        }

        models_.ReloadDocuments();

        return true;
    }

    MaterialEditParamType MaterialEditor::TypeOf(ncl_material::ParamType type)
    {
        switch (type)
        {
            case ncl_material::ParamType::Vec4:
                return MaterialEditParamType::Vec4;
            case ncl_material::ParamType::TextureRef:
                return MaterialEditParamType::Texture;
            default:
                return MaterialEditParamType::Scalar;
        }
    }
} // namespace render
