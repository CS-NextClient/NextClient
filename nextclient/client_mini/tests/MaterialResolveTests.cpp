#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <ncl_material/MaterialParse.h>
#include <ncl_material/MaterialResolve.h>
#include <ncl_material/ProgramManifest.h>

using namespace ncl_material;

namespace
{
    constexpr const char* kManifest = R"({
        "version": 1,
        "program": "weapon_finish",
        "parameters": {
            "base_tint":  { "type": "vec4", "default": [1, 1, 1, 1] },
            "layer_mix":  { "type": "scalar", "instance": true, "quantum": 0.25 },
            "seed_shift": { "type": "scalar" },
            "pattern":    { "type": "texture" }
        },
        "overlays": { "max": 2 }
    })";

    Material MaterialFrom(const char* json)
    {
        ncl_utils::ResultT<MaterialDoc> doc = ParseMaterialDoc(json);
        EXPECT_FALSE(doc.has_error()) << doc.error_str();

        const Material* material = doc->FindByTexture("ak47_body.bmp");
        EXPECT_NE(material, nullptr);

        return material != nullptr ? *material : Material {};
    }

    ProgramManifest ManifestFrom(const char* json)
    {
        ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(json);
        EXPECT_FALSE(manifest.has_error()) << manifest.error_str();

        return manifest.has_error() ? ProgramManifest {} : *manifest;
    }
}

TEST(ProgramManifest, ParsesSchema)
{
    ProgramManifest manifest = ManifestFrom(kManifest);

    EXPECT_EQ(manifest.program, "weapon_finish");
    EXPECT_EQ(manifest.parameters.size(), 4u);
    EXPECT_EQ(manifest.max_overlays, 2u);

    const ParamSchema* mix = manifest.FindParam("layer_mix");
    ASSERT_NE(mix, nullptr);
    EXPECT_EQ(mix->type, ParamType::Scalar);
    EXPECT_TRUE(mix->instance);
    EXPECT_FLOAT_EQ(mix->quantum, 0.25f);

    const ParamSchema* tint = manifest.FindParam("base_tint");
    ASSERT_NE(tint, nullptr);
    EXPECT_TRUE(tint->has_default);
    EXPECT_FLOAT_EQ(tint->default_value.vec4[3], 1.0f);
}

TEST(ProgramManifest, RejectsInstanceScalarWithoutQuantum)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "instance": true } }
    })";

    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(json);
    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("quantum"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, RejectsQuantumOnNonScalar)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": { "tint": { "type": "vec4", "quantum": 0.5 } }
    })";

    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(json);
    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("only a scalar"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, RejectsUnknownType)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": { "thing": { "type": "colour" } }
    })";

    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(json);
    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("unknown type"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, AcceptsAConformingMaterial)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "params": { "layer_mix": 0.5, "base_tint": [1, 0.5, 0.25, 1] },
                "overlays": [ { "name": "left", "rect": [0.1, 0.2, 0.3, 0.4] } ]
            }
        }
    })");

    EXPECT_FALSE(Validate(material, ManifestFrom(kManifest)).has_error());
}

TEST(ProgramManifest, FillsInDeclaredDefaults)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": 0.5 } }
        }
    })");

    ApplyDefaults(material, ManifestFrom(kManifest));

    // Absent and defaulted, which is the whole point: an uploader sends what the material
    // lists, and a uniform nobody sends reads as zero rather than as the declared value.
    const ParamValue* tint = material.FindParam("base_tint");
    ASSERT_NE(tint, nullptr);
    EXPECT_EQ(tint->type, ParamType::Vec4);
    EXPECT_FLOAT_EQ(tint->vec4[0], 1.0f);
}

TEST(ProgramManifest, DefaultsNeverOverrideTheMaterial)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "params": { "base_tint": [0.25, 0.5, 0.75, 1] }
            }
        }
    })");

    ApplyDefaults(material, ManifestFrom(kManifest));

    const ParamValue* tint = material.FindParam("base_tint");
    ASSERT_NE(tint, nullptr);
    EXPECT_FLOAT_EQ(tint->vec4[0], 0.25f);
}

