#include "ncl_material/MaterialParse.h"

#include <algorithm>
#include <string>
#include <vector>

#include <tao/json.hpp>

#include "ParseSupport.h"
#include "WriteSupport.h"

namespace ncl_material
{
    namespace
    {
        using namespace detail;

        MaterialDoc ParseDoc(const tao::json::value& root)
        {
            RequireObject(root, "document");

            MaterialDoc doc;
            doc.version = static_cast<int>(RequireNumber(RequireMember(root, "version", "document"), "document.version"));

            if (doc.version != kDocVersion)
            {
                throw ParseError(
                    "document.version: unsupported version " + std::to_string(doc.version) +
                    ", expected " + std::to_string(kDocVersion)
                );
            }

            const tao::json::value& materials = RequireMember(root, "materials", "document");
            RequireObject(materials, "document.materials");

            const auto& materials_object = materials.get_object();
            if (materials_object.size() > kMaxEntries)
            {
                throw ParseError("document.materials: too many entries, limit is " + std::to_string(kMaxEntries));
            }

            for (const auto& [texture_name, material_value] : materials_object)
            {
                if (texture_name.empty())
                {
                    throw ParseError("document.materials: texture name must not be empty");
                }

                std::string context = "document.materials[\"" + texture_name + "\"]";

                if (doc.FindByTexture(texture_name) != nullptr)
                {
                    throw ParseError(context + ": duplicate texture name, names are matched without regard to case");
                }

                doc.entries.push_back(MaterialEntry { texture_name, ParseMaterial(material_value, context) });
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
                throw ParseError(
                    "document: " + std::to_string(paths.size()) + " distinct textures, limit is " +
                    std::to_string(kMaxDocTextures)
                );
            }

            return doc;
        }
    }

    ncl_utils::ResultT<MaterialDoc> ParseMaterialDoc(std::string_view json)
    {
        try
        {
            tao::json::value root = tao::json::from_string(std::string(json));
            return ParseDoc(root);
        }
        catch (const detail::ParseError& e)
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
            materials[entry.texture_name] = detail::WriteMaterial(entry.material);
        }

        root["materials"] = materials;

        return tao::json::to_string(root, 2) + "\n";
    }
}
