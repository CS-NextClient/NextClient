#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>

#include "graphics/render/shader_manager.h"
#include "graphics/render/material/StudioProgramContract.h"

namespace
{
    ncl_material::ProgramManifest ThirdProgram()
    {
        std::string json;
        if (!render::Shaders_LoadSource("studio.json", json))
        {
            throw std::runtime_error("embedded studio.json is unavailable");
        }
        ncl_utils::ResultT<ncl_material::ProgramManifest> parsed = ncl_material::ParseProgramManifest(json);
        if (parsed.has_error())
        {
            throw std::runtime_error(parsed.error_str());
        }

        ncl_material::ProgramManifest manifest = *parsed;
        manifest.program = "third_embedded_program";
        manifest.render_contract = "studio_effects_v1";
        return manifest;
    }

    ncl_material::ParamSchema& Param(ncl_material::ProgramManifest& manifest, const char* name)
    {
        for (ncl_material::ParamSchema& schema : manifest.parameters)
        {
            if (schema.name == name)
            {
                return schema;
            }
        }

        throw std::runtime_error(std::string("missing semantic parameter: ") + name);
    }
} // namespace

TEST(StudioProgramContractTest, AcceptsThirdProgramWithSharedSemantics)
{
    EXPECT_FALSE(render::StudioContract_Validate(ThirdProgram()).has_error());
}

TEST(StudioProgramContractTest, RejectsUnknownProfile)
{
    ncl_material::ProgramManifest manifest = ThirdProgram();
    manifest.render_contract = "studio_effects_v2";
    EXPECT_TRUE(render::StudioContract_Validate(manifest).has_error());
}

TEST(StudioProgramContractTest, RejectsSemanticDeclarationDrift)
{
    ncl_material::ProgramManifest manifest = ThirdProgram();
    Param(manifest, "uBaseTint").type = ncl_material::ParamType::Scalar;
    EXPECT_TRUE(render::StudioContract_Validate(manifest).has_error());

    manifest = ThirdProgram();
    Param(manifest, "uNoiseScale").default_value.scalar = 128;
    EXPECT_TRUE(render::StudioContract_Validate(manifest).has_error());

    manifest = ThirdProgram();
    Param(manifest, "uOutlineWidth").max_value = 32;
    EXPECT_TRUE(render::StudioContract_Validate(manifest).has_error());

    manifest = ThirdProgram();
    Param(manifest, "uOccludedAmount").instance = false;
    EXPECT_TRUE(render::StudioContract_Validate(manifest).has_error());
}

TEST(StudioProgramContractTest, AllowsFinitePositiveRestrictedDefaults)
{
    ncl_material::ProgramManifest manifest = ThirdProgram();
    Param(manifest, "uOccludedAmount").default_value.scalar = 0.1234567f;
    Param(manifest, "uOutlineWidth").default_value.scalar = 2;
    EXPECT_FALSE(render::StudioContract_Validate(manifest).has_error());

    Param(manifest, "uOutlineWidth").default_value.scalar = 0.1f;
    EXPECT_FALSE(render::StudioContract_Validate(manifest).has_error());
}

TEST(StudioProgramContractTest, AcceptsOrdinaryManifestWithoutSpecializedSemantics)
{
    ncl_material::ProgramManifest manifest;
    manifest.program = "ordinary";
    EXPECT_FALSE(render::StudioContract_Validate(manifest).has_error());
    EXPECT_EQ(render::StudioContract_For(nullptr), nullptr);
}

TEST(StudioProgramContractTest, EmbeddedProgramsUseTheSameCheckedProfileAndPickingDefaults)
{
    for (const char* program : {"studio", "weapon_finish"})
    {
        ncl_material::Material material;
        material.program = program;
        const render::StudioProgramContract* contract = render::StudioContract_For(&material);
        ASSERT_NE(contract, nullptr) << program;
        EXPECT_EQ(contract, &render::StudioContract_EffectsV1());
        EXPECT_EQ(contract->coverage_profile, render::StudioCoverageProfile::DissolveNoiseGlitchV1);
        EXPECT_FLOAT_EQ(render::StudioContract_PickingAlpha(&material, 0.5f), 0.5f);
        ncl_material::Param tint;
        tint.name = contract->get_parameter(render::StudioParameterRole::BaseAlpha).name;
        tint.value.type = ncl_material::ParamType::Vec4;
        tint.value.vec4 = {1, 1, 1, 0.25f};
        material.params.push_back(tint);
        EXPECT_FLOAT_EQ(render::StudioContract_PickingAlpha(&material, 0.5f), 0.125f);
    }

    ncl_material::Material legacy;
    legacy.program = "studio_legacy";
    EXPECT_EQ(render::StudioContract_For(&legacy), nullptr);
    EXPECT_FLOAT_EQ(render::StudioContract_PickingAlpha(nullptr, 0.5f), 0.5f);
}

TEST(StudioProgramContractTest, RejectsMissingSemanticsAndNonfiniteRestrictedDefaults)
{
    ncl_material::ProgramManifest manifest = ThirdProgram();
    manifest.parameters.clear();
    EXPECT_TRUE(render::StudioContract_Validate(manifest).has_error());

    for (const char* name : {"uOccludedAmount", "uOutlineWidth"})
    {
        for (float invalid :
             {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
        {
            manifest = ThirdProgram();
            Param(manifest, name).default_value.scalar = invalid;
            EXPECT_TRUE(render::StudioContract_Validate(manifest).has_error());
        }
    }

    manifest = ThirdProgram();
    Param(manifest, "uOutlineWidth").default_value.scalar = 17;
    EXPECT_TRUE(render::StudioContract_Validate(manifest).has_error());
}
