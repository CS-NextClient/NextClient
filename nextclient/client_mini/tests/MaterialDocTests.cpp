#include <gtest/gtest.h>

#include <string>

#include <ncl_material/MaterialParse.h>

using namespace ncl_material;

namespace
{
    constexpr const char* kMinimalDoc = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish" }
        }
    })";

    std::string ErrorOf(std::string_view json)
    {
        ncl_utils::ResultT<MaterialDoc> result = ParseMaterialDoc(json);
        EXPECT_TRUE(result.has_error()) << "expected a parse error";

        return result.has_error() ? result.error_str() : std::string();
    }
}

TEST(MaterialDoc, WritesADocumentTheParserReadsBack)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "params": {
                    "uWear": 0.375,
                    "uWearSoftness": 0.08,
                    "uBaseTint": [1.0, 0.5, 0.25, 1.0],
                    "uWearMap": { "texture": "materials/masks/wear/even.png" }
                },
                "overlays": [ { "name": "left", "rect": [0.1, 0.2, 0.3, 0.4], "rotation": 15.0 } ]
            },
            "ak47_stock.bmp": { "program": "studio_legacy" }
        }
    })";

    ncl_utils::ResultT<MaterialDoc> original = ParseMaterialDoc(json);
    ASSERT_FALSE(original.has_error()) << original.error_str();

    std::string written = WriteMaterialDoc(*original);

    ncl_utils::ResultT<MaterialDoc> reparsed = ParseMaterialDoc(written);
    ASSERT_FALSE(reparsed.has_error()) << reparsed.error_str() << "\n" << written;

    ASSERT_EQ(reparsed->entries.size(), 2u);

    const Material* finish = reparsed->FindByTexture("ak47_body.bmp");
    ASSERT_NE(finish, nullptr);
    EXPECT_EQ(finish->program, "weapon_finish");
    EXPECT_FLOAT_EQ(finish->FindParam("uWear")->scalar, 0.375f);
    EXPECT_FLOAT_EQ(finish->FindParam("uWearSoftness")->scalar, 0.08f);
    EXPECT_FLOAT_EQ(finish->FindParam("uBaseTint")->vec4[1], 0.5f);
    EXPECT_EQ(finish->FindParam("uWearMap")->texture_ref, "materials/masks/wear/even.png");

    ASSERT_EQ(finish->overlays.size(), 1u);
    EXPECT_EQ(finish->overlays[0].name, "left");
    EXPECT_FLOAT_EQ(finish->overlays[0].rotation, 15.0f);

    EXPECT_NE(reparsed->FindByTexture("ak47_stock.bmp"), nullptr);
}

TEST(MaterialDoc, WritesAuthoredNumbersAsAuthored)
{
    // Widening a float to a double is what turns 0.08 into 0.079999998211860657, and a
    // file an author opens after saving is a file an author reads.
    MaterialDoc doc;

    Material material;
    material.program = "weapon_finish";
    material.params.push_back(Param {"uWearSoftness", ParamValue {ParamType::Scalar, 0.08f}});

    doc.entries.push_back(MaterialEntry {"ak47_body.bmp", material});

    EXPECT_NE(WriteMaterialDoc(doc).find("0.08"), std::string::npos) << WriteMaterialDoc(doc);
}

TEST(MaterialDoc, ParsesMinimalDocument)
{
    ncl_utils::ResultT<MaterialDoc> result = ParseMaterialDoc(kMinimalDoc);
    ASSERT_FALSE(result.has_error()) << result.error_str();

    EXPECT_EQ(result->version, kDocVersion);
    ASSERT_EQ(result->entries.size(), 1u);
    EXPECT_EQ(result->entries[0].texture_name, "ak47_body.bmp");
    EXPECT_EQ(result->entries[0].material.program, "weapon_finish");
}

TEST(MaterialDoc, ResolvesTextureNameIgnoringCase)
{
    ncl_utils::ResultT<MaterialDoc> result = ParseMaterialDoc(kMinimalDoc);
    ASSERT_FALSE(result.has_error()) << result.error_str();

    EXPECT_NE(result->FindByTexture("AK47_BODY.BMP"), nullptr);
    EXPECT_NE(result->FindByTexture("Ak47_Body.Bmp"), nullptr);
    EXPECT_EQ(result->FindByTexture("ak47_stock.bmp"), nullptr);
}

