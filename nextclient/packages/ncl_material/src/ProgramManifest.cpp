#include "ncl_material/ProgramManifest.h"

#include <cmath>
#include <stdexcept>
#include <string>

#include <tao/json.hpp>

#include "ncl_material/MaterialPreset.h"

namespace ncl_material
{
    namespace
    {
        // The tokens live in a file the localisation system reads, so what the manifest may
        // name is constrained the way a texture path is: relative, no traversal, and an
        // extension that system actually reads.
        constexpr size_t kMaxStringsPathLength = 96;

        class ParseError : public std::runtime_error
        {
        public:
            explicit ParseError(const std::string& message) :
                std::runtime_error(message)
            { }
        };

        const tao::json::value& RequireMember(const tao::json::value& object, const char* key, const std::string& context)
        {
            const tao::json::value* member = object.find(key);
            if (member == nullptr)
            {
                throw ParseError(context + ": missing required key \"" + key + "\"");
            }

            return *member;
        }

        double RequireNumber(const tao::json::value& value, const std::string& context)
        {
            if (!value.is_number())
            {
                throw ParseError(context + ": expected a number");
            }

            return value.as<double>();
        }

        std::string RequireNonEmptyString(const tao::json::value& value, const std::string& context)
        {
            if (!value.is_string_type())
            {
                throw ParseError(context + ": expected a string");
            }

            std::string result = value.as<std::string>();
            if (result.empty())
            {
                throw ParseError(context + ": must not be empty");
            }

            return result;
        }

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

            if (const tao::json::value* quantum = value.find("quantum"))
            {
                schema.quantum = static_cast<float>(RequireNumber(*quantum, context + ".quantum"));

                if (schema.quantum <= 0.0f)
                {
                    throw ParseError(context + ".quantum: must be greater than zero");
                }

                if (schema.type != ParamType::Scalar)
                {
                    throw ParseError(context + ".quantum: only a scalar can be quantised");
                }
            }

            if (schema.instance && schema.type == ParamType::Scalar && schema.quantum <= 0.0f)
            {
                throw ParseError(context + ": an instance scalar must declare a quantum, or its cached bakes are unbounded");
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
                // vec4 has one per component, which is what bounds a colour.
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
                        float component = schema.type == ParamType::Vec4
                            ? schema.default_value.vec4[i]
                            : schema.default_value.scalar;

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

            if (!widget.empty())
            {
                if (widget != "color")
                {
                    throw ParseError(context + ".widget: unknown widget \"" + widget + "\", expected color");
                }

                // Four numbers are what a colour is here, so anything else asking for a
                // colour control is asking for a control that cannot carry its value.
                if (schema.type != ParamType::Vec4)
                {
                    throw ParseError(context + ".widget: only a vec4 can be edited as a colour");
                }

                schema.widget = ParamWidget::Color;
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

            // Unnamed and unlabelled, so a display shows it as a run of parameters rather
            // than as a section called something the manifest never said.
            if (!rest.params.empty())
            {
                manifest.groups.push_back(std::move(rest));
            }
        }
    }

    const std::string& ParamSchema::label_or_name() const
    {
        return label.empty() ? name : label;
    }

    const std::string& ParamGroup::label_or_name() const
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
            manifest.version = static_cast<int>(RequireNumber(RequireMember(root, "version", "manifest"), "manifest.version"));

            if (manifest.version != kManifestVersion)
            {
                throw ParseError(
                    "manifest.version: unsupported version " + std::to_string(manifest.version) +
                    ", expected " + std::to_string(kManifestVersion)
                );
            }

            manifest.program = RequireNonEmptyString(RequireMember(root, "program", "manifest"), "manifest.program");

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
                float max_overlays = RequireNumber(RequireMember(*overlays, "max", "manifest.overlays"), "manifest.overlays.max");

                // Checked like the table's entry ids are: a negative value converts to a
                // huge unsigned one and removes the very ceiling this field declares, and a
                // fractional one would be truncated without saying so.
                if (max_overlays < 0.0f || max_overlays != static_cast<float>(static_cast<uint32_t>(max_overlays)) ||
                    static_cast<uint32_t>(max_overlays) > kMaxOverlaySlots)
                {
                    throw ParseError(
                        "manifest.overlays.max: expected a whole number in [0, " + std::to_string(kMaxOverlaySlots) + "]"
                    );
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

            if (schema->has_range)
            {
                // Every component of a vec4, so a colour bounded to 0..1 is bounded in all
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
                    return ncl_utils::ResultError(
                        "parameter \"" + param.name + "\" is outside the range the program declares"
                    );
                }
            }

            if (schema->quantum > 0.0f)
            {
                // Rejected rather than snapped: silently moving an authored value hides a
                // mistake, and the grid exists so cached bakes stay countable.
                float steps = param.value.scalar / schema->quantum;
                if (std::fabs(steps - std::round(steps)) > 1e-3f)
                {
                    return ncl_utils::ResultError(
                        "parameter \"" + param.name + "\" is not a multiple of its quantum " + std::to_string(schema->quantum)
                    );
                }
            }
        }

        if (material.overlays.size() > manifest.max_overlays)
        {
            return ncl_utils::ResultError(
                "material declares " + std::to_string(material.overlays.size()) +
                " overlay slots but the program allows " + std::to_string(manifest.max_overlays)
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

            material.params.push_back(Param {schema.name, schema.default_value});
        }
    }
}