TEST(ProgramManifest, LeavesParametersWithoutDefaultsAlone)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": 0.5 } }
        }
    })");

    ApplyDefaults(material, ManifestFrom(kManifest));

    EXPECT_EQ(material.FindParam("seed_shift"), nullptr);
    EXPECT_EQ(material.FindParam("pattern"), nullptr);
}

TEST(ProgramManifest, DefaultsStillValidate)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": {} }
        }
    })");

    ProgramManifest manifest = ManifestFrom(kManifest);
    ApplyDefaults(material, manifest);

    EXPECT_FALSE(Validate(material, manifest).has_error());
}

TEST(ProgramManifest, RejectsUndeclaredParameter)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "sparkle": 1.0 } }
        }
    })");

    ncl_utils::Result<> result = Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("not declared"), std::string::npos) << result.error_str();
}

TEST(ProgramManifest, RejectsWrongParameterType)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": [1, 1, 1, 1] } }
        }
    })");

    ncl_utils::Result<> result = Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("wrong type"), std::string::npos) << result.error_str();
}

TEST(ProgramManifest, RejectsValueOffTheQuantisationGrid)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": 0.3 } }
        }
    })");

    ncl_utils::Result<> result = Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("multiple of its quantum"), std::string::npos) << result.error_str();
}

TEST(ProgramManifest, RejectsTooManyOverlaySlotsForTheProgram)
{
    Material material = MaterialFrom(R"({
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

    ncl_utils::Result<> result = Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("overlay slots"), std::string::npos) << result.error_str();
}

TEST(ProgramManifest, RejectsAMaterialForAnotherProgram)
{
    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": { "ak47_body.bmp": { "program": "something_else" } }
    })");

    ncl_utils::Result<> result = Validate(material, ManifestFrom(kManifest));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("manifest describes"), std::string::npos) << result.error_str();
}

TEST(ProgramManifest, ParsesDeclaredStrings)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "wear": { "type": "scalar", "label": "#tok_wear", "hint": "#tok_wear_hint" },
            "tint": { "type": "vec4" }
        }
    })";

    ProgramManifest manifest = ManifestFrom(json);

    const ParamSchema* wear = manifest.FindParam("wear");
    ASSERT_NE(wear, nullptr);
    EXPECT_EQ(wear->label, "#tok_wear");
    EXPECT_EQ(wear->hint, "#tok_wear_hint");
    EXPECT_EQ(wear->label_or_name(), "#tok_wear");

    // The fallback the panel leans on: a parameter without a label still has something to
    // show, and it is the identifier rather than an empty row.
    const ParamSchema* tint = manifest.FindParam("tint");
    ASSERT_NE(tint, nullptr);
    EXPECT_TRUE(tint->label.empty());
    EXPECT_EQ(tint->label_or_name(), "tint");
}

TEST(ProgramManifest, RejectsAnEmptyLabel)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "label": "" } }
    })";

    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(json);
    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("must not be empty"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, ParsesADeclaredWidget)
{
    ProgramManifest manifest = ManifestFrom(R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "tint": { "type": "vec4", "widget": "color" },
            "transform": { "type": "vec4" }
        }
    })");

    EXPECT_EQ(manifest.FindParam("tint")->widget, ParamWidget::Color);
    EXPECT_EQ(manifest.FindParam("transform")->widget, ParamWidget::Default);
}

TEST(ProgramManifest, RejectsAColourWidgetOnSomethingThatIsNotFourNumbers)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "widget": "color" } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("only a vec4"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, RejectsAnUnknownWidget)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "tint": { "type": "vec4", "widget": "gradient" } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("unknown widget"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, ParsesADeclaredRange)
{
    ProgramManifest manifest = ManifestFrom(R"({
        "version": 1,
        "program": "p",
        "parameters": {
            "wear": { "type": "scalar", "min": 0.0, "max": 1.0 },
            "loose": { "type": "scalar" }
        }
    })");

    const ParamSchema* wear = manifest.FindParam("wear");
    ASSERT_NE(wear, nullptr);
    EXPECT_TRUE(wear->has_range);
    EXPECT_FLOAT_EQ(wear->min_value, 0.0f);
    EXPECT_FLOAT_EQ(wear->max_value, 1.0f);

    EXPECT_FALSE(manifest.FindParam("loose")->has_range);
}

