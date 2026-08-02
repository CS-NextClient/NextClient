#include "engine.h"

#include <memory>
#include <string>
#include <vector>

#include "console/console.h"
#include "graphics/render/MaterialManifests.h"
#include "graphics/render/ShaderManager.h"

namespace
{
    struct Entry
    {
        std::string program;
        ncl_material::ProgramManifest manifest;
        bool ok;
    };

    std::vector<std::unique_ptr<Entry>> g_Manifests;
}

const ncl_material::ProgramManifest* render::Manifest_Get(const std::string& program)
{
    for (const std::unique_ptr<Entry>& entry : g_Manifests)
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
    g_Manifests.push_back(std::move(entry));

    std::string json;
    if (!Shaders_LoadSource((program + ".json").c_str(), json))
    {
        // Not an error: a program is free to declare nothing, and then a material for it
        // is simply not checked.
        return nullptr;
    }

    ncl_utils::ResultT<ncl_material::ProgramManifest> parsed = ncl_material::ParseProgramManifest(json);

    if (parsed.has_error())
    {
        Con_Printf("render: manifest of program %s: %s\n", program.c_str(), parsed.error_str().c_str());
        return nullptr;
    }

    if (parsed->program != program)
    {
        Con_Printf(
            "render: manifest of program %s names \"%s\" instead\n",
            program.c_str(),
            parsed->program.c_str()
        );
        return nullptr;
    }

    stored.manifest = *parsed;
    stored.ok = true;

    // Only because the manifest names it. Picking a file up by the program's own name would
    // let one dropped beside a program redefine another program's tokens by being there.
    if (!stored.manifest.strings.empty() && g_pLocalize != nullptr)
    {
        g_pLocalize->AddFile(g_pFileSystem, stored.manifest.strings.c_str());
    }

    return &stored.manifest;
}

void render::Manifests_Clear()
{
    g_Manifests.clear();
}
