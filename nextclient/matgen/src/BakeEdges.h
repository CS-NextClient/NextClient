#pragma once

#include <string>

#include "Model.h"

namespace matgen
{
    struct BakeOptions
    {
        std::string out_dir;
        int size = 0;
        int skin_family = 0;
    };

    int RunBakeEdges(const std::string& model_path, const BakeOptions& options);
}