TEST(ProgramManifest, RejectsHalfADeclaredRange)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "min": 0.0 } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("or neither"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, RejectsAnInvertedRange)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "wear": { "type": "scalar", "min": 1.0, "max": 0.0 } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("below"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, RejectsARangeOnATexture)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "pattern": { "type": "texture", "min": 0.0, "max": 1.0 } }
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("cannot declare a range"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, RejectsAVec4ComponentOutsideTheDeclaredRange)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "weapon_finish",
        "parameters": { "base_tint": { "type": "vec4", "min": 0.0, "max": 1.0 } }
    })";

    // Bounded per component, so a colour the program limited to 0..1 is limited in all
    // four rather than only where an editor happened to look.
    Material below = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "base_tint": [1, -0.5, 1, 1] } }
        }
    })");

    ncl_utils::Result<> result = Validate(below, ManifestFrom(json));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("outside the range"), std::string::npos) << result.error_str();

    Material above = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "base_tint": [1, 1, 1, 2] } }
        }
    })");

    EXPECT_TRUE(Validate(above, ManifestFrom(json)).has_error());

    Material inside = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "base_tint": [0, 0.5, 1, 1] } }
        }
    })");

    EXPECT_FALSE(Validate(inside, ManifestFrom(json)).has_error());
}

TEST(ProgramManifest, RejectsAValueOutsideTheDeclaredRange)
{
    constexpr const char* json = R"({
        "version": 1,
        "program": "weapon_finish",
        "parameters": { "seed_shift": { "type": "scalar", "min": 0.0, "max": 1.0 } }
    })";

    Material material = MaterialFrom(R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "seed_shift": 1.5 } }
        }
    })");

    ncl_utils::Result<> result = Validate(material, ManifestFrom(json));
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("outside the range"), std::string::npos) << result.error_str();
}

TEST(ProgramManifest, GroupsEveryParameterWhenNoneAreDeclared)
{
    ProgramManifest manifest = ManifestFrom(kManifest);

    ASSERT_EQ(manifest.groups.size(), 1u);
    EXPECT_TRUE(manifest.groups[0].label_or_name().empty());
    EXPECT_EQ(manifest.groups[0].params.size(), manifest.parameters.size());
}

TEST(ProgramManifest, KeepsTheDeclaredGroupAndMemberOrder)
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

    ProgramManifest manifest = ManifestFrom(json);

    ASSERT_EQ(manifest.groups.size(), 2u);
    EXPECT_EQ(manifest.groups[0].label_or_name(), "#tok_second");
    EXPECT_EQ(manifest.groups[1].label_or_name(), "first");

    ASSERT_EQ(manifest.groups[0].params.size(), 2u);
    EXPECT_EQ(manifest.parameters[manifest.groups[0].params[0]].name, "c");
    EXPECT_EQ(manifest.parameters[manifest.groups[0].params[1]].name, "b");
}

TEST(ProgramManifest, PutsUnclaimedParametersInATrailingGroup)
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

    ProgramManifest manifest = ManifestFrom(json);

    ASSERT_EQ(manifest.groups.size(), 2u);
    EXPECT_TRUE(manifest.groups[1].label_or_name().empty());
    ASSERT_EQ(manifest.groups[1].params.size(), 1u);
    EXPECT_EQ(manifest.parameters[manifest.groups[1].params[0]].name, "b");
}

