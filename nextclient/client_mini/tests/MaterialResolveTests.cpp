#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

#include <ncl_material/material_parse.h>
#include <ncl_material/MaterialResolve.h>
#include <ncl_material/ProgramManifest.h>


namespace
{
    constexpr const char* kManifest = R"({
        "version": 1,
        "program": "weapon_finish",
        "parameters": {
            "base_tint":  { "type": "vec4", "default": [1, 1, 1, 1] },
            "layer_mix":  { "type": "scalar", "instance": true },
            "seed_shift": { "type": "scalar" },
            "pattern":    { "type": "texture" }
        },
        "overlays": { "max": 2 }
    })";

    ncl_material::Material MaterialFrom(const char* json)
    {
        ncl_utils::ResultT<ncl_material::MaterialDoc> doc = ncl_material::ParseMaterialDoc(json);
        EXPECT_FALSE(doc.has_error()) << doc.error_str();

        const ncl_material::Material* material = doc->FindByTexture("ak47_body.bmp");
        EXPECT_NE(material, nullptr);

        return material != nullptr ? *material : ncl_material::Material{};
    }

    ncl_material::ProgramManifest ManifestFrom(const char* json)
    {
        ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(json);
        EXPECT_FALSE(manifest.has_error()) << manifest.error_str();

        return manifest.has_error() ? ncl_material::ProgramManifest{} : *manifest;
    }
}

TEST(MaterialProgramManifest, ParsesSchema)
{
    ncl_material::ProgramManifest manifest = ManifestFrom(kManifest);

    EXPECT_EQ(manifest.program, "weapon_finish");
    EXPECT_EQ(manifest.parameters.size(), 4u);
    EXPECT_EQ(manifest.max_overlays, 2u);

    const ncl_material::ParamSchema* mix = manifest.FindParam("layer_mix");
    ASSERT_NE(mix, nullptr);
    EXPECT_EQ(mix->type, ncl_material::ParamType::Scalar);
    EXPECT_TRUE(mix->instance);

    const ncl_material::ParamSchema* tint = manifest.FindParam("base_tint");
    ASSERT_NE(tint, nullptr);
    EXPECT_TRUE(tint->has_default);
    EXPECT_FLOAT_EQ(tint->default_value.vec4[3], 1.0f);
}

TEST(MaterialProgramManifest, RejectsUnknownType)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": { "thing": { "type": "color" } }
    })";

    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(json);
    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("unknown type"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, AcceptsAConformingMaterial)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "params": { "layer_mix": 0.5, "base_tint": [1, 0.5, 0.25, 1] },
                "overlays": [ { "name": "left", "rect": [0.1, 0.2, 0.3, 0.4] } ]
            }
        }
    })");

    EXPECT_FALSE(ncl_material::Validate(material, ManifestFrom(kManifest)).has_error());
}

TEST(MaterialProgramManifest, FillsInDeclaredDefaults)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": 0.5 } }
        }
    })");

    ncl_material::ApplyDefaults(material, ManifestFrom(kManifest));

    // Absent and defaulted, which is the whole point: an uploader sends what the material
    // lists, and a uniform nobody sends reads as zero rather than as the declared value.
    const ncl_material::ParamValue* tint = material.FindParam("base_tint");
    ASSERT_NE(tint, nullptr);
    EXPECT_EQ(tint->type, ncl_material::ParamType::Vec4);
    EXPECT_FLOAT_EQ(tint->vec4[0], 1.0f);
}

TEST(MaterialProgramManifest, DefaultsNeverOverrideTheMaterial)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "params": { "base_tint": [0.25, 0.5, 0.75, 1] }
            }
        }
    })");

    ncl_material::ApplyDefaults(material, ManifestFrom(kManifest));

    const ncl_material::ParamValue* tint = material.FindParam("base_tint");
    ASSERT_NE(tint, nullptr);
    EXPECT_FLOAT_EQ(tint->vec4[0], 0.25f);
}

TEST(MaterialProgramManifest, LeavesParametersWithoutDefaultsAlone)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": 0.5 } }
        }
    })");

    ncl_material::ApplyDefaults(material, ManifestFrom(kManifest));

    EXPECT_EQ(material.FindParam("seed_shift"), nullptr);
    EXPECT_EQ(material.FindParam("pattern"), nullptr);
}

