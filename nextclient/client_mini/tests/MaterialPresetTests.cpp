#include <gtest/gtest.h>

#include <ncl_material/MaterialPreset.h>

using namespace ncl_material;

namespace
{
    constexpr const char* kPreset = R"({
        "version": 1,
        "materials": [
            {
                "name": "golden_slide",
                "texture": "glock_slide.bmp",
                "material": {
                    "program": "weapon_finish",
                    "params": {
                        "uPattern": { "texture": "materials/skins/stripes.png" },
                        "uLayerMix": 0.5
                    }
                }
            },
            {
                "name": "worn_base",
                "texture": "glock_base.bmp",
                "material": {
                    "program": "weapon_finish",
                    "params": { "uWear": 0.25 }
                }
            }
        ]
    })";

    MaterialPreset Parse(std::string_view json, PresetBudget budget = PresetBudget {})
    {
        ncl_utils::ResultT<MaterialPreset> result = ParseMaterialPreset(json, budget);

        EXPECT_FALSE(result.has_error()) << result.error_str();

        return result.has_error() ? MaterialPreset {} : *result;
    }

    std::string WithMaterials(const std::string& materials)
    {
        return R"({"version": 1, "materials": [)" + materials + "]}";
    }
}

TEST(MaterialPreset, ReadsEveryMaterialWithItsNameAndTexture)
{
    MaterialPreset preset = Parse(kPreset);

    ASSERT_EQ(preset.materials.size(), 2u);

    EXPECT_EQ(preset.materials[0].name, "golden_slide");
    EXPECT_EQ(preset.materials[0].texture_name, "glock_slide.bmp");
    EXPECT_EQ(preset.materials[0].material.program, "weapon_finish");

    EXPECT_EQ(preset.materials[1].name, "worn_base");
    EXPECT_EQ(preset.materials[1].texture_name, "glock_base.bmp");
}

TEST(MaterialPreset, FindsByNameIgnoringCase)
{
    MaterialPreset preset = Parse(kPreset);

    const PresetMaterial* found = preset.FindByName("GOLDEN_slide");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->texture_name, "glock_slide.bmp");
}

TEST(MaterialPreset, FindsNothingForAnUnknownOrEmptyName)
{
    MaterialPreset preset = Parse(kPreset);

    EXPECT_EQ(preset.FindByName("no_such_material"), nullptr);
    EXPECT_EQ(preset.FindByName(""), nullptr);
}

TEST(MaterialPreset, RejectsTwoMaterialsOfTheSameName)
{
    std::string json = WithMaterials(
        R"({"name": "same", "texture": "a.bmp", "material": {"program": "p", "params": {}}},)"
        R"({"name": "SAME", "texture": "b.bmp", "material": {"program": "p", "params": {}}})"
    );

    EXPECT_TRUE(ParseMaterialPreset(json, PresetBudget {}).has_error());
}

TEST(MaterialPreset, RejectsAMaterialWithoutANameOrWithAnEmptyOne)
{
    EXPECT_TRUE(ParseMaterialPreset(
        WithMaterials(R"({"texture": "a.bmp", "material": {"program": "p", "params": {}}})"),
        PresetBudget {}).has_error());

    EXPECT_TRUE(ParseMaterialPreset(
        WithMaterials(R"({"name": "", "texture": "a.bmp", "material": {"program": "p", "params": {}}})"),
        PresetBudget {}).has_error());
}

TEST(MaterialPreset, RejectsAMaterialWithoutATexture)
{
    EXPECT_TRUE(ParseMaterialPreset(
        WithMaterials(R"({"name": "n", "material": {"program": "p", "params": {}}})"),
        PresetBudget {}).has_error());
}

TEST(MaterialPreset, RejectsAnUnsupportedVersion)
{
    EXPECT_TRUE(ParseMaterialPreset(R"({"version": 2, "materials": []})", PresetBudget {}).has_error());
}

TEST(MaterialPreset, RejectsMaterialsThatIsNotAnArray)
{
    EXPECT_TRUE(ParseMaterialPreset(R"({"version": 1, "materials": {}})", PresetBudget {}).has_error());
}

