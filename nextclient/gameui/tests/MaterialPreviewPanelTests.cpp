#include <gtest/gtest.h>

#include <next_engine_mini/MaterialEditInterface.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISystem.h>

#include "GameUi/MaterialEditor/MaterialPreviewPanel.h"

using material_editor::CMaterialPreviewPanel;
using material_editor::MaterialPreviewState;

namespace
{
    class Input : public vgui2::IInputInternal
    {
    public:
        vgui2::VPANEL capture{};
        int x = 32;
        int y = 32;
        void SetMouseCapture(vgui2::VPANEL panel) override
        {
            capture = panel;
        }
        vgui2::VPANEL GetMouseCapture() override
        {
            return capture;
        }
        void GetCursorPos(int& out_x, int& out_y) override
        {
            out_x = x;
            out_y = y;
        }
        void SetCursorPos(int in_x, int in_y) override
        {
            x = in_x;
            y = in_y;
        }
        bool IsKeyDown(vgui2::KeyCode) override
        {
            return false;
        }
    };

    class Editor : public render::MaterialEditInterface
    {
    public:
        int GetProgramCount() override
        {
            return {};
        }
        const char* get_program_name(int index) override
        {
            return {};
        }
        int GetParamCount(const char* program) override
        {
            return {};
        }
        bool GetParam(const char* program, int index, render::MaterialEditParam* out) override
        {
            return {};
        }
        int GetGroupCount(const char* program) override
        {
            return {};
        }
        const char* GetGroupLabel(const char* program, int group) override
        {
            return {};
        }
        int GetGroupParamCount(const char* program, int group) override
        {
            return {};
        }
        int GetGroupParam(const char* program, int group, int index) override
        {
            return {};
        }
        int get_model_count() override
        {
            return {};
        }
        const char* GetModelName(int index) override
        {
            return {};
        }
        int get_texture_count(const char* model) override
        {
            return {};
        }
        const char* get_texture_name(const char* model, int index) override
        {
            return {};
        }
        render::MaterialEditSource GetMaterialSource(const char* model, const char* texture_name, int entity_index) override
        {
            return {};
        }
        int GetSkinFamilyCount(const char* model) override
        {
            return {};
        }
        int GetBodyCount(const char* model) override
        {
            return {};
        }
        bool GetBodyName(const char* model, int body, char* out, int out_len) override
        {
            return {};
        }
        bool Begin(const char* model, const char* texture_name, int entity_index) override
        {
            return {};
        }
        void End() override {}
        bool is_editing() override
        {
            return {};
        }
        int get_edited_entity_index() override
        {
            return {};
        }
        const char* get_edited_model() override
        {
            return {};
        }
        const char* get_edited_texture() override
        {
            return {};
        }
        const char* get_edited_program() override
        {
            return {};
        }
        bool SetEditedProgram(const char* program) override
        {
            return {};
        }
        bool get_scalar(const char* name, float* out) override
        {
            return {};
        }
        bool get_vec4(const char* name, float* out) override
        {
            return {};
        }
        const char* get_texture(const char* name) override
        {
            return {};
        }
        bool SetScalar(const char* name, float value) override
        {
            return {};
        }
        bool SetVec4(const char* name, const float* value) override
        {
            return {};
        }
        bool SetTexture(const char* name, const char* path) override
        {
            return {};
        }
        bool ClearParam(const char* name) override
        {
            return {};
        }
        const char* Validate() override
        {
            return {};
        }
        bool Save(const char* model, char* error_out, int error_out_len) override
        {
            return {};
        }
        const char* GetPresetStub() override
        {
            return {};
        }
        void Reload() override {}
        bool BeginGroup(const char* model, const char* const* textures, int count, int entity_index) override
        {
            return {};
        }
        int get_selection_count() override
        {
            return {};
        }
        const char* get_selected_texture(int index) override
        {
            return {};
        }
        bool has_mixed_programs() override
        {
            return {};
        }
        bool is_param_mixed(const char* name) override
        {
            return {};
        }
        int GetChoiceCount(const char* program, const char* param) override
        {
            return {};
        }
        const char* GetChoiceLabel(const char* program, const char* param, int index) override
        {
            return {};
        }
        bool BeginParamPreview(const char* name) override
        {
            return {};
        }
        void EndParamPreview(bool accept) override {}
    };

