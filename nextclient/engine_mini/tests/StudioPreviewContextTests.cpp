#include <gtest/gtest.h>

#include <goldsrc/entity.h>

#include "graphics/render/StudioPreviewContext.h"
#include "graphics/render/RendererState.h"

TEST(StudioPreviewContext, WorldCameraOwnsChromeOriginAndViewBasis)
{
    goldsrc::cl_entity_t entity{};
    PreviewDrawParams params{};
    params.camera.origin[0] = 25.0f;
    params.camera.angles[1] = 90.0f;
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.materials_enabled = false;
    PreviewModel model{};
    model.scale = 2.0f;

    render::StudioPreviewContext context(render::GetRendererState().studio, &entity, params, model);
    const render::StudioPreviewInputs* inputs = render::GetRendererState().studio.PreviewInputs(&entity);

    ASSERT_NE(inputs, nullptr);
    EXPECT_FLOAT_EQ(inputs->camera.origin[0], 25.0f);
    EXPECT_NEAR(inputs->view_right[0], 1.0f, 0.00001f);
    EXPECT_NEAR(inputs->view_right[1], 0.0f, 0.00001f);
    EXPECT_TRUE(inputs->authoring);
    EXPECT_FALSE(inputs->materials_enabled);
    EXPECT_FLOAT_EQ(inputs->scale, 2.0f);
}

TEST(StudioPreviewContext, CameraSpaceRotatesSceneLightAndMirrorsTheBasis)
{
    goldsrc::cl_entity_t entity{};
    PreviewDrawParams params{};
    params.camera.origin[0] = 25.0f;
    params.camera.angles[1] = 90.0f;
    params.light.direction[0] = 1.0f;
    PreviewModel model{};
    model.camera_space = true;
    model.mirror = true;

    render::StudioPreviewContext context(render::GetRendererState().studio, &entity, params, model);
    const render::StudioPreviewInputs* inputs = render::GetRendererState().studio.PreviewInputs(&entity);

    ASSERT_NE(inputs, nullptr);
    EXPECT_FLOAT_EQ(inputs->camera.origin[0], 0.0f);
    EXPECT_NEAR(inputs->light.direction[0], 0.0f, 0.00001f);
    EXPECT_NEAR(inputs->light.direction[1], 1.0f, 0.00001f);
    EXPECT_FLOAT_EQ(inputs->view_right[1], 1.0f);
}

TEST(StudioPreviewContext, ContextIsEntityScopedAndRestoresItsPredecessor)
{
    goldsrc::cl_entity_t first{};
    goldsrc::cl_entity_t second{};
    PreviewDrawParams params{};
    PreviewModel model{};

    EXPECT_EQ(render::GetRendererState().studio.PreviewInputs(&first), nullptr);

    {
        render::StudioPreviewContext outer(render::GetRendererState().studio, &first, params, model);
        EXPECT_EQ(render::GetRendererState().studio.PreviewInputs(&second), nullptr);

        {
            render::StudioPreviewContext inner(render::GetRendererState().studio, &second, params, model);
            EXPECT_NE(render::GetRendererState().studio.PreviewInputs(&second), nullptr);
            EXPECT_EQ(render::GetRendererState().studio.PreviewInputs(&first), nullptr);
        }

        EXPECT_NE(render::GetRendererState().studio.PreviewInputs(&first), nullptr);
    }

    EXPECT_EQ(render::GetRendererState().studio.PreviewInputs(&first), nullptr);
}

TEST(StudioPreviewContext, ReusedPreviewEntityKeepsIndependentModelDrawIdentities)
{
    goldsrc::cl_entity_t entity{};
    PreviewDrawParams params{};
    PreviewModel first{};
    PreviewModel second{};
    render::StudioPreviewContext outer(render::GetRendererState().studio, &entity, params, first);
    const uintptr_t first_identity = render::GetRendererState().studio.PreviewInputs(&entity)->draw_identity;
    {
        render::StudioPreviewContext inner(render::GetRendererState().studio, &entity, params, second);
        EXPECT_NE(render::GetRendererState().studio.PreviewInputs(&entity)->draw_identity, first_identity);
    }
    EXPECT_EQ(render::GetRendererState().studio.PreviewInputs(&entity)->draw_identity, first_identity);
}
