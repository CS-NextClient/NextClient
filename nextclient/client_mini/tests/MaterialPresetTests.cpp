#include <string>

#include <gtest/gtest.h>
#include <tao/json.hpp>

#include <ncl_material/MaterialPreset.h>


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

    ncl_material::MaterialPreset Parse(std::string_view json, ncl_material::PresetBudget budget = ncl_material::PresetBudget{})
    {
        ncl_utils::ResultT<ncl_material::MaterialPreset> result = ncl_material::ParseMaterialPreset(json, budget);

        EXPECT_FALSE(result.has_error()) << result.error_str();

        return result.has_error() ? ncl_material::MaterialPreset {} : *result;
    }

    std::string WithMaterials(const std::string& materials)
    {
        return R"({"version": 1, "materials": [)" + materials + "]}";
    }
}

TEST(MaterialPreset, ReadsEveryMaterialWithItsNameAndTexture)
{
    ncl_material::MaterialPreset preset = Parse(kPreset);

    ASSERT_EQ(preset.materials.size(), 2u);

    EXPECT_EQ(preset.materials[0].name, "golden_slide");
    EXPECT_EQ(preset.materials[0].texture_name, "glock_slide.bmp");
    EXPECT_EQ(preset.materials[0].material.program, "weapon_finish");

    EXPECT_EQ(preset.materials[1].name, "worn_base");
    EXPECT_EQ(preset.materials[1].texture_name, "glock_base.bmp");
}

TEST(MaterialPreset, FindsByNameIgnoringCase)
{
    ncl_material::MaterialPreset preset = Parse(kPreset);

    const ncl_material::PresetMaterial* found = preset.FindByName("GOLDEN_slide");

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->texture_name, "glock_slide.bmp");
}

TEST(MaterialPreset, FindsNothingForAnUnknownOrEmptyName)
{
    ncl_material::MaterialPreset preset = Parse(kPreset);

    EXPECT_EQ(preset.FindByName("no_such_material"), nullptr);
    EXPECT_EQ(preset.FindByName(""), nullptr);
}

TEST(MaterialPreset, RejectsTwoMaterialsOfTheSameName)
{
    std::string json = WithMaterials(
        R"({"name": "same", "texture": "a.bmp", "material": {"program": "p", "params": {}}},)"
        R"({"name": "SAME", "texture": "b.bmp", "material": {"program": "p", "params": {}}})"
    );

    EXPECT_TRUE(ncl_material::ParseMaterialPreset(json, ncl_material::PresetBudget{}).has_error());
}

TEST(MaterialPreset, RejectsAMaterialWithoutANameOrWithAnEmptyOne)
{
    EXPECT_TRUE(
        ncl_material::ParseMaterialPreset(WithMaterials(R"({"texture": "a.bmp", "material": {"program": "p", "params": {}}})"), ncl_material::PresetBudget{})
            .has_error()
    );

    EXPECT_TRUE(ncl_material::ParseMaterialPreset(
                    WithMaterials(R"({"name": "", "texture": "a.bmp", "material": {"program": "p", "params": {}}})"), ncl_material::PresetBudget{}
    )
                    .has_error());
}

TEST(MaterialPreset, RejectsAMaterialWithoutATexture)
{
    EXPECT_TRUE(ncl_material::ParseMaterialPreset(WithMaterials(R"({"name": "n", "material": {"program": "p", "params": {}}})"), ncl_material::PresetBudget{})
                    .has_error());
}

TEST(MaterialPreset, RejectsAnUnsupportedVersion)
{
    EXPECT_TRUE(ncl_material::ParseMaterialPreset(R"({"version": 2, "materials": []})", ncl_material::PresetBudget{}).has_error());
}

TEST(MaterialPreset, RejectsMaterialsThatIsNotAnArray)
{
    EXPECT_TRUE(ncl_material::ParseMaterialPreset(R"({"version": 1, "materials": {}})", ncl_material::PresetBudget{}).has_error());
}

TEST(MaterialPreset, AcceptsAPresetWithNoMaterials)
{
    ncl_material::MaterialPreset preset = Parse(R"({"version": 1, "materials": []})");

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

    ncl_material::PresetBudget budget;
    budget.max_materials = 2;

    EXPECT_TRUE(ncl_material::ParseMaterialPreset(WithMaterials(materials), budget).has_error());
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

    ncl_material::PresetBudget budget;
    budget.max_textures = 2;

    EXPECT_TRUE(ncl_material::ParseMaterialPreset(WithMaterials(materials), budget).has_error());
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

    ncl_material::PresetBudget budget;
    budget.max_textures = 1;

    EXPECT_FALSE(ncl_material::ParseMaterialPreset(WithMaterials(materials), budget).has_error());
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
        SCOPED_TRACE(path);
        std::string json = WithMaterials(
            R"({"name": "n", "texture": "t.bmp", "material": {"program": "p", "params": )"
            R"({"uPattern": {"texture": )" +
            tao::json::to_string(tao::json::value(path)) + "}}}}"
        );
        ncl_utils::ResultT<ncl_material::MaterialPreset> result = ncl_material::ParseMaterialPreset(json, ncl_material::PresetBudget{});

        ASSERT_TRUE(result.has_error());
        EXPECT_NE(result.error_str().find("not an admissible texture path"), std::string::npos) << result.error_str();
    }
}

TEST(MaterialPreset, RejectsATexturePathTheLoaderCannotRead)
{
    std::string json = WithMaterials(
        R"({"name": "n", "texture": "t.bmp", "material": {"program": "p", "params": )"
        R"({"uPattern": {"texture": "materials/skins/stripes.bmp"}}}})"
    );

    EXPECT_TRUE(ncl_material::ParseMaterialPreset(json, ncl_material::PresetBudget{}).has_error());
}

TEST(MaterialPreset, WritesAMaterialInTheShapeItReads)
{
    ncl_material::MaterialPreset preset = Parse(kPreset);

    std::string written = R"({"version": 1, "materials": [)" + ncl_material::WritePresetMaterial(preset.materials[0]) + "]}";

    ncl_material::MaterialPreset again = Parse(written);

    ASSERT_EQ(again.materials.size(), 1u);
    EXPECT_EQ(again.materials[0].name, preset.materials[0].name);
    EXPECT_EQ(again.materials[0].texture_name, preset.materials[0].texture_name);
    EXPECT_EQ(again.materials[0].material.program, preset.materials[0].material.program);
    EXPECT_EQ(again.materials[0].material.params.size(), preset.materials[0].material.params.size());
}

TEST(MaterialPreset, RejectsNumbersOutsideTheFiniteFloatRange)
{
    for (const char* value : {"1e100", "-1e100", "[1e100,0,0,0]", "[0,0,0,-1e100]"})
    {
        SCOPED_TRACE(value);
        std::string json = WithMaterials(
            R"({"name":"n","texture":"a.bmp","material":{"program":"weapon_finish","params":{"uDissolveTransform":)" + std::string(value) +
            "}}}"
        );

        EXPECT_TRUE(ncl_material::ParseMaterialPreset(json, ncl_material::PresetBudget{}).has_error());
    }
}

TEST(MaterialPreset, RejectsVersionsThatCannotBeRepresentedAsWholeInts)
{
    for (const char* version : {"1.5", "1e100", "-1e100", "2147483648", "-2147483649"})
    {
        SCOPED_TRACE(version);
        std::string json = R"({"version":)" + std::string(version) + R"(,"materials":[]})";

        EXPECT_TRUE(ncl_material::ParseMaterialPreset(json, ncl_material::PresetBudget{}).has_error());
    }
}
