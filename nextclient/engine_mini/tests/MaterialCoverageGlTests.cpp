#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <ncl_gl/program.h>

#include "RadarGlContext.h"
#include "engine.h"
#include "common/cvar.h"
#include "StudioMaterialProviders.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/shader_includes.h"
#include "graphics/render/shader_manager.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/material/material_manifests.h"
#include "graphics/render/material/StudioProgramContract.h"
#include "graphics/render/StudioSubmit.h"

namespace
{
    constexpr int kCoverageSize = 32;
    constexpr int kCoveragePixels = kCoverageSize * kCoverageSize;
    constexpr const char* kCoverageVertex = R"(
#version 330 compatibility
uniform vec2 uUvOffset;
out vec2 vUv;
out vec3 vColor;
out vec2 vSphereUv;
out vec3 vNormal;
out vec3 vToView;
out vec2 vLightLevel;
void main()
{
    vec2 position = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
    vUv = position + uUvOffset;
    vColor = vec3(1.0);
    vSphereUv = vec2(0.5);
    vNormal = vec3(0.0, 0.0, 1.0);
    vToView = vec3(0.0, 0.0, 1.0);
    vLightLevel = vec2(1.0);
}
)";

    ncl_utils::ResultT<std::string> LoadCoverageShader(const std::string& name)
    {
        return render::ShaderIncludes_Expand(name, [](std::string_view dependency, std::string& text) {
            return render::Shaders_LoadSource(std::string(dependency).c_str(), text);
        });
    }

    using ShaderFunctions_t = std::unordered_map<std::string, std::vector<std::string>>;

    size_t FindClosingToken(const std::vector<std::string>& tokens, size_t opening, const char* begin, const char* end)
    {
        int depth = 0;

        for (size_t index = opening; index < tokens.size(); index++)
        {
            depth += tokens[index] == begin ? 1 : 0;
            depth -= tokens[index] == end ? 1 : 0;

            if (depth == 0)
            {
                return index;
            }
        }

        return tokens.size();
    }

    std::vector<std::string> ShaderOperations(const std::string& source)
    {
        const std::regex token_pattern(R"(//[^\n]*|/\*[\s\S]*?\*/|[A-Za-z_][A-Za-z_0-9]*|[^\s])");
        std::vector<std::string> tokens;

        for (auto token = std::sregex_iterator(source.begin(), source.end(), token_pattern); token != std::sregex_iterator(); ++token)
        {
            std::string text = token->str();

            if (!text.starts_with("//") && !text.starts_with("/*"))
            {
                tokens.push_back(text);
            }
        }

        ShaderFunctions_t functions;

        for (size_t index = 1; index < tokens.size(); index++)
        {
            if (tokens[index] != "(")
            {
                continue;
            }

            size_t parameters_end = FindClosingToken(tokens, index, "(", ")");

            if (parameters_end + 1 >= tokens.size() || tokens[parameters_end + 1] != "{")
            {
                continue;
            }

            size_t body_end = FindClosingToken(tokens, parameters_end + 1, "{", "}");
            functions[tokens[index - 1]] = std::vector<std::string>(tokens.begin() + parameters_end + 2, tokens.begin() + body_end);
            index = body_end;
        }

        EXPECT_NE(functions.find("main"), functions.end());
        std::vector<std::string> operations;
        const auto visit = [&functions, &operations](const auto& self, const std::string& name, size_t depth) -> void {
            if (depth > functions.size())
            {
                ADD_FAILURE() << "Recursive shader call: " << name;
                return;
            }

            auto function = functions.find(name);

            if (function == functions.end())
            {
                return;
            }

            const std::vector<std::string>& body = function->second;

            for (size_t index = 0; index < body.size(); index++)
            {
                const std::string& token = body[index];

                if (token == "discard")
                {
                    operations.push_back(token);
                }
                else if (index + 1 < body.size() && body[index + 1] == "(")
                {
                    if (token == "texture" || token == "textureProj" || token == "textureOffset" || token == "textureProjOffset" ||
                        token == "dFdx" || token == "dFdy" || token == "fwidth")
                    {
                        operations.push_back(token);
                    }
                    else
                    {
                        self(self, token, depth + 1);
                    }
                }
            }
        };
        visit(visit, "main", 0);

        return operations;
    }

    std::vector<std::string> DerivativeReadsAfterDiscard(const std::vector<std::string>& operations)
    {
        bool discarded = false;
        std::vector<std::string> violations;

        // Conservatively visits both sides of branches, including calls into expanded includes.
        for (const std::string& operation : operations)
        {
            if (operation == "discard")
            {
                discarded = true;
            }
            else if (discarded)
            {
                violations.push_back(operation);
            }
        }

        return violations;
    }

    std::vector<uint8_t> CoverageMask(const std::vector<uint8_t>& pixels)
    {
        std::vector<uint8_t> coverage;

        for (size_t index = 3; index < pixels.size(); index += 4)
        {
            coverage.push_back(pixels[index] > 128 ? 1 : 0);
        }

        return coverage;
    }

    class MaterialCoverageGlTest : public testing::TestWithParam<const char*>
    {
    private:
        RadarGlContext context_;
        goldsrc::cvar_t developer_{};

    protected:
        ncl_gl::Program program_{};

    private:
        GLuint framebuffer_{};
        GLuint image_{};
        GLuint base_{};
        GLuint mask_{};
        GLuint vao_{};
        std::string fragment_;
        bool context_ready_{};

    protected:
        StudioMaterialSourceFixture source_;
        performance_test::Core coverage_performance_;
        render::MaterialCoverageRuntime coverage_{source_.catalog, source_.cache, coverage_performance_.monitor};
        render::MaterialCoverageBindings bindings_;

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            context_ready_ = true;
            developer = &developer_;
            source_.device.OnContextCreated();
            for (const char* name : {"studio", "weapon_finish"})
            {
                std::string json;
                ASSERT_TRUE(render::GetShaderBackend().LoadSource((std::string(name) + ".json").c_str(), json));
                ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(json);
                ASSERT_FALSE(manifest.has_error());
                source_.manifests[name] = *manifest;
            }
            render::g_RenderCfg = {};
            ncl_utils::ResultT<std::string> source = LoadCoverageShader(std::string(GetParam()) + ".frag");
            ASSERT_FALSE(source.has_error()) << source.error_str();
            fragment_ = WithDefines(*source);
            ncl_gl::LoadProgram(program_, kCoverageVertex, fragment_.c_str(), nullptr, 0, nullptr, 0, &error);
            ASSERT_TRUE(program_.ok) << error;
            bindings_ = render::MaterialCoverageBindings::Prepare(program_);
            glUseProgram(program_.prog);

            glGenVertexArrays(1, &vao_);
            glBindVertexArray(vao_);
            glGenTextures(1, &image_);
            glBindTexture(GL_TEXTURE_2D, image_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer_);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image_, 0);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_BLEND);
            glDisable(GL_CULL_FACE);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_STENCIL_TEST);
            glDisable(GL_DITHER);
            glDisable(GL_FRAMEBUFFER_SRGB);

            glGenTextures(1, &base_);
            glBindTexture(GL_TEXTURE_2D, base_);
            const uint8_t base[] = {128, 64, 32, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, base);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glUniform1i(glGetUniformLocation(program_.prog, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(program_.prog, "uBlend"), 1.0f);
            glUniform4f(glGetUniformLocation(program_.prog, "uBaseTint"), 1, 1, 1, 1);
            glUniform4f(glGetUniformLocation(program_.prog, "uPatternTint"), 1, 1, 1, 1);

            std::array<uint8_t, kCoverageSize * 4> gradient{};

            for (int column = 0; column < kCoverageSize; column++)
            {
                gradient[column * 4] = static_cast<uint8_t>(column * 255 / (kCoverageSize - 1));
                gradient[column * 4 + 3] = 255;
            }

            glGenTextures(1, &mask_);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, mask_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kCoverageSize, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gradient.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glUniform1i(glGetUniformLocation(program_.prog, "uDissolveMap"), 3);
            glUniform4f(glGetUniformLocation(program_.prog, "uDissolveTransform"), 1, 1, 0, 0);
            glActiveTexture(GL_TEXTURE0);
            Set("uNoiseScale", 256.0f);
        }

        void TearDown() override
        {
            if (!context_ready_)
            {
                return;
            }

            glUseProgram(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindVertexArray(0);
            glDeleteProgram(program_.prog);
            glDeleteFramebuffers(1, &framebuffer_);
            const GLuint textures[] = {image_, base_, mask_};
            glDeleteTextures(3, textures);
            glDeleteVertexArrays(1, &vao_);
            source_.device.OnContextDestroyed();
            developer = nullptr;
        }

        void Set(const char* name, float value)
        {
            glUniform1f(glGetUniformLocation(program_.prog, name), value);
        }

        void UploadCoverage(const ncl_material::Material* material)
        {
            render::StudioMaterialState textures;
            UploadPreparedCoverage(
                coverage_, render::MaterialCoverageBindings::Prepare(program_), material, {0, 0, kCoverageSize, kCoverageSize}, 0, textures
            );
            textures.Restore();
        }

        void SetUvOffset(float x, float y)
        {
            glUniform2f(glGetUniformLocation(program_.prog, "uUvOffset"), x, y);
        }

        void SetBaseAlpha(uint8_t alpha)
        {
            const uint8_t base[] = {128, 64, 32, alpha};
            glBindTexture(GL_TEXTURE_2D, base_);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, base);
        }

        void SetMipColoredBase(bool masked = false)
        {
            glBindTexture(GL_TEXTURE_2D, base_);

            for (int level = 0, size = 256; size > 0; level++, size /= 2)
            {
                std::vector<uint8_t> texels(size * size * 4);

                for (int y = 0; y < size; y++)
                {
                    for (int x = 0; x < size; x++)
                    {
                        int index = (y * size + x) * 4;
                        texels[index] = static_cast<uint8_t>(20 + x % 8 * 25);
                        texels[index + 1] = static_cast<uint8_t>(10 + level * 20);
                        texels[index + 2] = static_cast<uint8_t>(20 + (x + 3) % 8 * 25);
                        texels[index + 3] = masked && (x / 4) % 2 != 0 ? 64 : 255;
                    }
                }

                glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data());
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        std::vector<uint8_t> RenderAuxiliaryCoverage(const char* name, float alpha_scale)
        {
            ncl_utils::ResultT<std::string> source = LoadCoverageShader(std::string(name) + ".frag");
            EXPECT_FALSE(source.has_error()) << source.error_str();

            if (source.has_error())
            {
                return {};
            }

            std::string error;
            GLuint auxiliary = ncl_gl::LinkProgram(kCoverageVertex, source->c_str(), nullptr, 0, &error);
            EXPECT_NE(auxiliary, 0u) << error;

            if (auxiliary == 0)
            {
                return {};
            }

            glUseProgram(auxiliary);
            const char* scalars[] = {
                "uDissolve", "uDissolveSpace", "uNoise", "uNoiseSpace", "uNoiseScale", "uNoiseSeed", "uNoiseSpeed", "uEffectTime"
            };

            for (const char* scalar : scalars)
            {
                float value = 0;
                glGetUniformfv(program_.prog, glGetUniformLocation(program_.prog, scalar), &value);
                glUniform1f(glGetUniformLocation(auxiliary, scalar), value);
            }

            float transform[4]{};
            glGetUniformfv(program_.prog, glGetUniformLocation(program_.prog, "uDissolveTransform"), transform);
            glUniform4fv(glGetUniformLocation(auxiliary, "uDissolveTransform"), 1, transform);
            glUniform4f(glGetUniformLocation(auxiliary, "uEffectViewport"), 0, 0, kCoverageSize, kCoverageSize);
            glUniform1i(glGetUniformLocation(auxiliary, "uDissolveMap"), 3);
            glUniform1i(glGetUniformLocation(auxiliary, "uBase"), 0);
            glUniform1i(glGetUniformLocation(auxiliary, "uMasked"), 1);
            glUniform1f(glGetUniformLocation(auxiliary, "uAlphaScale"), alpha_scale);

            bool picking = std::string(name) == "preview_pick";

            if (picking)
            {
                glUniform1ui(glGetUniformLocation(auxiliary, "uId"), 1);
            }
            else
            {
                glUniform4f(glGetUniformLocation(auxiliary, "uFill"), 1, 1, 1, 1);
            }

            GLuint target = 0;
            GLuint framebuffer = 0;
            glGenTextures(1, &target);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, target);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                picking ? GL_R32UI : GL_RGBA8,
                kCoverageSize,
                kCoverageSize,
                0,
                picking ? GL_RED_INTEGER : GL_RGBA,
                picking ? GL_UNSIGNED_INT : GL_UNSIGNED_BYTE,
                nullptr
            );
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
            EXPECT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            glDisable(GL_ALPHA_TEST);
            const GLuint clear_id[] = {0, 0, 0, 0};
            const GLfloat clear_color[] = {0, 0, 0, 0};

            if (picking)
            {
                glClearBufferuiv(GL_COLOR, 0, clear_id);
            }
            else
            {
                glClearBufferfv(GL_COLOR, 0, clear_color);
            }

            glViewport(0, 0, kCoverageSize, kCoverageSize);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            std::vector<uint8_t> coverage(kCoveragePixels);

            if (picking)
            {
                std::vector<GLuint> ids(kCoveragePixels);
                glReadPixels(0, 0, kCoverageSize, kCoverageSize, GL_RED_INTEGER, GL_UNSIGNED_INT, ids.data());
                std::transform(ids.begin(), ids.end(), coverage.begin(), [](GLuint id) { return id != 0 ? 1 : 0; });
            }
            else
            {
                std::vector<uint8_t> pixels(kCoveragePixels * 4);
                glReadPixels(0, 0, kCoverageSize, kCoverageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                coverage = CoverageMask(pixels);
            }

            glDeleteFramebuffers(1, &framebuffer);
            glDeleteTextures(1, &target);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glActiveTexture(GL_TEXTURE0);
            glUseProgram(program_.prog);
            EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));

            return coverage;
        }

        std::vector<uint8_t> Render(int x = 0, int y = 0)
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glViewport(x, y, kCoverageSize, kCoverageSize);
            glUniform4f(
                glGetUniformLocation(program_.prog, "uEffectViewport"),
                static_cast<float>(x),
                static_cast<float>(y),
                kCoverageSize,
                kCoverageSize
            );
            glDrawArrays(GL_TRIANGLES, 0, 3);
            std::vector<uint8_t> pixels(kCoveragePixels * 4);
            glReadPixels(x, y, kCoverageSize, kCoverageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
            return pixels;
        }

        int Count(const std::vector<uint8_t>& pixels)
        {
            int count = 0;

            for (size_t index = 3; index < pixels.size(); index += 4)
            {
                count += pixels[index] > 128 ? 1 : 0;
            }

            return count;
        }

        void CheckProductionVertex()
        {
            ncl_utils::ResultT<std::string> vertex = LoadCoverageShader(std::string(GetParam()) + ".vert");
            ASSERT_FALSE(vertex.has_error()) << vertex.error_str();
            std::string error;
            std::string source = WithDefines(*vertex);
            GLuint program = ncl_gl::LinkProgram(source.c_str(), fragment_.c_str(), nullptr, 0, &error);
            EXPECT_NE(program, 0u) << error;
            glDeleteProgram(program);
        }

    private:
        std::string WithDefines(std::string source)
        {
            source.insert(source.find('\n') + 1, render::Studio_ShaderDefines());
            return source;
        }
    };
} // namespace

