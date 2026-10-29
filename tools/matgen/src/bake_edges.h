#pragma once

#include <string>

#include "Model.h"

namespace matgen
{
    struct BakeOptions
    {
        std::string out_dir;
        int size{};
        int skin_family{};
    };

    int RunBakeEdges(const std::string& model_path, const BakeOptions& options);
} // namespace matgen
