#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <ncl_utils/safe_result.h>

#include "ncl_material/MaterialDoc.h"

namespace ncl_material
{
    inline constexpr int kManifestVersion = 1;
    inline constexpr size_t kMaxSchemaParams = 64;
    inline constexpr size_t kMaxSchemaChoices = 64;

    // Texture parameters are scarcer than parameters: each one takes a texture unit of its
    // own at draw time, above the unit the model's own skin occupies.
    inline constexpr size_t kMaxSchemaTextures = 8;


    // How a program would like one of its values edited. Not a role: it says nothing about
    // what the value means or what the client should do with it, only which control an
    // editor should offer. Four numbers are four numbers either way, and a program that
    // declares nothing gets the plain fields.
    enum class ParamWidget
    {
        Default,
        Color,
        Choice
    };

    // Generic material editing and validation use the declared type, instance policy, and bounds.
    // Renderer-specific semantic contracts are interpreted by their consuming subsystem.
    struct ParamSchema
    {
        std::string name;
        ParamType type = ParamType::Scalar;

        // Whether the server may override this per entity rather than per material.
        bool instance{};

        // Range the value has to lie in; for a vec4 it bounds every component. Optional,
        // and not for a texture: without it an editor has no span to lay a slider over and
        // falls back to typed numbers, which is the honest answer when the program declared
        // no range rather than an invented 0..1.
        bool has_range{};
        float min_value{};
        float max_value{};

        ParamValue default_value;
        bool has_default{};

        // What an editor shows instead of the GLSL identifier, and what it explains next to
        // it. Both optional and both usually tokens: a leading '#' is resolved against the
        // localization files by whoever displays them. Generic editors use these hints
        // without interpreting renderer-specific parameter roles.
        std::string label;
        std::string hint;

        ParamWidget widget = ParamWidget::Default;
        std::vector<std::string> choices;

        // Never empty, and never the raw token: a display that cannot resolve the token has
        // to show something an author can act on, and the identifier is that something.
        const std::string& get_label_or_name() const;
    };

    // A section of an editor's parameter list. Its members are indices into
    // ncl_material::ProgramManifest::parameters, in the order they are to be shown.
    struct ParamGroup
    {
        std::string name;
        std::string label;
        std::vector<size_t> params;

        // Empty only for the group holding what no declared group claimed, which is the one
        // case where a section header would name something the manifest never said.
        const std::string& get_label_or_name() const;
    };

    struct ProgramManifest
    {
        int version = kManifestVersion;
        std::string program;
        // Optional consumer-defined semantics; the material package validates only the field representation.
        std::string render_contract;
        std::vector<ParamSchema> parameters;

        // Every parameter appears in exactly one group. A manifest declaring no groups
        // still gets one holding all of them, so a consumer only ever walks this and never
        // has to handle the ungrouped case separately.
        std::vector<ParamGroup> groups;

        // Localization file carrying this program's tokens, empty when it uses the
        // client's own. Loaded because it is declared here and not because of where it
        // sits: picking one up by the program's name would let a file redefine another
        // program's tokens by the mere fact of being placed beside it.
        std::string strings;

        uint32_t max_overlays{};

        const ParamSchema* FindParam(std::string_view name) const;
    };

    ncl_utils::ResultT<ProgramManifest> ParseProgramManifest(std::string_view json);

    // Checks a material against the schema of the program it names. Values absent from
    // the material are not an error; the program supplies its declared default.
    ncl_utils::Result<> Validate(const Material& material, const ProgramManifest& manifest);

    // Writes the declared defaults into what the material left out, so that a resolved
    // material carries every value the program will read. Without this the promise made
    // by Validate is empty: an uploader only sends what the material lists, and a uniform
    // nobody sends reads as zero, which is a value no program asked for.
    void ApplyDefaults(Material& material, const ProgramManifest& manifest);
} // namespace ncl_material
