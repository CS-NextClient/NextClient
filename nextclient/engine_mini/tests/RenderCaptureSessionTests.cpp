#include <gtest/gtest.h>

#include "RendererOwnerTestSupport.h"

TEST(RenderCaptureSession, IndependentSessionsRestoreTheirSuppliedCvarsExactly)
{
    renderer_test::CaptureBackend first_backend;
    renderer_test::CaptureBackend second_backend;
    render::RenderCaptureSession first(first_backend);
    render::RenderCaptureSession second(second_backend);
    goldsrc::cvar_t first_shader{};
    goldsrc::cvar_t first_materials{};
    goldsrc::cvar_t second_shader{};
    first_shader.string = const_cast<char*>("2.2500");
    first_materials.string = const_cast<char*>("003");
    second_shader.string = const_cast<char*>("4e-1");
    first.Begin("first", first_shader, &first_materials);
    second.Begin("second", second_shader, nullptr);
    ASSERT_EQ(first_backend.changes.size(), 2u);
    EXPECT_EQ(first_backend.changes[0].first, &first_materials);
    EXPECT_EQ(first_backend.changes[1].first, &first_shader);
    first.OnSceneEnd();
    EXPECT_EQ(first_backend.captures, (std::vector<std::string>{"first_off"}));
    EXPECT_STREQ(first_shader.string, "1");
    EXPECT_STREQ(second_shader.string, "0");
    first.OnSceneEnd();
    EXPECT_FALSE(first.is_pending());
    EXPECT_TRUE(second.is_pending());
    EXPECT_STREQ(first_shader.string, "2.2500");
    EXPECT_STREQ(first_materials.string, "003");
    EXPECT_EQ(first_backend.reports, 1u);
    second.Cancel();
    EXPECT_STREQ(second_shader.string, "4e-1");
}

TEST(RenderCaptureSession, IdleAndRepeatedCancellationHaveNoExternalWork)
{
    renderer_test::CaptureBackend backend;
    render::RenderCaptureSession capture(backend);
    capture.OnSceneEnd();
    capture.Cancel();
    EXPECT_TRUE(backend.changes.empty());
    EXPECT_TRUE(backend.captures.empty());
    goldsrc::cvar_t shader{};
    shader.string = const_cast<char*>("1.50");
    capture.Begin("failure", shader, nullptr);
    backend.capture_result = false;
    capture.OnSceneEnd();
    capture.Cancel();
    EXPECT_FALSE(capture.is_pending());
    EXPECT_STREQ(shader.string, "1.50");
    EXPECT_EQ(backend.changes.size(), 2u);
    EXPECT_EQ(backend.reports, 0u);
}

TEST(RenderCaptureSession, NestedCancellationDuringSuppressionCannotReapplyOffAfterRestoration)
{
    renderer_test::CaptureBackend backend;
    render::RenderCaptureSession capture(backend);
    goldsrc::cvar_t shader{};
    goldsrc::cvar_t materials{};
    shader.string = const_cast<char*>("2.75");
    materials.string = const_cast<char*>("5.00");
    bool nested = false;
    backend.on_set = [&] {
        if (!nested)
        {
            nested = true;
            capture.Cancel();
        }
    };
    capture.Begin("nested", shader, &materials);
    EXPECT_FALSE(capture.is_pending());
    EXPECT_STREQ(shader.string, "2.75");
    EXPECT_STREQ(materials.string, "5.00");
    EXPECT_EQ(backend.changes.size(), 3u);
}

TEST(RenderCaptureSession, CancellationDuringFramebufferCaptureStopsTheSecondFrame)
{
    renderer_test::CaptureBackend backend;
    render::RenderCaptureSession capture(backend);
    goldsrc::cvar_t shader{};
    shader.string = const_cast<char*>("03");
    capture.Begin("nested", shader, nullptr);
    backend.on_capture = [&] { capture.Cancel(); };
    capture.OnSceneEnd();
    capture.OnSceneEnd();
    EXPECT_FALSE(capture.is_pending());
    EXPECT_STREQ(shader.string, "03");
    EXPECT_EQ(backend.captures.size(), 1u);
    EXPECT_EQ(backend.reports, 0u);
}