TEST(MaterialCoverageShaderContractTest, DerivativeReadsPrecedeDiscardAcrossIncludedFunctions)
{
    for (const char* name : {"studio.frag", "weapon_finish.frag", "preview_pick.frag", "studio_effect_mask.frag"})
    {
        SCOPED_TRACE(name);
        ncl_utils::ResultT<std::string> source = LoadCoverageShader(name);
        ASSERT_FALSE(source.has_error()) << source.error_str();
        std::vector<std::string> operations = ShaderOperations(*source);
        ASSERT_GT(std::count(operations.begin(), operations.end(), "discard"), 0);
        ASSERT_GT(operations.size(), std::count(operations.begin(), operations.end(), "discard"));
        std::vector<std::string> violations = DerivativeReadsAfterDiscard(operations);
        EXPECT_TRUE(violations.empty()) << testing::PrintToString(violations);
    }
}

TEST(MaterialCoverageShaderContractTest, RejectsEarlyCoverageDiscardInACalledHelper)
{
    for (const char* name : {"studio.frag", "weapon_finish.frag", "preview_pick.frag", "studio_effect_mask.frag"})
    {
        SCOPED_TRACE(name);
        ncl_utils::ResultT<std::string> loaded = LoadCoverageShader(name);
        ASSERT_FALSE(loaded.has_error()) << loaded.error_str();
        std::string source = *loaded;
        size_t main = source.find("void main()");
        ASSERT_NE(main, std::string::npos);
        size_t body = source.find('{', main);
        ASSERT_NE(body, std::string::npos);
        source.insert(body + 1, "\nRegression_EarlyCoverage();\n");
        source.insert(main, "void Regression_EarlyCoverage() { if (vUv.x < 0.5) { discard; } }\n");

        EXPECT_FALSE(DerivativeReadsAfterDiscard(ShaderOperations(source)).empty());
    }
}