TEST(ProgramManifest, GroupsCoverEveryParameterExactlyOnce)
{
    ProgramManifest manifest = ManifestFrom(R"({
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
    for (const ParamGroup& group : manifest.groups)
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

TEST(ProgramManifest, RejectsAGroupNamingAnUndeclaredParameter)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "groups": [ { "name": "one", "params": ["b"] } ]
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("not a declared parameter"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, RejectsAParameterInTwoGroups)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
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

TEST(ProgramManifest, RejectsAGroupWithNoParameters)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "groups": [ { "name": "one", "params": [] } ]
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("non-empty array"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, AcceptsADeclaredStringsFile)
{
    ProgramManifest manifest = ManifestFrom(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "strings": "resource/p_%language%.txt"
    })");

    EXPECT_EQ(manifest.strings, "resource/p_%language%.txt");
}

TEST(ProgramManifest, RejectsAStringsFileThatLeavesTheGameDirectory)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
        "version": 1,
        "program": "p",
        "parameters": { "a": { "type": "scalar" } },
        "strings": "../resource/p_%language%.txt"
    })");

    ASSERT_TRUE(manifest.has_error());
    EXPECT_NE(manifest.error_str().find("manifest.strings"), std::string::npos) << manifest.error_str();
}

TEST(ProgramManifest, RejectsAStringsFileTheLocalisationSystemDoesNotRead)
{
    ncl_utils::ResultT<ProgramManifest> manifest = ParseProgramManifest(R"({
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
    Material material = MakeImplicitMaterial(kFlagChrome | kFlagMasked);

    EXPECT_EQ(material.program, kLegacyProgram);
    ASSERT_EQ(material.params.size(), 5u);

    EXPECT_FLOAT_EQ(material.FindParam("chrome")->scalar, 1.0f);
    EXPECT_FLOAT_EQ(material.FindParam("masked")->scalar, 1.0f);
    EXPECT_FLOAT_EQ(material.FindParam("additive")->scalar, 0.0f);
    EXPECT_FLOAT_EQ(material.FindParam("flatshade")->scalar, 0.0f);
    EXPECT_FLOAT_EQ(material.FindParam("fullbright")->scalar, 0.0f);
}

TEST(MaterialResolve, FallsBackToTheImplicitMaterial)
{
    ResolvedMaterial resolved = Resolve(MaterialSources {}, "ak47_body.bmp", kFlagAdditive);

    EXPECT_EQ(resolved.origin, MaterialOrigin::Implicit);
    EXPECT_EQ(resolved.material.program, kLegacyProgram);
    EXPECT_FLOAT_EQ(resolved.material.FindParam("additive")->scalar, 1.0f);
}

TEST(MaterialResolve, PrefersEachSourceOverTheNext)
{
    ncl_utils::ResultT<MaterialDoc> model = ParseMaterialDoc(R"({
        "version": 1,
        "materials": { "ak47_body.bmp": { "program": "from_model" } }
    })");
    ASSERT_FALSE(model.has_error()) << model.error_str();

    ncl_utils::ResultT<MaterialDoc> server = ParseMaterialDoc(R"({
        "version": 1,
        "materials": { "ak47_body.bmp": { "program": "from_server" } }
    })");
    ASSERT_FALSE(server.has_error()) << server.error_str();

    Material instance;
    instance.program = "from_instance";

    MaterialSources sources;
    sources.model = &*model;

    ResolvedMaterial resolved = Resolve(sources, "ak47_body.bmp", 0);
    EXPECT_EQ(resolved.origin, MaterialOrigin::ModelDocument);
    EXPECT_EQ(resolved.material.program, "from_model");

    sources.instance = &instance;
    resolved = Resolve(sources, "ak47_body.bmp", 0);
    EXPECT_EQ(resolved.origin, MaterialOrigin::Instance);
    EXPECT_EQ(resolved.material.program, "from_instance");
}

TEST(MaterialResolve, SkipsASourceThatDoesNotDescribeTheTexture)
{
    ncl_utils::ResultT<MaterialDoc> model = ParseMaterialDoc(R"({
        "version": 1,
        "materials": { "other.bmp": { "program": "from_model" } }
    })");
    ASSERT_FALSE(model.has_error()) << model.error_str();

    MaterialSources sources;
    sources.model = &*model;

    ResolvedMaterial resolved = Resolve(sources, "ak47_body.bmp", 0);

    EXPECT_EQ(resolved.origin, MaterialOrigin::Implicit);
}
