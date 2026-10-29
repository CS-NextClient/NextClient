#include "ncl_material/ProgramManifest.h"

#include <cmath>
#include <string>

#include <tao/json.hpp>

#include "parse_support.h"
#include "ncl_material/MaterialPreset.h"

namespace ncl_material
{
    namespace
    {
        // The tokens live in a file the localization system reads, so what the manifest may
        // name is constrained the way a texture path is: relative, no traversal, and an
        // extension that system actually reads.
        constexpr size_t kMaxStringsPathLength = 96;
        constexpr size_t kMaxRenderContractLength = 64;

        using detail::ParseError;
        using detail::RequireInteger;
        using detail::RequireMember;
        using detail::RequireNonEmptyString;
        using detail::RequireNumber;

        std::string ParseOptionalString(const tao::json::value& object, const char* key, const std::string& context)
        {
            const tao::json::value* member = object.find(key);

            if (member == nullptr)
            {
                return {};
            }

            return RequireNonEmptyString(*member, context + "." + key);
        }

        ParamType ParseType(const std::string& name, const std::string& context)
        {
            if (name == "scalar")
            {
                return ParamType::Scalar;
            }

            if (name == "vec4")
            {
                return ParamType::Vec4;
            }

            if (name == "texture")
            {
                return ParamType::TextureRef;
            }

            throw ParseError(context + ": unknown type \"" + name + "\", expected scalar, vec4 or texture");
        }

        ParamValue ParseDefault(const tao::json::value& value, ParamType type, const std::string& context)
        {
            ParamValue result;
            result.type = type;

            if (type == ParamType::Scalar)
            {
                result.scalar = static_cast<float>(RequireNumber(value, context));

                return result;
            }

            if (type == ParamType::Vec4)
            {
                if (!value.is_array() || value.get_array().size() != 4)
                {
                    throw ParseError(context + ": expected exactly 4 numbers");
                }

                const auto& items = value.get_array();

                for (size_t i = 0; i < 4; i++)
                {
                    result.vec4[i] = static_cast<float>(RequireNumber(items[i], context + "[" + std::to_string(i) + "]"));
                }

                return result;
            }

            result.texture_ref = RequireNonEmptyString(value, context);

            return result;
        }

