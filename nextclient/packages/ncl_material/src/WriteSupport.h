#pragma once

#include <array>
#include <charconv>
#include <cstdlib>
#include <string>

#include <tao/json.hpp>

#include "ncl_material/MaterialDoc.h"

namespace ncl_material::detail
{
    // The document holds floats and JSON holds doubles, and widening 0.08f gives
    // 0.079999998211860657, which is what a written file would then contain. Going through
    // the shortest text that reads back as the same float keeps an authored number looking
    // like the number that was authored.
    inline double ShortestDouble(float value)
    {
        char buffer[64];
        std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer) - 1, value);
        *result.ptr = '\0';

        return std::strtod(buffer, nullptr);
    }

    inline tao::json::value WriteVec4(const std::array<float, 4>& values)
    {
        tao::json::value array = tao::json::empty_array;

        for (float value : values)
        {
            array.push_back(ShortestDouble(value));
        }

        return array;
    }

    inline tao::json::value WriteParamValue(const ParamValue& value)
    {
        if (value.type == ParamType::Scalar)
        {
            return tao::json::value(ShortestDouble(value.scalar));
        }

        if (value.type == ParamType::Vec4)
        {
            return WriteVec4(value.vec4);
        }

        tao::json::value texture = tao::json::empty_object;
        texture["texture"] = value.texture_ref;

        return texture;
    }

    inline tao::json::value WriteMaterial(const Material& material)
    {
        tao::json::value result = tao::json::empty_object;
        result["program"] = material.program;

        if (!material.params.empty())
        {
            tao::json::value params = tao::json::empty_object;

            for (const Param& param : material.params)
            {
                params[param.name] = WriteParamValue(param.value);
            }

            result["params"] = params;
        }

        if (!material.overlays.empty())
        {
            tao::json::value overlays = tao::json::empty_array;

            for (const OverlaySlot& slot : material.overlays)
            {
                tao::json::value entry = tao::json::empty_object;
                entry["name"] = slot.name;
                entry["rect"] = WriteVec4(slot.rect);
                entry["rotation"] = ShortestDouble(slot.rotation);

                overlays.push_back(std::move(entry));
            }

            result["overlays"] = overlays;
        }

        return result;
    }
}