TEST(MaterialProgramManifest, DefaultsStillValidate)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": {} }
        }
    })");

    ncl_material::ProgramManifest manifest = ManifestFrom(kManifest);
    ncl_material::ApplyDefaults(material, manifest);

    EXPECT_FALSE(ncl_material::Validate(material, manifest).has_error());
}

TEST(MaterialProgramManifest, RejectsUndeclaredParameter)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "sparkle": 1.0 } }
        }
    })");

    ncl_utils::Result<> result = ncl_material::Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("not declared"), std::string::npos) << result.error_str();
}

TEST(MaterialProgramManifest, RejectsWrongParameterType)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": [1, 1, 1, 1] } }
        }
    })");

    ncl_utils::Result<> result = ncl_material::Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("wrong type"), std::string::npos) << result.error_str();
}

TEST(MaterialProgramManifest, AcceptsAnOrdinaryNonGridValue)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": 0.3 } }
        }
    })");

    ncl_utils::Result<> result = ncl_material::Validate(material, ManifestFrom(kManifest));
    EXPECT_FALSE(result.has_error()) << result.error_str();
    EXPECT_EQ(material.FindParam("layer_mix")->scalar, 0.3f);
}

TEST(MaterialProgramManifest, RejectsTooManyOverlaySlotsForTheProgram)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "overlays": [
                    { "name": "a", "rect": [0.0, 0.0, 0.2, 0.2] },
                    { "name": "b", "rect": [0.2, 0.2, 0.4, 0.4] },
                    { "name": "c", "rect": [0.4, 0.4, 0.6, 0.6] }
                ]
            }
        }
    })");

    ncl_utils::Result<> result = ncl_material::Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("overlay slots"), std::string::npos) << result.error_str();
}

TEST(MaterialProgramManifest, RejectsAMaterialForAnotherProgram)
{
    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": { "ak47_body.bmp": { "program": "something_else" } }
    })");

    ncl_utils::Result<> result = ncl_material::Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("manifest describes"), std::string::npos) << result.error_str();
}

TEST(MaterialProgramManifest, ParsesDeclaredStrings)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "wear": { "type": "scalar", "label": "#tok_wear", "hint": "#tok_wear_hint" },
            "tint": { "type": "vec4" }
        }
    })";

    ncl_material::ProgramManifest manifest = ManifestFrom(json);

    const ncl_material::ParamSchema* wear = manifest.FindParam("wear");
    ASSERT_NE(wear, nullptr);
    EXPECT_EQ(wear->label, "#tok_wear");
    EXPECT_EQ(wear->hint, "#tok_wear_hint");
    EXPECT_EQ(wear->get_label_or_name(), "#tok_wear");

    // The fallback the panel leans on: a parameter without a label still has something to
    // show, and it is the identifier rather than an empty row.
    const ncl_material::ParamSchema* tint = manifest.FindParam("tint");
    ASSERT_NE(tint, nullptr);
    EXPECT_TRUE(tint->label.empty());
    EXPECT_EQ(tint->get_label_or_name(), "tint");
}

TEST(MaterialProgramManifest, RejectsAnEmptyLabel)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "label": "" } }
    })";

    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(json);
    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("must not be empty"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, ParsesADeclaredWidget)
{
    ncl_material::ProgramManifest manifest = ManifestFrom(R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "tint": { "type": "vec4", "widget": "color" },
            "transform": { "type": "vec4" }
        }
    })");

    EXPECT_EQ(manifest.FindParam("tint")->widget, ncl_material::ParamWidget::Color);
    EXPECT_EQ(manifest.FindParam("transform")->widget, ncl_material::ParamWidget::Default);
}

TEST(MaterialProgramManifest, RejectsAColorWidgetOnSomethingThatIsNotFourNumbers)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "widget": "color" } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("only a vec4"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, RejectsAnUnknownWidget)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "tint": { "type": "vec4", "widget": "gradient" } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("unknown widget"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, ParsesScalarChoiceWithIntegerDefault)
{
    const ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "mode": {
                "type": "scalar", "widget": "choice", "choices": ["#Mode_Off", "#Mode_On"],
                "instance": true, "min": 0, "max": 1, "default": 1
            }
        }
    })");

    ASSERT_FALSE(manifest.has_error()) << manifest.error_str();
    const ncl_material::ParamSchema* mode = manifest->FindParam("mode");
    ASSERT_NE(mode, nullptr);
    EXPECT_EQ(mode->type, ncl_material::ParamType::Scalar);
    EXPECT_TRUE(mode->instance);
    EXPECT_EQ(mode->widget, ncl_material::ParamWidget::Choice);
    EXPECT_EQ(mode->choices, (std::vector<std::string>{"#Mode_Off", "#Mode_On"}));
    EXPECT_FLOAT_EQ(mode->default_value.scalar, 1.0f);
}

