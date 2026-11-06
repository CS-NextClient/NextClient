#include <string_view>

#include <cvars/CvarDefinition.h>
#include <cvars/cvar_defaults.h>
#include <gtest/gtest.h>

#include "next_client_mini/radar_cvars.h"

namespace
{
    constexpr cvars::CvarDefinition kDefinitions[] = {
        {"test_scale", "1.5", -2.0f, 3.0f, 8},
        {"test_scale_max", "2", 0.0f, 4.0f, 16},
    };

    constexpr cvars::CvarDefinition kCompileTime = cvars::FindDefinition(kDefinitions, "test_scale");
    static_assert(kCompileTime.flags == 8);
    static_assert(cvars::FindDefinition(kDefinitions, "unknown").name == nullptr);
} // namespace

TEST(CvarDefinition, LookupPreservesTheWholeOwnerDefinition)
{
    cvars::CvarDefinition definition = cvars::FindDefinition(kDefinitions, "test_scale_max");

    ASSERT_NE(definition.name, nullptr);
    EXPECT_STREQ(definition.default_value, "2");
    EXPECT_FLOAT_EQ(definition.min_value, 0.0f);
    EXPECT_FLOAT_EQ(definition.max_value, 4.0f);
    EXPECT_EQ(definition.flags, 16);
}

TEST(CvarDefinition, LookupUsesTheExactViewLength)
{
    std::string_view name = "test_scale_max";
    cvars::CvarDefinition definition = cvars::FindDefinition(kDefinitions, name.substr(0, 10));

    EXPECT_STREQ(definition.name, "test_scale");
    EXPECT_EQ(cvars::FindDefinition(kDefinitions, "test_scal").name, nullptr);
    EXPECT_EQ(cvars::FindDefinition(kDefinitions, "TEST_SCALE").name, nullptr);
}

TEST(CvarDefinition, MissingNameHasNoDefaultRangeOrFlags)
{
    cvars::CvarDefinition definition = cvars::FindDefinition(kDefinitions, "missing");

    EXPECT_EQ(definition.name, nullptr);
    EXPECT_EQ(definition.default_value, nullptr);
    EXPECT_FLOAT_EQ(definition.min_value, 0.0f);
    EXPECT_FLOAT_EQ(definition.max_value, 0.0f);
    EXPECT_EQ(definition.flags, 0);
}

TEST(CvarDefinition, GameplayDefaultsUseTheSameLookupContract)
{
    EXPECT_STREQ(cvars::FindDefault("viewmodel_fov"), "90.000000");
    EXPECT_EQ(cvars::FindDefault("viewmodel_fov_extra"), nullptr);
}

TEST(CvarDefinition, RadarSettingsRemainArchived)
{
    for (const cvars::CvarDefinition& definition : radar::kCvarDefinitions)
    {
        SCOPED_TRACE(definition.name);
        EXPECT_NE(radar::FindCvar(definition.name).flags & goldsrc::FCVAR_ARCHIVE, 0);
    }
}