TEST_P(MaterialCoverageGlTest, PartialCoveragePreservesMinifiedMipColorsWithRgbSplit)
{
    SetMipColoredBase();
    std::vector<uint8_t> unsplit = Render();

    for (int pixel = 0; pixel < kCoveragePixels; pixel++)
    {
        ASSERT_EQ(unsplit[pixel * 4 + 1], 70) << pixel;
    }

    Set("uGlitchAmount", 1.0f);
    Set("uGlitchRgbSplit", 4.0f);
    Set("uGlitchBands", 8.0f);
    Set("uGlitchSeed", 3.0f);
    std::vector<uint8_t> split = Render();
    ASSERT_NE(split, unsplit);
    Set("uDissolve", 0.35f);
    Set("uNoise", 0.4f);
    std::vector<uint8_t> covered = Render();
    ASSERT_GT(Count(covered), 200);
    ASSERT_LT(Count(covered), 600);

    for (int pixel = 0; pixel < kCoveragePixels; pixel++)
    {
        if (covered[pixel * 4 + 3] != 0)
        {
            EXPECT_EQ(covered[pixel * 4 + 1], 70) << pixel;

            for (int channel = 0; channel < 4; channel++)
            {
                EXPECT_EQ(covered[pixel * 4 + channel], split[pixel * 4 + channel]) << pixel << ":" << channel;
            }
        }
    }
}