        ParamSchema ParseParam(const std::string& name, const tao::json::value& value, const std::string& context)
        {
            if (!value.is_object())
            {
                throw ParseError(context + ": expected an object");
            }

            ParamSchema schema;
            schema.name = name;
            schema.type = ParseType(RequireNonEmptyString(RequireMember(value, "type", context), context + ".type"), context + ".type");

            if (const tao::json::value* instance = value.find("instance"))
            {
                if (!instance->is_boolean())
                {
                    throw ParseError(context + ".instance: expected true or false");
                }

                schema.instance = instance->get_boolean();
            }

            const tao::json::value* min_value = value.find("min");
            const tao::json::value* max_value = value.find("max");

            if ((min_value == nullptr) != (max_value == nullptr))
            {
                throw ParseError(context + ": declare both \"min\" and \"max\" or neither");
            }

            if (min_value != nullptr)
            {
                // A texture reference has no ordering, so a range over it means nothing; a
                // vec4 has one per component, which is what bounds a color.
                if (schema.type == ParamType::TextureRef)
                {
                    throw ParseError(context + ": a texture cannot declare a range");
                }

                schema.has_range = true;
                schema.min_value = static_cast<float>(RequireNumber(*min_value, context + ".min"));
                schema.max_value = static_cast<float>(RequireNumber(*max_value, context + ".max"));

                if (!(schema.min_value < schema.max_value))
                {
                    throw ParseError(context + ": expected \"min\" below \"max\"");
                }
            }

            if (const tao::json::value* fallback = value.find("default"))
            {
                schema.default_value = ParseDefault(*fallback, schema.type, context + ".default");
                schema.has_default = true;

                // Held to the schema's own range. A default outside it is a value a material
                // is given by the parameter that declares the values are not allowed there,
                // and the two rules are written a dozen lines apart in the same object.
                if (schema.has_range)
                {
                    int components = schema.type == ParamType::Vec4 ? 4 : 1;

                    for (int i = 0; i < components; i++)
                    {
                        float component = schema.type == ParamType::Vec4 ? schema.default_value.vec4[i] : schema.default_value.scalar;

                        if (component < schema.min_value || component > schema.max_value)
                        {
                            throw ParseError(context + ".default: outside the declared range");
                        }
                    }
                }
            }

            schema.label = ParseOptionalString(value, "label", context);
            schema.hint = ParseOptionalString(value, "hint", context);

            std::string widget = ParseOptionalString(value, "widget", context);

            if (widget == "color")
            {
                if (schema.type != ParamType::Vec4)
                {
                    throw ParseError(context + ".widget: only a vec4 can be edited as a color");
                }

                schema.widget = ParamWidget::Color;
            }
            else if (widget == "choice")
            {
                if (schema.type != ParamType::Scalar)
                {
                    throw ParseError(context + ".widget: only a scalar can be edited as a choice");
                }

                const tao::json::value& choices = RequireMember(value, "choices", context);
                if (!choices.is_array() || choices.get_array().size() < 2 || choices.get_array().size() > kMaxSchemaChoices)
                {
                    throw ParseError(context + ".choices: expected between 2 and " + std::to_string(kMaxSchemaChoices) + " labels");
                }

                for (const tao::json::value& choice : choices.get_array())
                {
                    schema.choices.push_back(RequireNonEmptyString(choice, context + ".choices"));
                }

                if (!schema.has_range || RequireNumber(*min_value, context + ".min") != 0.0 ||
                    RequireNumber(*max_value, context + ".max") != static_cast<double>(schema.choices.size() - 1))
                {
                    throw ParseError(context + ": a choice must declare the range 0..choices.size()-1");
                }

                const double default_index = RequireNumber(RequireMember(value, "default", context), context + ".default");
                if (!std::isfinite(default_index) || default_index != std::round(default_index) || default_index < 0.0 ||
                    default_index >= static_cast<double>(schema.choices.size()))
                {
                    throw ParseError(context + ".default: expected an integer choice index in the declared range");
                }

                schema.widget = ParamWidget::Choice;
            }
            else if (!widget.empty())
            {
                throw ParseError(context + ".widget: unknown widget \"" + widget + "\", expected color or choice");
            }

            if (value.find("choices") != nullptr && schema.widget != ParamWidget::Choice)
            {
                throw ParseError(context + ".choices: labels require the choice widget");
            }

            return schema;
        }

        size_t IndexOfParam(const ProgramManifest& manifest, std::string_view name)
        {
            for (size_t i = 0; i < manifest.parameters.size(); i++)
            {
                if (manifest.parameters[i].name == name)
                {
                    return i;
                }
            }

            return manifest.parameters.size();
        }