TEST(MaterialPreset, AcceptsAPresetWithNoMaterials)
{
    MaterialPreset preset = Parse(R"({"version": 1, "materials": []})");

    EXPECT_TRUE(preset.materials.empty());
}

TEST(MaterialPreset, RejectsTheWholePresetWhenThereAreTooManyMaterials)
{
    std::string materials;

    for (int i = 0; i < 3; i++)
    {
        materials += i == 0 ? "" : ",";
        materials += R"({"name": "m)" + std::to_string(i) +
                     R"(", "texture": "t.bmp", "material": {"program": "p", "params": {}}})";
    }

    PresetBudget budget;
    budget.max_materials = 2;

    EXPECT_TRUE(ParseMaterialPreset(WithMaterials(materials), budget).has_error());
}

TEST(MaterialPreset, RejectsTheWholePresetWhenThereAreTooManyDistinctTextures)
{
    std::string materials;

    for (int i = 0; i < 3; i++)
    {
        materials += i == 0 ? "" : ",";
        materials += R"({"name": "m)" + std::to_string(i) +
                     R"(", "texture": "t.bmp", "material": {"program": "p", "params": )" +
                     R"({"uPattern": {"texture": "materials/skins/)" + std::to_string(i) + R"(.png"}}}})";
    }

    PresetBudget budget;
    budget.max_textures = 2;

    EXPECT_TRUE(ParseMaterialPreset(WithMaterials(materials), budget).has_error());
}

TEST(MaterialPreset, CountsTheSameTexturePathOnceAgainstTheBudget)
{
    std::string materials;

    for (int i = 0; i < 4; i++)
    {
        materials += i == 0 ? "" : ",";
        materials += R"({"name": "m)" + std::to_string(i) +
                     R"(", "texture": "t.bmp", "material": {"program": "p", "params": )" +
                     R"({"uPattern": {"texture": "materials/skins/one.png"}}}})";
    }

    PresetBudget budget;
    budget.max_textures = 1;

    EXPECT_FALSE(ParseMaterialPreset(WithMaterials(materials), budget).has_error());
}

TEST(MaterialPreset, RejectsATexturePathThatLeavesTheGameDirectory)
{
    const char* paths[] = {
        "../secret.png",
        "/etc/passwd.png",
        "C:/windows/win.png",
        "materials\\skins\\back.png",
        "materials//double.png",
        "materials/./here.png",
    };

    for (const char* path : paths)
    {
        std::string json = WithMaterials(
            R"({"name": "n", "texture": "t.bmp", "material": {"program": "p", "params": )"
            R"({"uPattern": {"texture": ")" + std::string(path) + R"("}}}})"
        );

        EXPECT_TRUE(ParseMaterialPreset(json, PresetBudget {}).has_error()) << path;
    }
}

TEST(MaterialPreset, RejectsATexturePathTheLoaderCannotRead)
{
    std::string json = WithMaterials(
        R"({"name": "n", "texture": "t.bmp", "material": {"program": "p", "params": )"
        R"({"uPattern": {"texture": "materials/skins/stripes.bmp"}}}})"
    );

    EXPECT_TRUE(ParseMaterialPreset(json, PresetBudget {}).has_error());
}

TEST(MaterialPreset, WritesAMaterialInTheShapeItReads)
{
    MaterialPreset preset = Parse(kPreset);

    std::string written = R"({"version": 1, "materials": [)" + WritePresetMaterial(preset.materials[0]) + "]}";

    MaterialPreset again = Parse(written);

    ASSERT_EQ(again.materials.size(), 1u);
    EXPECT_EQ(again.materials[0].name, preset.materials[0].name);
    EXPECT_EQ(again.materials[0].texture_name, preset.materials[0].texture_name);
    EXPECT_EQ(again.materials[0].material.program, preset.materials[0].material.program);
    EXPECT_EQ(again.materials[0].material.params.size(), preset.materials[0].material.params.size());
}