TEST_P(MaterialCoverageGlTest, MinifiedMaskedPickingAndEffectCoverageMatchColor)
{
    SetMipColoredBase(true);
    Set("uBlend", 0.75f);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    ASSERT_EQ(Count(Render()), kCoveragePixels / 2);
    Set("uDissolve", 0.35f);
    Set("uNoise", 0.4f);
    Set("uNoiseSeed", 7.0f);
    Set("uNoiseSpace", 1.0f);
    std::vector<uint8_t> color = CoverageMask(Render());
    ASSERT_GT(std::count(color.begin(), color.end(), 1), 100);
    ASSERT_LT(std::count(color.begin(), color.end(), 1), 400);
    EXPECT_EQ(RenderAuxiliaryCoverage("preview_pick", 0.75f), color);
    EXPECT_EQ(RenderAuxiliaryCoverage("studio_effect_mask", 0.75f), color);
}

TEST_P(MaterialCoverageGlTest, DisabledParityAndCompleteDissolveOrNoise)
{
    std::vector<uint8_t> pixels = Render();
    EXPECT_EQ(Count(pixels), kCoveragePixels);
    EXPECT_EQ(pixels[0], 128);
    EXPECT_EQ(pixels[1], 64);
    EXPECT_EQ(pixels[2], 32);
    Set("uDissolve", 1.0f);
    EXPECT_EQ(Count(Render()), 0);
    Set("uDissolve", 0.0f);
    Set("uNoise", 1.0f);
    EXPECT_EQ(Count(Render()), 0);
}

