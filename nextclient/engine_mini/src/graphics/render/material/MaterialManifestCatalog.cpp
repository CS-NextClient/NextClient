#include "graphics/render/material/MaterialManifestCatalog.h"

#include <memory>
#include <string>
#include <vector>

#include "graphics/render/ShaderManager.h"
#include "graphics/render/material/StudioProgramContract.h"

namespace render
{
    struct MaterialManifestCatalog::Entry
    {
        std::string program;
        ncl_material::ProgramManifest manifest;
        bool ok{};
    };

    MaterialManifestCatalog::MaterialManifestCatalog(ShaderManager& shaders, ManifestBackend& backend) :
        shaders_(shaders),
        backend_(backend)
    {}

    MaterialManifestCatalog::~MaterialManifestCatalog() = default;

    const ncl_material::ProgramManifest* MaterialManifestCatalog::Get(const std::string& program)
    {
        for (const std::unique_ptr<Entry>& entry : manifests_)
        {
            if (entry->program == program)
            {
                return entry->ok ? &entry->manifest : nullptr;
            }
        }

        std::unique_ptr<Entry> entry = std::make_unique<Entry>();
        entry->program = program;
        entry->ok = false;

        Entry& stored = *entry;
        manifests_.push_back(std::move(entry));

        std::string json;

        if (!shaders_.LoadSource((program + ".json").c_str(), json))
        {
            // Material admission requires a validated manifest for its program.
            return nullptr;
        }

        ncl_utils::ResultT<ncl_material::ProgramManifest> parsed = ncl_material::ParseProgramManifest(json);

        if (parsed.has_error())
        {
            backend_.ReportError(program, parsed.error_str());

            return nullptr;
        }

        if (parsed->program != program)
        {
            backend_.ReportError(program, "manifest names \"" + parsed->program + "\" instead");

            return nullptr;
        }

        std::string vertex;
        std::string fragment;
        if (!shaders_.LoadSource((program + ".vert").c_str(), vertex) || !shaders_.LoadSource((program + ".frag").c_str(), fragment))
        {
            backend_.ReportError(program, "no embedded shader pair");
            return nullptr;
        }

        const ncl_utils::Result<> contract = StudioContract_Validate(*parsed);
        if (contract.has_error())
        {
            backend_.ReportError(program, contract.error_str());
            return nullptr;
        }

        stored.manifest = *parsed;
        stored.ok = true;

        // Only because the manifest names it. Picking a file up by the program's own name would
        // let one dropped beside a program redefine another program's tokens by being there.
        if (!stored.manifest.strings.empty())
        {
            backend_.RegisterLocalization(stored.manifest.strings.c_str());
        }

        return &stored.manifest;
    }

    const ncl_material::ParamValue* MaterialManifestCatalog::GetParam(const ncl_material::Material* material, std::string_view name)
    {
        if (material == nullptr)
        {
            return nullptr;
        }

        if (const ncl_material::ParamValue* value = material->FindParam(name))
        {
            return value;
        }

        const ncl_material::ProgramManifest* manifest = Get(material->program);
        const ncl_material::ParamSchema* schema = manifest != nullptr ? manifest->FindParam(name) : nullptr;

        return schema != nullptr && schema->has_default ? &schema->default_value : nullptr;
    }
} // namespace render