TEST(MaterialProgramManifest, RejectsMalformedScalarChoices)
{
    const char* schemas[] = {
        R"({"type":"vec4","widget":"choice","choices":["Off","On"],"min":0,"max":1,"default":[0,0,0,0]})",
        R"({"type":"scalar","widget":"choice","min":0,"max":1,"default":0})",
        R"({"type":"scalar","widget":"choice","choices":"Off","min":0,"max":1,"default":0})",
        R"({"type":"scalar","widget":"choice","choices":[],"min":0,"max":1,"default":0})",
        R"({"type":"scalar","widget":"choice","choices":["Off"],"min":0,"max":1,"default":0})",
        R"({"type":"scalar","widget":"choice","choices":["Off",2],"min":0,"max":1,"default":0})",
        R"({"type":"scalar","widget":"choice","choices":["Off",""],"min":0,"max":1,"default":0})",
        R"({"type":"scalar","widget":"choice","choices":["Off","On"],"default":0})",
        R"({"type":"scalar","widget":"choice","choices":["Off","On"],"min":-1,"max":1,"default":0})",
        R"({"type":"scalar","widget":"choice","choices":["Off","On"],"min":0,"max":2,"default":0})",
        R"({"type":"scalar","widget":"choice","choices":["Off","On"],"min":0,"max":1})",
        R"({"type":"scalar","widget":"choice","choices":["Off","On"],"min":0,"max":1,"default":0.5})",
        R"({"type":"scalar","widget":"choice","choices":["Off","On"],"min":0,"max":1,"default":2})",
        R"({"type":"scalar","choices":["Off","On"]})"
    };

    for (const char* schema : schemas)
    {
        SCOPED_TRACE(schema);
        const std::string json = std::string(R"({"version":1,"program":"p","parameters":{"mode":)") + schema + "}}";
        const ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(json);
        ASSERT_TRUE(manifest.has_error());
        EXPECT_EQ(manifest.error_str().find("unknown widget"), std::string::npos) << manifest.error_str();
    }
}

TEST(MaterialProgramManifest, RejectsUnboundedScalarChoiceList)
{
    std::string json = R"({"version":1,"program":"p","parameters":{"mode":{"type":"scalar","widget":"choice","choices":[)";
    for (int i = 0; i < 65; i++)
    {
        json += i == 0 ? "\"Mode\"" : ",\"Mode\"";
    }
    json += R"(],"min":0,"max":64,"default":0}}})";

    const ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(json);
    ASSERT_TRUE(manifest.has_error());
    EXPECT_EQ(manifest.error_str().find("unknown widget"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, ChoiceMaterialsRejectFractionalAndOutOfRangeIndices)
{
    const ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version":1,"program":"p","parameters":{"mode":{
            "type":"scalar","widget":"choice","choices":["Off","On"],"min":0,"max":1,"default":0
        }}
    })");
    ASSERT_FALSE(manifest.has_error()) << manifest.error_str();

    ncl_material::Material material;
    material.program = "p";
    material.params.push_back({"mode", {ncl_material::ParamType::Scalar}});
    for (float invalid : {-1.0f, 0.0001f, 0.5f, 0.9999f, 2.0f, std::numeric_limits<float>::quiet_NaN()})
    {
        material.params.front().value.scalar = invalid;
        EXPECT_TRUE(ncl_material::Validate(material, *manifest).has_error()) << invalid;
    }
    for (float valid : {0.0f, 1.0f})
    {
        material.params.front().value.scalar = valid;
        EXPECT_FALSE(ncl_material::Validate(material, *manifest).has_error()) << valid;
    }
}

TEST(MaterialProgramManifest, ParsesADeclaredRange)
{
    ncl_material::ProgramManifest manifest = ManifestFrom(R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "wear": { "type": "scalar", "min": 0.0, "max": 1.0 },
            "loose": { "type": "scalar" }
        }
    })");

    const ncl_material::ParamSchema* wear = manifest.FindParam("wear");
    ASSERT_NE(wear, nullptr);
    EXPECT_TRUE(wear->has_range);
    EXPECT_FLOAT_EQ(wear->min_value, 0.0f);
    EXPECT_FLOAT_EQ(wear->max_value, 1.0f);

    EXPECT_FALSE(manifest.FindParam("loose")->has_range);
}