TEST(MaterialDoc, ParsesEveryParamForm)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "params": {
                    "layer_mix": 0.25,
                    "base_tint": [1.0, 0.5, 0.25, 1.0],
                    "pattern": { "texture": "materials/weapons/ak47/pattern" }
                }
            }
        }
    })";

    ncl_utils::ResultT<MaterialDoc> result = ParseMaterialDoc(json);
    ASSERT_FALSE(result.has_error()) << result.error_str();

    const Material* material = result->FindByTexture("ak47_body.bmp");
    ASSERT_NE(material, nullptr);

    const ParamValue* scalar = material->FindParam("layer_mix");
    ASSERT_NE(scalar, nullptr);
    EXPECT_EQ(scalar->type, ParamType::Scalar);
    EXPECT_FLOAT_EQ(scalar->scalar, 0.25f);

    const ParamValue* vec4 = material->FindParam("base_tint");
    ASSERT_NE(vec4, nullptr);
    EXPECT_EQ(vec4->type, ParamType::Vec4);
    EXPECT_FLOAT_EQ(vec4->vec4[1], 0.5f);

    const ParamValue* texture = material->FindParam("pattern");
    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(texture->type, ParamType::TextureRef);
    EXPECT_EQ(texture->texture_ref, "materials/weapons/ak47/pattern");
}

TEST(MaterialDoc, ParamNamesAreCaseSensitive)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": 0.25 } }
        }
    })";

    ncl_utils::ResultT<MaterialDoc> result = ParseMaterialDoc(json);
    ASSERT_FALSE(result.has_error()) << result.error_str();

    const Material* material = result->FindByTexture("ak47_body.bmp");
    ASSERT_NE(material, nullptr);
    EXPECT_NE(material->FindParam("layer_mix"), nullptr);
    EXPECT_EQ(material->FindParam("Layer_Mix"), nullptr);
}

TEST(MaterialDoc, ParsesOverlaySlots)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "overlays": [
                    { "name": "left", "rect": [0.1, 0.2, 0.3, 0.4], "rotation": 15.0 },
                    { "name": "right", "rect": [0.5, 0.2, 0.7, 0.4] }
                ]
            }
        }
    })";

    ncl_utils::ResultT<MaterialDoc> result = ParseMaterialDoc(json);
    ASSERT_FALSE(result.has_error()) << result.error_str();

    const Material* material = result->FindByTexture("ak47_body.bmp");
    ASSERT_NE(material, nullptr);
    ASSERT_EQ(material->overlays.size(), 2u);

    const OverlaySlot* left = material->FindOverlaySlot("LEFT");
    ASSERT_NE(left, nullptr);
    EXPECT_FLOAT_EQ(left->rect[2], 0.3f);
    EXPECT_FLOAT_EQ(left->rotation, 15.0f);

    const OverlaySlot* right = material->FindOverlaySlot("right");
    ASSERT_NE(right, nullptr);
    EXPECT_FLOAT_EQ(right->rotation, 0.0f);
}

TEST(MaterialDoc, RejectsUnsupportedVersion)
{
    constexpr const char* json = R"({ "version": 2, "materials": {} })";

    EXPECT_NE(ErrorOf(json).find("document.version"), std::string::npos);
}

TEST(MaterialDoc, RejectsMissingProgram)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": { "ak47_body.bmp": { "params": { "layer_mix": 0.25 } } }
    })";

    std::string error = ErrorOf(json);
    EXPECT_NE(error.find("ak47_body.bmp"), std::string::npos) << error;
    EXPECT_NE(error.find("program"), std::string::npos) << error;
}

TEST(MaterialDoc, RejectsUnknownParamForm)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "layer_mix": "0.25" } }
        }
    })";

    std::string error = ErrorOf(json);
    EXPECT_NE(error.find(R"(params["layer_mix"])"), std::string::npos) << error;
}

