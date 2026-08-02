#pragma once

#include <string>

#include "Model.h"

namespace matgen
{
    struct UvOptions
    {
        std::string out_dir;

        // 0 takes each texture's own size, which is what makes the template line up with
        // the skin an author is painting over.
        int size = 0;

        int skin_family = 0;
    };

    int RunUv(const std::string& model_path, const UvOptions& options);
}
