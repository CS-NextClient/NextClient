#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

#include "graphics/render/shader_includes.h"

namespace
{
    using Sources_t = std::unordered_map<std::string, std::string>;

    ncl_utils::ResultT<std::string> Expand(const Sources_t& sources)
    {
        return render::ShaderIncludes_Expand("studio.frag", [&sources](std::string_view name, std::string& out) {
            auto found = sources.find(std::string(name));

            if (found == sources.end())
            {
                return false;
            }

            out = found->second;
            return true;
        });
    }
} // namespace

TEST(ShaderIncludes, ExpandsNestedRelativeFilesAfterTheVersionInOrder)
{
    Sources_t sources = {
        {"studio.frag", "#version 330 compatibility\n#include \"parts/surface.glsl\"\nvoid main() {}\n"},
        {"parts/surface.glsl", "#include \"coverage.glsl\"\nfloat surface;\n"},
        {"parts/coverage.glsl", "float coverage;\n"},
    };
    ncl_utils::ResultT<std::string> result = Expand(sources);
    ASSERT_FALSE(result.has_error()) << result.error_str();
    EXPECT_EQ(*result, "#version 330 compatibility\nfloat coverage;\nfloat surface;\nvoid main() {}\n");
}

TEST(ShaderIncludes, LeavesCommentedIncludesAndNonIncludeDirectivesUntouched)
{
    std::string source =
        "#version 330 compatibility\n/*\n#include \"missing.glsl\"\n*/\n// #include \"also_missing.glsl\"\n#define VALUE 1\n";
    ncl_utils::ResultT<std::string> result = Expand({{"studio.frag", source}});
    ASSERT_FALSE(result.has_error()) << result.error_str();
    EXPECT_EQ(*result, source);
}

TEST(ShaderIncludes, AllowsWhitespaceAndCommentsAroundAnInclude)
{
    ncl_utils::ResultT<std::string> result = Expand({
        {"studio.frag", "#version 330 compatibility\n /* prefix */ # include \"coverage.glsl\" // suffix\n"},
        {"coverage.glsl", "float coverage;\n"},
    });
    ASSERT_FALSE(result.has_error()) << result.error_str();
    EXPECT_NE(result->find("float coverage;"), std::string::npos);
    EXPECT_EQ(result->find("# include"), std::string::npos);
}

TEST(ShaderIncludes, ReportsTheMissingDependencyName)
{
    ncl_utils::ResultT<std::string> result = Expand({{"studio.frag", "#include \"missing.glsl\"\n"}});
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("missing.glsl"), std::string::npos);
}

TEST(ShaderIncludes, PreservesBlockCommentsThatCrossAnIncludeLine)
{
    ncl_utils::ResultT<std::string> result = Expand({
        {"studio.frag",
         "#version 330 compatibility\n/* before\nend */ #include \"coverage.glsl\" /* after\n#include \"missing.glsl\"\n*/\n"},
        {"coverage.glsl", "float coverage;\n"},
    });
    ASSERT_FALSE(result.has_error()) << result.error_str();
    EXPECT_EQ(*result, "#version 330 compatibility\n/* before\n*/\nfloat coverage;\n/*\n#include \"missing.glsl\"\n*/\n");
}

TEST(ShaderIncludes, RejectsInvalidPathsAndMalformedDirectives)
{
    const char* directives[] = {
        "#include \"../coverage.glsl\"\n",
        "#include \"/coverage.glsl\"\n",
        "#include \"D:/coverage.glsl\"\n",
        "#include \"parts\\coverage.glsl\"\n",
        "#include \"parts//coverage.glsl\"\n",
        "#include <coverage.glsl>\n",
        "#include \"coverage.glsl\" trailing\n",
        "#include \"coverage.glsl\n",
    };

    for (const char* directive : directives)
    {
        SCOPED_TRACE(directive);
        EXPECT_TRUE(Expand({{"studio.frag", directive}, {"coverage.glsl", "float coverage;\n"}}).has_error());
    }
}

TEST(ShaderIncludes, RejectsCyclesAndVersionDirectivesInsideDependencies)
{
    EXPECT_TRUE(Expand({{"studio.frag", "#include \"coverage.glsl\"\n"}, {"coverage.glsl", "#include \"studio.frag\"\n"}}).has_error());
    EXPECT_TRUE(Expand({{"studio.frag", "#include \"coverage.glsl\"\n"}, {"coverage.glsl", "#version 330 compatibility\n"}}).has_error());
}

TEST(ShaderIncludes, LimitsIncludeDepthAndExpandedBytes)
{
    Sources_t sources;
    sources["studio.frag"] = "#include \"part0.glsl\"\n";

    for (int index = 0; index < 20; index++)
    {
        sources["part" + std::to_string(index) + ".glsl"] = "#include \"part" + std::to_string(index + 1) + ".glsl\"\n";
    }

    sources["part20.glsl"] = "float coverage;\n";
    EXPECT_TRUE(Expand(sources).has_error());
    sources = {{"studio.frag", std::string(1024 * 1024 + 1, ' ')}};
    EXPECT_TRUE(Expand(sources).has_error());
    sources = {{"studio.frag", "#include \"coverage.glsl\"\n#include \"coverage.glsl\"\n"}, {"coverage.glsl", std::string(600000, ' ')}};
    EXPECT_TRUE(Expand(sources).has_error());
}

TEST(ShaderIncludes, ChangedDependencyContentChangesTheCompilationInput)
{
    Sources_t sources = {{"studio.frag", "#include \"coverage.glsl\"\n"}, {"coverage.glsl", "float coverage = 0.0;\n"}};
    ncl_utils::ResultT<std::string> first = Expand(sources);
    sources["coverage.glsl"] = "float coverage = 1.0;\n";
    ncl_utils::ResultT<std::string> second = Expand(sources);
    ASSERT_FALSE(first.has_error()) << first.error_str();
    ASSERT_FALSE(second.has_error()) << second.error_str();
    EXPECT_NE(*first, *second);
}
