#include "ncl_material/material_parse.h"

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


        MaterialDoc ParseDoc(const tao::json::value& root)
        {
            ncl_material::detail::RequireObject(root, "document");

            MaterialDoc doc;
            doc.version =
                ncl_material::detail::RequireInteger(ncl_material::detail::RequireMember(root, "version", "document"), "document.version");

            if (doc.version != kDocVersion)
            {
                throw ncl_material::detail::ParseError(
                    "document.version: unsupported version " + std::to_string(doc.version) + ", expected " + std::to_string(kDocVersion)
                );
            }

            const tao::json::value& materials = ncl_material::detail::RequireMember(root, "materials", "document");
            ncl_material::detail::RequireObject(materials, "document.materials");

            const auto& materials_object = materials.get_object();

            if (materials_object.size() > kMaxEntries)
            {
                throw ncl_material::detail::ParseError("document.materials: too many entries, limit is " + std::to_string(kMaxEntries));
            }

            for (const auto& [texture_name, material_value] : materials_object)
            {
                if (texture_name.empty())
                {
                    throw ncl_material::detail::ParseError("document.materials: texture name must not be empty");
                }

                std::string context = "document.materials[\"" + texture_name + "\"]";

                if (doc.FindByTexture(texture_name) != nullptr)
                {
                    throw ncl_material::detail::ParseError(context + ": duplicate texture name, names are matched without regard to case");
                }

                doc.entries.push_back(MaterialEntry{texture_name, ncl_material::detail::ParseMaterial(material_value, context)});
            }

            // Counted across the whole document and only once per distinct file: a document
            // that reuses one image should not be charged for reusing it, and the ceiling
            // exists to bound what the client is told to decode and upload.
            std::vector<std::string> paths;

            for (const MaterialEntry& entry : doc.entries)
            {
                for (const Param& param : entry.material.params)
                {
                    if (param.value.type != ParamType::TextureRef)
                    {
                        continue;
                    }

                    if (std::find(paths.begin(), paths.end(), param.value.texture_ref) == paths.end())
                    {
                        paths.push_back(param.value.texture_ref);
                    }
                }
            }

            if (paths.size() > kMaxDocTextures)
            {
                throw ncl_material::detail::ParseError(
                    "document: " + std::to_string(paths.size()) + " distinct textures, limit is " + std::to_string(kMaxDocTextures)
                );
            }

            return doc;
        }
    } // namespace

    ncl_utils::ResultT<MaterialDoc> ParseMaterialDoc(std::string_view json)
    {
        try
        {
            tao::json::value root = tao::json::from_string(std::string(json));

            return ParseDoc(root);
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

    std::string WriteMaterialDoc(const MaterialDoc& doc)
    {
        tao::json::value root = tao::json::empty_object;
        root["version"] = doc.version;

        tao::json::value materials = tao::json::empty_object;

        for (const MaterialEntry& entry : doc.entries)
        {
            materials[entry.texture_name] = ncl_material::detail::WriteMaterial(entry.material);
        }

        root["materials"] = materials;

        return tao::json::to_string(root, 2) + "\n";
    }
} // namespace ncl_material