TEST_P(MaterialCoverageGlTest, ThresholdAndNoiseIntersectTheirSurvivingPixels)
{
    Set("uDissolve", 0.5f);
    EXPECT_EQ(Count(Render()), kCoveragePixels / 2);
    Set("uNoise", 0.5f);
    int count = Count(Render());
    EXPECT_GT(count, 200);
    EXPECT_LT(count, 312);
}

TEST_P(MaterialCoverageGlTest, SeedAndSpeedControlTheNoisePattern)
{
    Set("uNoise", 0.5f);
    Set("uNoiseSeed", 7.0f);
    std::vector<uint8_t> frozen = Render();
    int count = Count(frozen);
    EXPECT_GT(count, 400);
    EXPECT_LT(count, 624);
    Set("uEffectTime", 10.0f);
    EXPECT_EQ(Render(), frozen);
    Set("uNoiseSeed", 8.0f);
    EXPECT_NE(Render(), frozen);
    Set("uNoiseSeed", 7.0f);
    Set("uNoiseSpeed", 4.0f);
    Set("uEffectTime", 0.25f);
    std::vector<uint8_t> animated = Render();
    EXPECT_NE(animated, frozen);
    EXPECT_EQ(Render(), animated);
}

TEST_P(MaterialCoverageGlTest, ScreenNoiseTracksTheViewportAndIgnoresSurfaceUvs)
{
    Set("uNoise", 0.5f);
    Set("uNoiseSpace", 1.0f);
    std::vector<uint8_t> first = Render();
    EXPECT_EQ(Render(5, 7), first);
    SetUvOffset(0.31f, 0.23f);
    EXPECT_EQ(Render(5, 7), first);
}

