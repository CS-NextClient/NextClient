#include "MapLayerOverrides.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace
{
    constexpr std::array<std::string_view, 3> kLayerFields = {"name", "min_z", "max_z"};
    constexpr std::array<std::string_view, 3> kRegionFields = {"layer", "mins", "maxs"};
    constexpr unsigned int kAllFields = 0b111u;
    constexpr unsigned int kBoundsFields = 0b110u;

    bool Number(std::string_view token, float& value)
    {
        if (!token.empty() && token.front() == '+')
        {
            token.remove_prefix(1);

            if (token.empty() || token.front() == '-' || token.front() == '+')
            {
                return false;
            }
        }

        const char* end = token.data() + token.size();
        std::from_chars_result result = std::from_chars(token.data(), end, value);

        return result.ec == std::errc{} && result.ptr == end && std::isfinite(value);
    }

    bool Vector(const std::string& token, ncl_math::Vector3& value)
    {
        std::istringstream input(token);
        input.imbue(std::locale::classic());

        std::string component;

        for (int axis = 0; axis < 3; axis++)
        {
            if (!(input >> component) || !Number(component, value[axis]))
            {
                return false;
            }
        }

        return !(input >> component);
    }

    bool Fail(std::string& error, const std::string& message)
    {
        error = message;

        return false;
    }

    bool Name(const std::string& name)
    {
        return !name.empty() && name != "{" && name != "}" && name.find_first_not_of(" \t\r\n") != std::string::npos;
    }

    bool IsValidBounds(const ncl_math::Aabb3& bounds)
    {
        for (int axis = 0; axis < 3; axis++)
        {
            if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]) || bounds.min[axis] >= bounds.max[axis])
            {
                return false;
            }
        }

        return true;
    }

} // namespace

bool MapLayerOverrides_Validate(const RadarLayerOverrides& overrides, std::string& error)
{
    error.clear();

    std::unordered_set<std::string> names;

    for (const RadarLayerOverride& layer : overrides.layers)
    {
        if (!Name(layer.name) || !names.insert(layer.name).second)
        {
            return Fail(error, "Layer names must be nonempty and unique.");
        }

        if (!std::isfinite(layer.min_z) || !std::isfinite(layer.max_z) || layer.min_z >= layer.max_z)
        {
            return Fail(error, "Layer height ranges must be finite and nonempty.");
        }
    }

    for (size_t i = 0; i < overrides.layers.size(); i++)
    {
        for (size_t j = i + 1; j < overrides.layers.size(); j++)
        {
            const RadarLayerOverride& a = overrides.layers[i];
            const RadarLayerOverride& b = overrides.layers[j];

            if (std::max(a.min_z, b.min_z) < std::min(a.max_z, b.max_z))
            {
                return Fail(error, "Layer height ranges overlap.");
            }
        }
    }

    for (const RadarLayerRegion& region : overrides.regions)
    {
        if (!names.contains(region.layer))
        {
            return Fail(error, "Region refers to an undeclared layer: " + region.layer);
        }

        if (!IsValidBounds(region.bounds))
        {
            return Fail(error, "Region bounds must be finite and nonempty.");
        }
    }

    for (const ncl_math::Aabb3& hide : overrides.hides)
    {
        if (!IsValidBounds(hide))
        {
            return Fail(error, "Hide bounds must be finite and nonempty.");
        }
    }

    for (size_t i = 0; i < overrides.regions.size(); i++)
    {
        for (size_t j = i + 1; j < overrides.regions.size(); j++)
        {
            const RadarLayerRegion& a = overrides.regions[i];
            const RadarLayerRegion& b = overrides.regions[j];

            if (a.layer != b.layer && a.bounds.OverlapsInterior(b.bounds))
            {
                return Fail(error, "Regions of different layers overlap.");
            }
        }
    }

    return true;
}

bool MapLayerOverrides_Parse(std::span<const std::string> tokens, RadarLayerOverrides& out, std::string& error)
{
    RadarLayerOverrides parsed;
    size_t position = 0;

    auto consume = [&](const std::string& expected) {
        if (position >= tokens.size() || tokens[position] != expected)
        {
            return false;
        }

        position++;

        return true;
    };

    if (!consume("radar_layers") || !consume("{"))
    {
        return Fail(error, "Expected radar_layers block.");
    }

    while (position < tokens.size() && tokens[position] != "}")
    {
        std::string block = tokens[position++];

        if ((block != "layer" && block != "region" && block != "hide") || !consume("{"))
        {
            return Fail(error, "Expected layer, region or hide block.");
        }

        RadarLayerOverride layer;
        RadarLayerRegion region;
        ncl_math::Aabb3 hide{};
        const std::array<std::string_view, 3>& field_names = block == "layer" ? kLayerFields : kRegionFields;
        unsigned int fields = 0;

        while (position < tokens.size() && tokens[position] != "}")
        {
            std::string field = tokens[position++];
            auto field_position = std::find(field_names.begin(), field_names.end(), field);
            unsigned int field_flag = field_position == field_names.end() ? 0u : 1u << (field_position - field_names.begin());

            if ((fields & field_flag) != 0 || position >= tokens.size() || tokens[position] == "{" || tokens[position] == "}")
            {
                return Fail(error, "Duplicate field or missing value in " + block + " block.");
            }

            const std::string& value = tokens[position++];
            bool valid = false;

            if (block == "layer")
            {
                if (field == "name")
                {
                    layer.name = value;
                    valid = true;
                }
                else if (field == "min_z")
                {
                    valid = Number(value, layer.min_z);
                }
                else if (field == "max_z")
                {
                    valid = Number(value, layer.max_z);
                }
            }
            else if (block == "region" && field == "layer")
            {
                region.layer = value;
                valid = true;
            }
            else if (field == "mins")
            {
                valid = Vector(value, block == "hide" ? hide.min : region.bounds.min);
            }
            else if (field == "maxs")
            {
                valid = Vector(value, block == "hide" ? hide.max : region.bounds.max);
            }

            if (!valid)
            {
                return Fail(error, "Unknown field or invalid value: " + field);
            }

            fields |= field_flag;
        }

        if (!consume("}") || fields != (block == "hide" ? kBoundsFields : kAllFields))
        {
            return Fail(error, "Incomplete " + block + " block.");
        }

        if (block == "layer")
        {
            parsed.layers.push_back(std::move(layer));
        }
        else if (block == "region")
        {
            parsed.regions.push_back(std::move(region));
        }
        else
        {
            parsed.hides.push_back(hide);
        }
    }

    if (!consume("}") || position != tokens.size())
    {
        return Fail(error, "Missing final brace or unexpected trailing token.");
    }

    if (!MapLayerOverrides_Validate(parsed, error))
    {
        return false;
    }

    out = std::move(parsed);

    return true;
}