    class Preview : public ScenePreviewInterface
    {
    public:
        bool is_authoring_available() const override
        {
            return true;
        }
        PreviewSceneHandle LoadScene(const char*, const char* const*, int) override
        {
            return 0;
        }
        void FreeScene(PreviewSceneHandle) override {}
        PreviewModelStatus PrepareModel(const char*, PreviewModelInfo*) override
        {
            return PreviewModelStatus::Ready;
        }
        bool GetSequenceInfo(const char*, int, PreviewSequenceInfo*) override
        {
            return false;
        }
        PreviewDrawStatus Draw(const PreviewDrawParams&, const PreviewRect&, const PreviewRect&, PreviewHit*) override
        {
            return PreviewDrawStatus::Ready;
        }
        void CloseAuthoring() override {}
    };

    Input g_Input;
    vgui2::IPanel g_Panels;
    vgui2::ISystem g_System;

    void ExpectSameCamera(const PreviewCamera& expected, const PreviewCamera& actual)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            EXPECT_FLOAT_EQ(expected.origin[axis], actual.origin[axis]);
            EXPECT_FLOAT_EQ(expected.angles[axis], actual.angles[axis]);
        }
        EXPECT_FLOAT_EQ(expected.fov, actual.fov);
    }

    class MaterialPreviewPanelTest : public testing::Test
    {
    protected:
        MaterialPreviewState state;
        Preview preview;
        Editor editor;
        vgui2::Panel parent;
        CMaterialPreviewPanel panel{&parent, state, preview, editor};

        void SetUp() override
        {
            g_Input = Input{};
        }
    };
} // namespace

vgui2::IInputInternal* vgui2::input()
{
    return &g_Input;
}
vgui2::IPanel* vgui2::ipanel()
{
    return &g_Panels;
}
vgui2::ISystem* vgui2::system()
{
    return &g_System;
}

TEST_F(MaterialPreviewPanelTest, CaptureLossAfterOutsideSecondPressStopsCameraMovement)
{
    panel.OnMousePressed(vgui2::MOUSE_LEFT);
    panel.OnCursorMoved(50, 40);
    PreviewCamera before_loss = state.BuildCamera();
    g_Input.x = 150;
    panel.OnMousePressed(vgui2::MOUSE_RIGHT);
    g_Input.capture = 0;
    panel.OnMouseCaptureLost();
    panel.OnCursorMoved(70, 60);
    ExpectSameCamera(before_loss, state.BuildCamera());
}

TEST_F(MaterialPreviewPanelTest, CancelPreservesForeignCaptureAndStopsCameraMovement)
{
    panel.OnMousePressed(vgui2::MOUSE_LEFT);
    vgui2::Panel other;
    g_Input.capture = other.GetVPanel();
    panel.CancelInteraction();
    PreviewCamera canceled = state.BuildCamera();
    panel.OnCursorMoved(70, 60);
    ExpectSameCamera(canceled, state.BuildCamera());
    EXPECT_EQ(g_Input.capture, other.GetVPanel());
}

TEST_F(MaterialPreviewPanelTest, StaleCaptureLossAfterReacquisitionKeepsTheNewDrag)
{
    panel.OnMousePressed(vgui2::MOUSE_LEFT);
    g_Input.capture = 0;
    panel.OnMousePressed(vgui2::MOUSE_RIGHT);
    panel.OnMouseCaptureLost();
    PreviewCamera before_move = state.BuildCamera();
    panel.OnCursorMoved(70, 60);
    EXPECT_NE(state.BuildCamera().origin[0], before_move.origin[0]);
    EXPECT_EQ(g_Input.capture, panel.GetVPanel());
    panel.OnMouseReleased(vgui2::MOUSE_RIGHT);
    PreviewCamera released = state.BuildCamera();
    panel.OnMouseCaptureLost();
    panel.OnCursorMoved(90, 70);
    ExpectSameCamera(released, state.BuildCamera());
    EXPECT_EQ(g_Input.capture, 0u);
}

TEST_F(MaterialPreviewPanelTest, CaptureLossToForeignOwnerStopsPanAndPreservesCapture)
{
    panel.OnMousePressed(vgui2::MOUSE_MIDDLE);
    vgui2::Panel other;
    g_Input.capture = other.GetVPanel();
    panel.OnMouseCaptureLost();
    PreviewCamera before_move = state.BuildCamera();
    panel.OnCursorMoved(70, 60);
    ExpectSameCamera(before_move, state.BuildCamera());
    EXPECT_EQ(g_Input.capture, other.GetVPanel());
}

TEST_F(MaterialPreviewPanelTest, FocusLossCancelsOwnedCapture)
{
    panel.OnMousePressed(vgui2::MOUSE_LEFT);
    panel.OnKillFocus();
    PreviewCamera before_move = state.BuildCamera();
    panel.OnCursorMoved(70, 60);
    ExpectSameCamera(before_move, state.BuildCamera());
    EXPECT_EQ(g_Input.capture, 0u);
}