TEST_P(MaterialCoverageGlTest, UvNoiseTracksTheSurfaceCoordinates)
{
    Set("uNoise", 0.5f);
    std::vector<uint8_t> first = Render();
    SetUvOffset(0.31f, 0.23f);
    EXPECT_NE(Render(), first);
}

TEST_P(MaterialCoverageGlTest, ScreenDissolveIsIndependentFromSurfaceUvs)
{
    Set("uDissolve", 0.5f);
    Set("uDissolveSpace", 1.0f);
    std::vector<uint8_t> first = Render();
    EXPECT_EQ(Count(first), kCoveragePixels / 2);
    SetUvOffset(0.25f, 0.0f);
    EXPECT_EQ(Render(), first);
    Set("uDissolveSpace", 0.0f);
    EXPECT_NE(Render(), first);
}

TEST_P(MaterialCoverageGlTest, SourceAlphaStillControlsMaskedPixels)
{
    SetBaseAlpha(64);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    EXPECT_EQ(Count(Render()), 0);
}

TEST_P(MaterialCoverageGlTest, LinksWithTheProductionStudioVertex)
{
    CheckProductionVertex();
}

TEST_P(MaterialCoverageGlTest, CheckedProfileResetsCoverageBetweenAuthoredOrdinaryAndNullMaterials)
{
    ncl_material::Material material;
    material.program = GetParam();
    const ncl_material::ProgramManifest* manifest = coverage_.ManifestFor(&material);
    ASSERT_NE(render::StudioContract_For(&material, manifest), nullptr);
    ASSERT_NE(manifest, nullptr);
    ASSERT_FALSE(render::StudioContract_Validate(*manifest).has_error());
    material.params.push_back({"uDissolve", {}});
    material.params.back().value.scalar = 1;
    UploadCoverage(&material);
    EXPECT_EQ(Count(Render()), 0);

    ncl_material::Material ordinary;
    ordinary.program = "studio_legacy";
    ordinary.params = material.params;
    UploadCoverage(&ordinary);
    EXPECT_EQ(Count(Render()), kCoveragePixels);

    UploadCoverage(&material);
    EXPECT_EQ(Count(Render()), 0);
    UploadCoverage(nullptr);
    EXPECT_EQ(Count(Render()), kCoveragePixels);
}