        void ParseGroups(const tao::json::value& root, ProgramManifest& manifest)
        {
            std::vector<bool> claimed(manifest.parameters.size(), false);

            if (const tao::json::value* groups = root.find("groups"))
            {
                if (!groups->is_array())
                {
                    throw ParseError("manifest.groups: expected an array");
                }

                for (const tao::json::value& entry : groups->get_array())
                {
                    if (!entry.is_object())
                    {
                        throw ParseError("manifest.groups: expected an object");
                    }

                    ParamGroup group;
                    group.name = RequireNonEmptyString(RequireMember(entry, "name", "manifest.groups"), "manifest.groups.name");

                    std::string context = "manifest.groups[\"" + group.name + "\"]";
                    group.label = ParseOptionalString(entry, "label", context);

                    const tao::json::value& params = RequireMember(entry, "params", context);

                    if (!params.is_array() || params.get_array().empty())
                    {
                        throw ParseError(context + ".params: expected a non-empty array of parameter names");
                    }

                    for (const tao::json::value& member : params.get_array())
                    {
                        std::string name = RequireNonEmptyString(member, context + ".params");
                        size_t index = IndexOfParam(manifest, name);

                        // Refused rather than dropped into the trailing group: a misspelt
                        // name would otherwise still lay out, with one parameter quietly
                        // sitting somewhere its author did not put it.
                        if (index == manifest.parameters.size())
                        {
                            throw ParseError(context + ".params: \"" + name + "\" is not a declared parameter");
                        }

                        if (claimed[index])
                        {
                            throw ParseError(context + ".params: \"" + name + "\" is already in a group");
                        }

                        claimed[index] = true;
                        group.params.push_back(index);
                    }

                    manifest.groups.push_back(std::move(group));
                }
            }

            ParamGroup rest;

            for (size_t i = 0; i < manifest.parameters.size(); i++)
            {
                if (!claimed[i])
                {
                    rest.params.push_back(i);
                }
            }

            // Unnamed and unlabeled, so a display shows it as a run of parameters rather
            // than as a section called something the manifest never said.
            if (!rest.params.empty())
            {
                manifest.groups.push_back(std::move(rest));
            }
        }
    } // namespace

    const std::string& ParamSchema::get_label_or_name() const
    {
        return label.empty() ? name : label;
    }

    const std::string& ParamGroup::get_label_or_name() const
    {
        return label.empty() ? name : label;
    }

    const ParamSchema* ProgramManifest::FindParam(std::string_view name) const
    {
        for (const ParamSchema& schema : parameters)
        {
            if (schema.name == name)
            {
                return &schema;
            }
        }

        return nullptr;
    }

    ncl_utils::ResultT<ProgramManifest> ParseProgramManifest(std::string_view json)
    {
        try
        {
            tao::json::value root = tao::json::from_string(std::string(json));

            if (!root.is_object())
            {
                throw ParseError("manifest: expected an object");
            }

            ProgramManifest manifest;
            manifest.version = RequireInteger(RequireMember(root, "version", "manifest"), "manifest.version");

            if (manifest.version != kManifestVersion)
            {
                throw ParseError(
                    "manifest.version: unsupported version " + std::to_string(manifest.version) + ", expected " +
                    std::to_string(kManifestVersion)
                );
            }

            manifest.program = RequireNonEmptyString(RequireMember(root, "program", "manifest"), "manifest.program");

            manifest.render_contract = ParseOptionalString(root, "render_contract", "manifest");

            if (manifest.render_contract.size() > kMaxRenderContractLength)
            {
                throw ParseError("manifest.render_contract: maximum length is 64 bytes");
            }

            const tao::json::value& parameters = RequireMember(root, "parameters", "manifest");

            if (!parameters.is_object())
            {
                throw ParseError("manifest.parameters: expected an object");
            }

            const auto& entries = parameters.get_object();

            if (entries.size() > kMaxSchemaParams)
            {
                throw ParseError("manifest.parameters: too many parameters, limit is " + std::to_string(kMaxSchemaParams));
            }

            for (const auto& [name, value] : entries)
            {
                if (name.empty())
                {
                    throw ParseError("manifest.parameters: parameter name must not be empty");
                }

                manifest.parameters.push_back(ParseParam(name, value, "manifest.parameters[\"" + name + "\"]"));
            }

            // Each texture parameter takes a texture unit of its own at draw time, and the
            // unit above the ceiling would leave its sampler reading unit zero, which is the
            // model's own skin. Refused here rather than dropped there.
            size_t textures = 0;

            for (const ParamSchema& schema : manifest.parameters)
            {
                textures += schema.type == ParamType::TextureRef ? 1 : 0;
            }

            if (textures > kMaxSchemaTextures)
            {
                throw ParseError(
                    "manifest.parameters: " + std::to_string(textures) + " texture parameters, limit is " +
                    std::to_string(kMaxSchemaTextures)
                );
            }

            ParseGroups(root, manifest);

            manifest.strings = ParseOptionalString(root, "strings", "manifest");

            if (!manifest.strings.empty())
            {
                bool is_text_file = manifest.strings.size() > 4 &&
                                    EqualsIgnoreCaseAscii(std::string_view(manifest.strings).substr(manifest.strings.size() - 4), ".txt");

                if (!IsAdmissibleRelativePath(manifest.strings, kMaxStringsPathLength) || !is_text_file)
                {
                    throw ParseError("manifest.strings: expected a relative path to a .txt file inside the game directory");
                }
            }

            if (const tao::json::value* overlays = root.find("overlays"))
            {
                double max_overlays = RequireNumber(RequireMember(*overlays, "max", "manifest.overlays"), "manifest.overlays.max");

                if (max_overlays < 0.0 || max_overlays > kMaxOverlaySlots || max_overlays != std::trunc(max_overlays))
                {
                    throw ParseError("manifest.overlays.max: expected a whole number in [0, " + std::to_string(kMaxOverlaySlots) + "]");
                }

                manifest.max_overlays = static_cast<uint32_t>(max_overlays);
            }

            return std::move(manifest);
        }
        catch (const ParseError& e)
        {
            return ncl_utils::ResultError(e.what());
        }
        catch (const std::exception& e)
        {
            return ncl_utils::ResultError(std::string("malformed JSON: ") + e.what());
        }
    }

