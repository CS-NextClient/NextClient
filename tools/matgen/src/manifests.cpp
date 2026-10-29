#include "manifests.h"

#include <map>

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(matgen_rc);

namespace matgen
{
    namespace
    {
        std::map<std::string, ncl_material::ProgramManifest> g_Manifests;
    }

    const ncl_material::ProgramManifest* FindManifest(const std::string& program, std::string& error)
    {
        error.clear();

        std::map<std::string, ncl_material::ProgramManifest>::iterator cached = g_Manifests.find(program);

        if (cached != g_Manifests.end())
        {
            return &cached->second;
        }

        cmrc::embedded_filesystem fs = cmrc::matgen_rc::get_filesystem();
        std::string path = "assets/shaders/" + program + ".json";

        if (!fs.is_file(path))
        {
            return nullptr;
        }

        cmrc::file file = fs.open(path);
        ncl_utils::ResultT<ncl_material::ProgramManifest> parsed =
            ncl_material::ParseProgramManifest(std::string_view(file.begin(), file.size()));

        if (parsed.has_error())
        {
            error = parsed.error_str();

            return nullptr;
        }

        return &g_Manifests.emplace(program, std::move(*parsed)).first->second;
    }
} // namespace matgen
