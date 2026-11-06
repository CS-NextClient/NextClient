#pragma once

#include <string>

namespace matgen
{
    struct CheckOptions
    {
        // Root the paths inside a material are relative to, which is the mod directory the
        // client resolves them against.
        std::string game_dir;
    };

    int RunCheck(const std::string& material_path, const CheckOptions& options);
} // namespace matgen