TEST(MaterialProgramManifest, RejectsHalfADeclaredRange)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "min": 0.0 } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("or neither"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, RejectsAnInvertedRange)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "min": 1.0, "max": 0.0 } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("below"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, RejectsARangeOnATexture)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "pattern": { "type": "texture", "min": 0.0, "max": 1.0 } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("cannot declare a range"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, RejectsAVec4ComponentOutsideTheDeclaredRange)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "weapon_finish",
        "parameters": { "base_tint": { "type": "vec4", "min": 0.0, "max": 1.0 } }
    })";

    // Bounded per component, so a color the program limited to 0..1 is limited in all
    // four rather than only where an editor happened to look.
    ncl_material::Material below = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "base_tint": [1, -0.5, 1, 1] } }
        }
    })");

    ncl_utils::Result<> result = ncl_material::Validate(below, ManifestFrom(json));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("outside the range"), std::string::npos) << result.error_str();

    ncl_material::Material above = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "base_tint": [1, 1, 1, 2] } }
        }
    })");

    EXPECT_TRUE(ncl_material::Validate(above, ManifestFrom(json)).has_error());

    ncl_material::Material inside = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "base_tint": [0, 0.5, 1, 1] } }
        }
    })");

    EXPECT_FALSE(ncl_material::Validate(inside, ManifestFrom(json)).has_error());
}

TEST(MaterialProgramManifest, RejectsAValueOutsideTheDeclaredRange)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "weapon_finish",
        "parameters": { "seed_shift": { "type": "scalar", "min": 0.0, "max": 1.0 } }
    })";

    ncl_material::Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "seed_shift": 1.5 } }
        }
    })");

    ncl_utils::Result<> result = ncl_material::Validate(material, ManifestFrom(json));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("outside the range"), std::string::npos) << result.error_str();
}

TEST(MaterialProgramManifest, GroupsEveryParameterWhenNoneAreDeclared)
{
    ncl_material::ProgramManifest manifest = ManifestFrom(kManifest);

    ASSERT_EQ(manifest.groups.size(), 1u);
    EXPECT_TRUE(manifest.groups[0].get_label_or_name().empty());
    EXPECT_EQ(manifest.groups[0].params.size(), manifest.parameters.size());
}

TEST(MaterialProgramManifest, KeepsTheDeclaredGroupAndMemberOrder)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "a": { "type": "scalar" },
            "b": { "type": "scalar" },
            "c": { "type": "scalar" }
        },
        "groups": [
            { "name": "second", "label": "#tok_second", "params": ["c", "b"] },
            { "name": "first", "params": ["a"] }
        ]
    })";

    ncl_material::ProgramManifest manifest = ManifestFrom(json);

    ASSERT_EQ(manifest.groups.size(), 2u);
    EXPECT_EQ(manifest.groups[0].get_label_or_name(), "#tok_second");
    EXPECT_EQ(manifest.groups[1].get_label_or_name(), "first");

    ASSERT_EQ(manifest.groups[0].params.size(), 2u);
    EXPECT_EQ(manifest.parameters[manifest.groups[0].params[0]].name, "c");
    EXPECT_EQ(manifest.parameters[manifest.groups[0].params[1]].name, "b");
}

TEST(MaterialProgramManifest, PutsUnclaimedParametersInATrailingGroup)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "a": { "type": "scalar" },
            "b": { "type": "scalar" }
        },
        "groups": [ { "name": "first", "params": ["a"] } ]
    })";

    ncl_material::ProgramManifest manifest = ManifestFrom(json);

    ASSERT_EQ(manifest.groups.size(), 2u);
    EXPECT_TRUE(manifest.groups[1].get_label_or_name().empty());
    ASSERT_EQ(manifest.groups[1].params.size(), 1u);
    EXPECT_EQ(manifest.parameters[manifest.groups[1].params[0]].name, "b");
}

TEST(MaterialProgramManifest, GroupsCoverEveryParameterExactlyOnce)
{
    ncl_material::ProgramManifest manifest = ManifestFrom(R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "a": { "type": "scalar" },
            "b": { "type": "scalar" },
            "c": { "type": "scalar" }
        },
        "groups": [ { "name": "one", "params": ["b"] } ]
    })");

    std::vector<int> seen(manifest.parameters.size(), 0);
    for (const ncl_material::ParamGroup& group : manifest.groups)
    {
        for (size_t index : group.params)
        {
            ASSERT_LT(index, manifest.parameters.size());
            seen[index]++;
        }
    }

    for (int count : seen)
    {
        EXPECT_EQ(count, 1);
    }
}

