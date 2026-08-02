#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

#include <tao/json.hpp>

#include "ncl_material/MaterialDoc.h"

namespace ncl_material::detail
{
    class ParseError : public std::runtime_error
    {
    public:
        explicit ParseError(const std::string& message) :
            std::runtime_error(message)
        { }
    };

    inline void RequireObject(const tao::json::value& value, const std::string& context)
    {
        if (!value.is_object())
        {
            throw ParseError(context + ": expected an object");
        }
    }

    inline const tao::json::value& RequireMember(const tao::json::value& object, const char* key, const std::string& context)
    {
        const tao::json::value* member = object.find(key);
        if (member == nullptr)
        {
            throw ParseError(context + ": missing required key \"" + key + "\"");
        }

        return *member;
    }

    inline float RequireNumber(const tao::json::value& value, const std::string& context)
    {
        if (!value.is_number())
        {
            throw ParseError(context + ": expected a number");
        }

        return static_cast<float>(value.as<double>());
    }

    inline std::string RequireNonEmptyString(const tao::json::value& value, const std::string& context)
    {
        if (!value.is_string_type())
        {
            throw ParseError(context + ": expected a string");
        }

        std::string result = value.as<std::string>();
        if (result.empty())
        {
            throw ParseError(context + ": must not be empty");
        }

        return result;
    }

    inline std::array<float, 4> RequireVec4(const tao::json::value& value, const std::string& context)
    {
        if (!value.is_array())
        {
            throw ParseError(context + ": expected an array of 4 numbers");
        }

        const auto& items = value.get_array();
        if (items.size() != 4)
        {
            throw ParseError(context + ": expected exactly 4 numbers, got " + std::to_string(items.size()));
        }

        std::array<float, 4> result = {0.0f, 0.0f, 0.0f, 0.0f};
        for (size_t i = 0; i < result.size(); i++)
        {
            result[i] = RequireNumber(items[i], context + "[" + std::to_string(i) + "]");
        }

        return result;
    }

    inline ParamValue ParseParamValue(const tao::json::value& value, const std::string& context)
    {
        ParamValue result;

        if (value.is_number())
        {
            result.type = ParamType::Scalar;
            result.scalar = static_cast<float>(value.as<double>());
            return result;
        }

        if (value.is_array())
        {
            result.type = ParamType::Vec4;
            result.vec4 = RequireVec4(value, context);
            return result;
        }

        if (value.is_object())
        {
            result.type = ParamType::TextureRef;
            result.texture_ref = RequireNonEmptyString(RequireMember(value, "texture", context), context + ".texture");
            return result;
        }

        throw ParseError(context + R"(: expected a number, an array of 4 numbers or {"texture": "<path>"})");
    }

    inline OverlaySlot ParseOverlaySlot(const tao::json::value& value, const std::string& context)
    {
        RequireObject(value, context);

        OverlaySlot slot;
        slot.name = RequireNonEmptyString(RequireMember(value, "name", context), context + ".name");
        slot.rect = RequireVec4(RequireMember(value, "rect", context), context + ".rect");

        for (float coord : slot.rect)
        {
            if (coord < 0.0f || coord > 1.0f)
            {
                throw ParseError(context + ".rect: coordinates must lie within [0, 1]");
            }
        }

        if (slot.rect[0] >= slot.rect[2] || slot.rect[1] >= slot.rect[3])
        {
            throw ParseError(context + ".rect: expected u0 < u1 and v0 < v1");
        }

        const tao::json::value* rotation = value.find("rotation");
        if (rotation != nullptr)
        {
            slot.rotation = RequireNumber(*rotation, context + ".rotation");
        }

        return slot;
    }

    inline void ParseParams(const tao::json::value& value, const std::string& context, Material& material)
    {
        RequireObject(value, context);

        const auto& params_object = value.get_object();
        if (params_object.size() > kMaxParams)
        {
            throw ParseError(context + ": too many parameters, limit is " + std::to_string(kMaxParams));
        }

        for (const auto& [name, param_value] : params_object)
        {
            if (name.empty())
            {
                throw ParseError(context + ": parameter name must not be empty");
            }

            std::string param_context = context + "[\"" + name + "\"]";
            material.params.push_back(Param { name, ParseParamValue(param_value, param_context) });
        }
    }

    inline void ParseOverlays(const tao::json::value& value, const std::string& context, Material& material)
    {
        if (!value.is_array())
        {
            throw ParseError(context + ": expected an array");
        }

        const auto& items = value.get_array();
        if (items.size() > kMaxOverlaySlots)
        {
            throw ParseError(context + ": too many overlay slots, limit is " + std::to_string(kMaxOverlaySlots));
        }

        for (size_t i = 0; i < items.size(); i++)
        {
            std::string slot_context = context + "[" + std::to_string(i) + "]";
            OverlaySlot slot = ParseOverlaySlot(items[i], slot_context);

            if (material.FindOverlaySlot(slot.name) != nullptr)
            {
                throw ParseError(slot_context + ": duplicate slot name \"" + slot.name + "\"");
            }

            material.overlays.push_back(std::move(slot));
        }
    }

    inline Material ParseMaterial(const tao::json::value& value, const std::string& context)
    {
        RequireObject(value, context);

        Material material;
        material.program = RequireNonEmptyString(RequireMember(value, "program", context), context + ".program");

        const tao::json::value* params = value.find("params");
        if (params != nullptr)
        {
            ParseParams(*params, context + ".params", material);
        }

        const tao::json::value* overlays = value.find("overlays");
        if (overlays != nullptr)
        {
            ParseOverlays(*overlays, context + ".overlays", material);
        }

        return material;
    }
}
