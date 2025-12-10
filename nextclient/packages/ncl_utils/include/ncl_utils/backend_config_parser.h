#pragma once
#include <string>
#include "backend_config_data/Config.h"

namespace ncl_utils
{
    backend_config_data::Config ParseBackendConfig(const std::string& config_path);
}
