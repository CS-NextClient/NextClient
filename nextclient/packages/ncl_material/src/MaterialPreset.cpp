#include "ncl_material/MaterialPreset.h"

#include <algorithm>
#include <string>
#include <vector>

#include <tao/json.hpp>

#include "parse_support.h"
#include "write_support.h"

namespace ncl_material
{
    namespace
    {


        constexpr const char* kTextureExtensions[] = {".png", ".tga"};

        bool HasAdmissibleExtension(std::string_view path)
        {
            for (const char* extension : kTextureExtensions)
            {
                std::string_view suffix(extension);

                if (path.size() > suffix.size() && EqualsIgnoreCaseAscii(path.substr(path.size() - suffix.size()), suffix))
                {
                    return true;
                }
            }

            return false;
        }

        void CollectTexturePaths(
            const Material& material,
            const std::string& context,
            const PresetBudget& budget,
            std::vector<std::string>& paths
        )
        {
            for (const Param& param : material.params)
            {
                if (param.value.type != ParamType::TextureRef)
                {
                    continue;
                }

                const std::string& path = param.value.texture_ref;

                if (!IsAdmissibleTexturePath(path, budget.max_path_length))
                {
                    throw ncl_material::detail::ParseError(
                        context + ".params[\"" + param.name + "\"]: \"" + path + "\" is not an admissible texture path"
                    );
                }

                if (std::find(paths.begin(), paths.end(), path) == paths.end())
                {
                    paths.push_back(path);
                }
            }

            if (paths.size() > budget.max_textures)
            {
                throw ncl_material::detail::ParseError(
                    "preset: too many distinct textures, limit is " + std::to_string(budget.max_textures)
                );
            }
        }

        PresetMaterial ParsePresetMaterial(
            const tao::json::value& value,
            const std::string& context,
            const PresetBudget& budget,
            std::vector<std::string>& paths
        )
        {
            ncl_material::detail::RequireObject(value, context);

            PresetMaterial material;

            material.name =
                ncl_material::detail::RequireNonEmptyString(ncl_material::detail::RequireMember(value, "name", context), context + ".name");
            material.texture_name = ncl_material::detail::RequireNonEmptyString(
                ncl_material::detail::RequireMember(value, "texture", context), context + ".texture"
            );

            material.material =
                ncl_material::detail::ParseMaterial(ncl_material::detail::RequireMember(value, "material", context), context + ".material");
            CollectTexturePaths(material.material, context + ".material", budget, paths);

            return material;
        }

        MaterialPreset ParsePreset(const tao::json::value& root, const PresetBudget& budget)
        {
            ncl_material::detail::RequireObject(root, "preset");

            MaterialPreset preset;
            preset.version =
                ncl_material::detail::RequireInteger(ncl_material::detail::RequireMember(root, "version", "preset"), "preset.version");

            if (preset.version != kPresetVersion)
            {
                throw ncl_material::detail::ParseError(
                    "preset.version: unsupported version " + std::to_string(preset.version) + ", expected " + std::to_string(kPresetVersion)
                );
            }

            const tao::json::value& materials = ncl_material::detail::RequireMember(root, "materials", "preset");

            if (!materials.is_array())
            {
                throw ncl_material::detail::ParseError("preset.materials: expected an array");
            }

            const auto& items = materials.get_array();

            if (items.size() > budget.max_materials)
            {
                throw ncl_material::detail::ParseError(
                    "preset.materials: too many materials, limit is " + std::to_string(budget.max_materials)
                );
            }

            std::vector<std::string> paths;

            for (size_t i = 0; i < items.size(); i++)
            {
                std::string context = "preset.materials[" + std::to_string(i) + "]";
                PresetMaterial material = ParsePresetMaterial(items[i], context, budget, paths);

                // A name is what a plugin writes, so two of the same is a mistake a plugin
                // author would spend an evening on: one of them would simply never apply.
                if (preset.FindByName(material.name) != nullptr)
                {
                    throw ncl_material::detail::ParseError(context + ".name: duplicate name \"" + material.name + "\"");
                }

                preset.materials.push_back(std::move(material));
            }

            return preset;
        }
    } // namespace

    const PresetMaterial* MaterialPreset::FindByName(std::string_view name) const
    {
        if (name.empty())
        {
            return nullptr;
        }

        for (const PresetMaterial& material : materials)
        {
            // Case insensitive for the same reason texture names are: a plugin author
            // types this by hand, and the file was written by hand too.
            if (EqualsIgnoreCaseAscii(material.name, name))
            {
                return &material;
            }
        }

        return nullptr;
    }

    bool IsAdmissibleRelativePath(std::string_view path, size_t max_length)
    {
        if (path.empty() || path.size() > max_length)
        {
            return false;
        }

        // A drive letter, a stream name or a root anchor all leave the game directory;
        // so does a backslash, which the filesystem would accept as a separator and the
        // segment scan below would not see.
        if (path.front() == '/' || path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos)
        {
            return false;
        }

        for (char c : path)
        {
            if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) == 0x7f)
            {
                return false;
            }
        }

        size_t begin = 0;

        while (begin <= path.size())
        {
            size_t end = path.find('/', begin);

            if (end == std::string_view::npos)
            {
                end = path.size();
            }

            std::string_view segment = path.substr(begin, end - begin);

            // "." and an empty segment are rejected rather than folded away: accepting
            // them would mean the client and the server disagree on what the path is.
            if (segment.empty() || segment == "." || segment == "..")
            {
                return false;
            }

            begin = end + 1;
        }

        return true;
    }

    bool IsAdmissibleTexturePath(std::string_view path, size_t max_length)
    {
        return IsAdmissibleRelativePath(path, max_length) && HasAdmissibleExtension(path);
    }

    std::string WritePresetMaterial(const PresetMaterial& material)
    {
        tao::json::value result = tao::json::empty_object;
        result["name"] = material.name;
        result["texture"] = material.texture_name;
        result["material"] = ncl_material::detail::WriteMaterial(material.material);

        return tao::json::to_string(result, 2);
    }

    ncl_utils::ResultT<MaterialPreset> ParseMaterialPreset(std::string_view json, const PresetBudget& budget)
    {
        try
        {
            tao::json::value root = tao::json::from_string(std::string(json));

            return ParsePreset(root, budget);
        }
        catch (const ncl_material::detail::ParseError& e)
        {
            return ncl_utils::ResultError(e.what());
        }
        catch (const std::exception& e)
        {
            return ncl_utils::ResultError(std::string("malformed JSON: ") + e.what());
        }
    }
} // namespace ncl_material