    ncl_utils::Result<> Validate(const Material& material, const ProgramManifest& manifest)
    {
        if (material.program != manifest.program)
        {
            return ncl_utils::ResultError(
                "material names program \"" + material.program + "\" but the manifest describes \"" + manifest.program + "\""
            );
        }

        for (const Param& param : material.params)
        {
            const ParamSchema* schema = manifest.FindParam(param.name);

            if (schema == nullptr)
            {
                return ncl_utils::ResultError("parameter \"" + param.name + "\" is not declared by the program");
            }

            if (schema->type != param.value.type)
            {
                return ncl_utils::ResultError("parameter \"" + param.name + "\" has the wrong type for the program");
            }

            if (param.value.type == ParamType::Scalar && !std::isfinite(param.value.scalar))
            {
                return ncl_utils::ResultError("parameter \"" + param.name + "\" must be finite");
            }

            if (param.value.type == ParamType::Vec4)
            {
                for (float component : param.value.vec4)
                {
                    if (!std::isfinite(component))
                    {
                        return ncl_utils::ResultError("parameter \"" + param.name + "\" must have finite components");
                    }
                }
            }

            if (schema->widget == ParamWidget::Choice && param.value.scalar != std::trunc(param.value.scalar))
            {
                return ncl_utils::ResultError("parameter \"" + param.name + "\" must be an integer choice index");
            }

            if (schema->has_range)
            {
                // Every component of a vec4, so a color bounded to 0..1 is bounded in all
                // four rather than only where an editor happened to look.
                bool outside = false;

                if (param.value.type == ParamType::Scalar)
                {
                    outside = param.value.scalar < schema->min_value || param.value.scalar > schema->max_value;
                }
                else if (param.value.type == ParamType::Vec4)
                {
                    for (float component : param.value.vec4)
                    {
                        outside = outside || component < schema->min_value || component > schema->max_value;
                    }
                }

                if (outside)
                {
                    return ncl_utils::ResultError("parameter \"" + param.name + "\" is outside the range the program declares");
                }
            }
        }

        if (material.overlays.size() > manifest.max_overlays)
        {
            return ncl_utils::ResultError(
                "material declares " + std::to_string(material.overlays.size()) + " overlay slots but the program allows " +
                std::to_string(manifest.max_overlays)
            );
        }

        return {};
    }

    void ApplyDefaults(Material& material, const ProgramManifest& manifest)
    {
        for (const ParamSchema& schema : manifest.parameters)
        {
            if (!schema.has_default || material.FindParam(schema.name) != nullptr)
            {
                continue;
            }

            material.params.push_back(Param{schema.name, schema.default_value});
        }
    }
} // namespace ncl_material