TEST(MaterialProgramManifest, RejectsAGroupNamingAnUndeclaredParameter)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "groups": [ { "name": "one", "params": ["b"] } ]
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("not a declared parameter"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, RejectsAParameterInTwoGroups)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "groups": [
            { "name": "one", "params": ["a"] },
            { "name": "two", "params": ["a"] }
        ]
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("already in a group"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, RejectsAGroupWithNoParameters)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "groups": [ { "name": "one", "params": [] } ]
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("non-empty array"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, AcceptsADeclaredStringsFile)
{
    ncl_material::ProgramManifest manifest = ManifestFrom(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "strings": "resource/p_%language%.txt"
    })");

    EXPECT_EQ(manifest.strings, "resource/p_%language%.txt");
}

TEST(MaterialProgramManifest, RejectsAStringsFileThatLeavesTheGameDirectory)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "strings": "../resource/p_%language%.txt"
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("manifest.strings"), std::string::npos) << manifest.error_str();
}

TEST(MaterialProgramManifest, RejectsAStringsFileTheLocalizationSystemDoesNotRead)
{
    ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "strings": "resource/p_%language%.res"
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("manifest.strings"), std::string::npos) << manifest.error_str();
}

TEST(MaterialResolve, ImplicitMaterialCarriesEveryFlag)
{
    ncl_material::Material material = ncl_material::MakeImplicitMaterial(ncl_material::kFlagChrome | ncl_material::kFlagMasked);

    EXPECT_EQ(material.program, ncl_material::kLegacyProgram);
    ASSERT_EQ(material.params.size(), 5u);

    EXPECT_FLOAT_EQ(material.FindParam("chrome")->scalar, 1.0f);
    EXPECT_FLOAT_EQ(material.FindParam("masked")->scalar, 1.0f);
    EXPECT_FLOAT_EQ(material.FindParam("additive")->scalar, 0.0f);
    EXPECT_FLOAT_EQ(material.FindParam("flatshade")->scalar, 0.0f);
    EXPECT_FLOAT_EQ(material.FindParam("fullbright")->scalar, 0.0f);
}

TEST(MaterialResolve, FallsBackToTheImplicitMaterial)
{
    ncl_material::ResolvedMaterial resolved = ncl_material::Resolve(ncl_material::MaterialSources{}, "ak47_body.bmp", ncl_material::kFlagAdditive);

    EXPECT_EQ(resolved.origin, ncl_material::MaterialOrigin::Implicit);
    EXPECT_EQ(resolved.material.program, ncl_material::kLegacyProgram);
    EXPECT_FLOAT_EQ(resolved.material.FindParam("additive")->scalar, 1.0f);
}

TEST(MaterialResolve, PrefersEachSourceOverTheNext)
{
    ncl_utils::ResultT<ncl_material::MaterialDoc> model = ncl_material::ParseMaterialDoc(R"({
        "version": 1,
        "materials": { "ak47_body.bmp": { "program": "from_model" } }
    })");
    ASSERT_FALSE(model.has_error()) << model.error_str();

    ncl_utils::ResultT<ncl_material::MaterialDoc> server = ncl_material::ParseMaterialDoc(R"({
        "version": 1,
        "materials": { "ak47_body.bmp": { "program": "from_server" } }
    })");
    ASSERT_FALSE(server.has_error()) << server.error_str();

    ncl_material::Material instance;
    instance.program = "from_instance";

    ncl_material::MaterialSources sources;
    sources.model = &*model;

    ncl_material::ResolvedMaterial resolved = ncl_material::Resolve(sources, "ak47_body.bmp", 0);
    EXPECT_EQ(resolved.origin, ncl_material::MaterialOrigin::ModelDocument);
    EXPECT_EQ(resolved.material.program, "from_model");

    sources.instance = &instance;
    resolved = ncl_material::Resolve(sources, "ak47_body.bmp", 0);
    EXPECT_EQ(resolved.origin, ncl_material::MaterialOrigin::Instance);
    EXPECT_EQ(resolved.material.program, "from_instance");
}

