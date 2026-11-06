#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <ncl_utils/safe_result.h>

namespace render
{
    using ShaderSourceLoader_t = std::function<bool(std::string_view, std::string&)>;

    ncl_utils::ResultT<std::string> ShaderIncludes_Expand(std::string_view name, const ShaderSourceLoader_t& loader);
} // namespace render
