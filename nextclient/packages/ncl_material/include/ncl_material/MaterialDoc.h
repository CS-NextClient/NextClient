#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ncl_material
{
    inline constexpr int kDocVersion = 1;
    inline constexpr size_t kMaxEntries = 64;
    inline constexpr size_t kMaxParams = 32;
    inline constexpr size_t kMaxOverlaySlots = 8;

    // Distinct texture files one document may pull in. The server table has the same
    // ceiling for the same reason: a document is installed content, and every reference in
    // it is a decode and an upload the client is told to perform.
    inline constexpr size_t kMaxDocTextures = 64;

    enum class ParamType
    {
        Scalar,
        Vec4,
        TextureRef
    };

    struct ParamValue
    {
        ParamType type = ParamType::Scalar;
        float scalar = 0.0f;
        std::array<float, 4> vec4 = {0.0f, 0.0f, 0.0f, 0.0f};
        std::string texture_ref;
    };

    struct Param
    {
        std::string name;
        ParamValue value;
    };

    // Named UV rectangle declared by the model author; an overlay image supplied later
    // is placed into it. The client attaches no meaning to a slot beyond its rectangle.
    struct OverlaySlot
    {
        std::string name;
        std::array<float, 4> rect = {0.0f, 0.0f, 1.0f, 1.0f}; // u0, v0, u1, v1
        float rotation = 0.0f;                                // degrees
    };

    struct Material
    {
        std::string program;
        std::vector<Param> params;
        std::vector<OverlaySlot> overlays;

        // Parameter names are shader-side identifiers and match exactly; slot names are
        // authored alongside model data and match without regard to case.
        const ParamValue* FindParam(std::string_view name) const;
        const OverlaySlot* FindOverlaySlot(std::string_view name) const;
    };

    struct MaterialEntry
    {
        std::string texture_name;
        Material material;
    };

    struct MaterialDoc
    {
        int version = kDocVersion;
        std::vector<MaterialEntry> entries;

        const Material* FindByTexture(std::string_view texture_name) const;
    };

    bool EqualsIgnoreCaseAscii(std::string_view left, std::string_view right);
}