TEST(MaterialResolve, SkipsASourceThatDoesNotDescribeTheTexture)
{
    ncl_utils::ResultT<ncl_material::MaterialDoc> model = ncl_material::ParseMaterialDoc(R"({
        "version": 1,
        "materials": { "other.bmp": { "program": "from_model" } }
    })");
    ASSERT_FALSE(model.has_error()) << model.error_str();

    ncl_material::MaterialSources sources;
    sources.model = &*model;

    ncl_material::ResolvedMaterial resolved = ncl_material::Resolve(sources, "ak47_body.bmp", 0);

    EXPECT_EQ(resolved.origin, ncl_material::MaterialOrigin::Implicit);
}

TEST(MaterialProgramManifest, RejectsNumbersOutsideTheFiniteFloatRange)
{
    for (const char* parameter :
         {R"({"type":"scalar","default":1e100})",
          R"({"type":"scalar","default":-1e100})",
          R"({"type":"vec4","default":[1e100,0,0,0]})",
          R"({"type":"vec4","default":[0,0,0,-1e100]})",
          R"({"type":"scalar","min":-1e100,"max":1})",
          R"({"type":"scalar","min":0,"max":1e100})"})
    {
        SCOPED_TRACE(parameter);
        std::string json = R"({"version":1,"program":"weapon_finish","parameters":{"p":)" + std::string(parameter) + "}}";

        EXPECT_TRUE(ncl_material::ParseProgramManifest(json).has_error());
    }
}

TEST(MaterialProgramManifest, RejectsVersionsThatCannotBeRepresentedAsWholeInts)
{
    for (const char* version : {"1.5", "1e100", "-1e100", "2147483648", "-2147483649"})
    {
        SCOPED_TRACE(version);
        std::string json = R"({"version":)" + std::string(version) + R"(,"program":"p","parameters":{}})";

        EXPECT_TRUE(ncl_material::ParseProgramManifest(json).has_error());
    }
}

TEST(MaterialProgramManifest, RejectsOverlayLimitsBeforeUnsignedConversion)
{
    for (const char* limit : {"1e100", "-1e100", "4294967296", "-1", "1.5"})
    {
        SCOPED_TRACE(limit);
        std::string json = R"({"version":1,"program":"p","parameters":{},"overlays":{"max":)" + std::string(limit) + "}}";

        EXPECT_TRUE(ncl_material::ParseProgramManifest(json).has_error());
    }
}

TEST(MaterialProgramManifest, DirectFloatInstanceSchemaNeedsNoStep)
{
    const ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
        "version":1,"program":"p","parameters":{
            "value":{"type":"scalar","instance":true,"min":-1,"max":1,"default":0.1234567}
        }
    })");
    ASSERT_FALSE(manifest.has_error()) << manifest.error_str();
    EXPECT_EQ(manifest->FindParam("value")->default_value.scalar, 0.1234567f);
}

TEST(MaterialProgramManifest, DirectFloatAuthoredValuesPreserveNonGridScalars)
{
    const ncl_material::ProgramManifest manifest = ManifestFrom(kManifest);
    ncl_material::Material material;
    material.program = "weapon_finish";
    material.params.push_back({"layer_mix", {ncl_material::ParamType::Scalar}});
    for (float value : {0.1234567f, -0.1234567f, 0.0000001f})
    {
        material.params.front().value.scalar = value;
        EXPECT_FALSE(ncl_material::Validate(material, manifest).has_error()) << value;
        EXPECT_EQ(material.params.front().value.scalar, value);
    }
}

TEST(MaterialProgramManifest, DirectFloatValidationRejectsEveryNonfiniteComponent)
{
    const ncl_material::ProgramManifest manifest = ManifestFrom(kManifest);
    ncl_material::Material material;
    material.program = "weapon_finish";
    material.params.push_back({"seed_shift", {ncl_material::ParamType::Scalar}});
    for (float invalid :
         {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
    {
        material.params.front().value.scalar = invalid;
        EXPECT_TRUE(ncl_material::Validate(material, manifest).has_error());
        material.params.front().name = "base_tint";
        material.params.front().value.type = ncl_material::ParamType::Vec4;
        for (int component = 0; component < 4; component++)
        {
            material.params.front().value.vec4 = {0.1f, 0.2f, 0.3f, 0.4f};
            material.params.front().value.vec4[component] = invalid;
            EXPECT_TRUE(ncl_material::Validate(material, manifest).has_error()) << component;
        }
        material.params.front().name = "seed_shift";
        material.params.front().value.type = ncl_material::ParamType::Scalar;
    }
}
