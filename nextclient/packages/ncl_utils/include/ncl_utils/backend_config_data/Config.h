#pragma once
#include "Payload.h"

namespace ncl_utils::backend_config_data
{
    struct Config
    {
        Payload payload;
        std::string signature;
    };
}