INSTANTIATE_TEST_SUITE_P(Programs, MaterialCoverageGlTest, testing::Values("studio", "weapon_finish"));

TEST_P(MaterialCoverageGlTest, IndependentOwnersLoadImagesBeforeBindingAndRestoreTheSuppliedTextureState)
{
    class CoverageImages final : public StudioMaterialSourceFixture
    {
    public:
        GLuint original{};
        unsigned int checked{};

        GLuint Load(const char* path, tex::TextureLoadGuard) override
        {
            CheckBinding();
            return StudioMaterialSourceFixture::Load(path, {});
        }

        GLuint CreateNeutral() override
        {
            CheckBinding();
            return StudioMaterialSourceFixture::CreateNeutral();
        }

    private:
        void CheckBinding()
        {
            GLint active = 0;
            GLint bound = 0;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
            glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
            EXPECT_EQ(bound, static_cast<GLint>(original));
            glActiveTexture(active);
            ++checked;
        }
    };

    CoverageImages first;
    CoverageImages second;
    first.manifests[GetParam()] = source_.manifests[GetParam()];
    second.manifests[GetParam()] = source_.manifests[GetParam()];
    performance_test::Core first_coverage_performance;
    render::MaterialCoverageRuntime first_coverage(first.catalog, first.cache, first_coverage_performance.monitor);
    performance_test::Core second_coverage_performance;
    render::MaterialCoverageRuntime second_coverage(second.catalog, second.cache, second_coverage_performance.monitor);
    GLuint images[3]{};
    glGenTextures(3, images);
    first.original = second.original = images[0];
    first.textures["fixtures/coverage.png"] = images[1];
    second.neutral = images[2];
    glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit);
    glBindTexture(GL_TEXTURE_2D, images[0]);
    glActiveTexture(GL_TEXTURE0);
    ncl_material::Material material;
    material.program = GetParam();
    material.params.push_back({"uDissolve", {}});
    material.params.back().value.scalar = 0.5f;
    material.params.push_back({"uDissolveMap", {}});
    material.params.back().value.type = ncl_material::ParamType::TextureRef;
    material.params.back().value.texture_ref = "fixtures/coverage.png";

    for (int index = 0; index < 2; ++index)
    {
        render::StudioMaterialState state;
        render::MaterialCoverageRuntime& coverage = index == 0 ? first_coverage : second_coverage;
        UploadPreparedCoverage(coverage, bindings_, &material, {1, 2, 30, 31}, 7, state);
        glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit);
        GLint bound = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        EXPECT_EQ(bound, static_cast<GLint>(images[index + 1]));
        state.Restore();
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        EXPECT_EQ(bound, static_cast<GLint>(images[0]));
        glActiveTexture(GL_TEXTURE0);
    }

    EXPECT_EQ(first.checked, 1u);
    EXPECT_EQ(second.checked, 2u);
    EXPECT_EQ(first.texture_queries, (std::vector<std::string>{"fixtures/coverage.png"}));
    EXPECT_EQ(second.texture_queries, first.texture_queries);
    EXPECT_EQ(first.neutral_queries, 0u);
    EXPECT_EQ(second.neutral_queries, 1u);
    float time = 0;
    glGetUniformfv(program_.prog, program_.UniformLocation("uEffectTime"), &time);
    EXPECT_FLOAT_EQ(time, 7);
    float scale = 0;
    glGetUniformfv(program_.prog, program_.UniformLocation("uNoiseScale"), &scale);
    EXPECT_FLOAT_EQ(scale, 256);
    first.device.OnContextDestroyed();
    second.device.OnContextDestroyed();
    glDeleteTextures(3, images);
}