TEST(MaterialDoc, RejectsVec4OfWrongLength)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish", "params": { "base_tint": [1.0, 0.5, 0.25] } }
        }
    })";

    std::string error = ErrorOf(json);
    EXPECT_NE(error.find("exactly 4 numbers"), std::string::npos) << error;
}

TEST(MaterialDoc, RejectsOverlayRectOutsideUnitRange)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "overlays": [ { "name": "left", "rect": [0.1, 0.2, 1.3, 0.4] } ]
            }
        }
    })";

    std::string error = ErrorOf(json);
    EXPECT_NE(error.find("[0, 1]"), std::string::npos) << error;
}

TEST(MaterialDoc, RejectsInvertedOverlayRect)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "overlays": [ { "name": "left", "rect": [0.4, 0.2, 0.1, 0.4] } ]
            }
        }
    })";

    std::string error = ErrorOf(json);
    EXPECT_NE(error.find("u0 < u1"), std::string::npos) << error;
}

TEST(MaterialDoc, RejectsDuplicateOverlaySlotName)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": {
                "program": "weapon_finish",
                "overlays": [
                    { "name": "left", "rect": [0.1, 0.2, 0.3, 0.4] },
                    { "name": "LEFT", "rect": [0.5, 0.2, 0.7, 0.4] }
                ]
            }
        }
    })";

    std::string error = ErrorOf(json);
    EXPECT_NE(error.find("duplicate slot name"), std::string::npos) << error;
}

TEST(MaterialDoc, RejectsTooManyOverlaySlots)
{
    std::string json = R"({ "version": 1, "materials": { "ak47_body.bmp": { "program": "p", "overlays": [)";
    for (size_t i = 0; i <= kMaxOverlaySlots; i++)
    {
        if (i != 0)
        {
            json += ",";
        }

        json += R"({ "name": "slot)" + std::to_string(i) + R"(", "rect": [0.0, 0.0, 1.0, 1.0] })";
    }
    json += "] } } }";

    std::string error = ErrorOf(json);
    EXPECT_NE(error.find("too many overlay slots"), std::string::npos) << error;
}

TEST(MaterialDoc, RejectsDuplicateTextureNameIgnoringCase)
{
    constexpr const char* json = R"({
        "version": 1,
        "materials": {
            "ak47_body.bmp": { "program": "weapon_finish" },
            "AK47_Body.bmp": { "program": "weapon_finish" }
        }
    })";

    std::string error = ErrorOf(json);
    EXPECT_NE(error.find("duplicate texture name"), std::string::npos) << error;
}

TEST(MaterialDoc, RejectsMalformedJson)
{
    EXPECT_NE(ErrorOf("{ \"version\": 1, ").find("malformed JSON"), std::string::npos);
}

// Three materials of thirty parameters each: enough distinct files to pass the ceiling
// without approaching the separate ceilings on entries and on parameters.
std::string DocumentWithTextures(bool distinct)
{
    std::string json = R"({"version": 1, "materials": {)";

    for (int entry = 0; entry < 3; entry++)
    {
        json += entry == 0 ? "" : ",";
        json += "\"t" + std::to_string(entry) + R"(.bmp": {"program": "p", "params": {)";

        for (int param = 0; param < 30; param++)
        {
            json += param == 0 ? "" : ",";
            json += "\"a" + std::to_string(param) + "\": {\"texture\": \"";
            json += distinct ? std::to_string(entry * 30 + param) + ".png" : std::string("one.png");
            json += "\"}";
        }

        json += "}}";
    }

    return json + "}}";
}

TEST(MaterialDoc, CountsDistinctTexturesOnlyAgainstTheBudget)
{
    // Ninety references, one file, which is one decode and one upload.
    ncl_utils::ResultT<MaterialDoc> doc = ParseMaterialDoc(DocumentWithTextures(false));

    EXPECT_FALSE(doc.has_error()) << doc.error_str();
}

TEST(MaterialDoc, RejectsTooManyDistinctTextures)
{
    ncl_utils::ResultT<MaterialDoc> doc = ParseMaterialDoc(DocumentWithTextures(true));

    ASSERT_TRUE(doc.has_error());
    EXPECT_NE(doc.error_str().find("distinct textures"), std::string::npos) << doc.error_str();
}
