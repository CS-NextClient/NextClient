#include "GameUi/MaterialEditor/MaterialPreviewPanel.h"

#include <next_engine_mini/MaterialEditInterface.h>
#include <next_engine_mini/ScenePreviewInterface.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Label.h>
#include <tier1/KeyValues.h>

namespace material_editor
{

using namespace vgui2;

namespace
{
    constexpr float kMouseDegreesPerPixel = 0.4f;
} // namespace

CMaterialPreviewPanel::CMaterialPreviewPanel(
    Panel* parent,
    MaterialPreviewState& state,
    ScenePreviewInterface& preview,
    render::MaterialEditInterface& editor
) :
    BaseClass(parent, "MaterialPreview"),
    state_(state),
    preview_(preview),
    editor_(editor)
{
    SetPaintBackgroundEnabled(false);
    SetMouseInputEnabled(true);
    SetKeyBoardInputEnabled(true);
    status_ = new Label(this, "PreviewStatus", "#GameUI_MatEdit_SelectModel");
    status_->SetWrap(true);

}

int CMaterialPreviewPanel::get_scene_height()
{
    return GetTall();
}

void CMaterialPreviewPanel::Paint()
{
    BaseClass::Paint();
    PreviewModel model = state_.BuildModel();
    PreviewDrawParams params{};
    params.camera = state_.BuildCamera();
    params.light = state_.BuildLight();
    params.time = system()->GetCurrentTime();
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.materials_enabled = !comparing_;
    params.clear = true;
    params.interactive = true;
    params.sun_gizmo = state_.is_sun_gizmo_shown();
    params.occluder = state_.is_occluder_shown();
    input()->GetCursorPos(params.cursor_x, params.cursor_y);
    ScreenToLocal(params.cursor_x, params.cursor_y);

    if (dragging_ || input()->GetAppModalSurface() != GetParent()->GetVPanel())
    {
        params.cursor_x = params.cursor_y = -1;
    }

    bool select = !click_model_.empty() && click_model_ == state_.get_model();

    if (select)
    {
        params.cursor_x = click_x_;
        params.cursor_y = click_y_;
    }

    params.interactive =
        params.cursor_x >= 0 && params.cursor_x < GetWide() && params.cursor_y >= 0 && params.cursor_y < get_scene_height();
    params.clear_color[0] = 0.16f;
    params.clear_color[1] = 0.16f;
    params.clear_color[2] = 0.16f;

    if (*model.path != '\0')
    {
        params.models = &model;
        params.model_count = 1;
    }

    PreviewRect rect{0, 0, GetWide(), get_scene_height()};
    LocalToScreen(rect.x, rect.y);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    ipanel()->GetClipRect(GetVPanel(), x0, y0, x1, y1);
    PreviewRect clip{x0, y0, x1 - x0, y1 - y0};
    PreviewHit hit{};
    PreviewDrawStatus result = preview_.Draw(params, rect, clip, &hit);

    if (select && result == PreviewDrawStatus::Ready && hit.model_index == 0 && hit.texture_index >= 0)
    {
        const char* texture = editor_.get_texture_name(state_.get_model(), hit.texture_index);

        if (texture != nullptr && *texture != '\0')
        {
            KeyValues* picked = new KeyValues("TexturePicked", "model", state_.get_model());
            picked->SetString("texture", texture);
            PostActionSignal(picked);
        }
    }

    click_model_.clear();

    if (result == PreviewDrawStatus::RendererUnavailable)
    {
        status_->SetText("#GameUI_MatEdit_PreviewUnavailable");
    }
    else if (result == PreviewDrawStatus::ModelDrawFailed)
    {
        status_->SetText("#GameUI_MatEdit_PreviewFailed");
    }
    else
    {
        status_->SetText(!load_error_.empty() ? load_error_.c_str() : params.model_count == 0 ? "#GameUI_MatEdit_SelectModel" : "");
    }
}

void CMaterialPreviewPanel::PerformLayout()
{
    BaseClass::PerformLayout();
    status_->SetBounds(12, 8, GetWide() - 24, 64);
}

void CMaterialPreviewPanel::SetLoadError(const char* message)
{
    load_error_ = message;
}

void CMaterialPreviewPanel::SetComparison(bool comparing)
{
    comparing_ = comparing;
}

bool CMaterialPreviewPanel::IsCursorOverScene()
{
    int x = 0;
    int y = 0;
    input()->GetCursorPos(x, y);
    ScreenToLocal(x, y);
    return x >= 0 && x < GetWide() && y >= 0 && y < get_scene_height();
}

void CMaterialPreviewPanel::OnMousePressed(MouseCode code)
{
    if (!IsCursorOverScene() || (code != MOUSE_LEFT && code != MOUSE_RIGHT && code != MOUSE_MIDDLE))
    {
        return;
    }

    input()->GetCursorPos(last_x_, last_y_);
    ScreenToLocal(last_x_, last_y_);
    pointer_.Begin(last_x_, last_y_);
    click_model_.clear();
    dragging_ = true;
    drag_button_ = code;
    RequestFocus();
    input()->SetMouseCapture(GetVPanel());
}

void CMaterialPreviewPanel::OnMouseReleased(MouseCode code)
{
    if (dragging_ && code == drag_button_)
    {
        int x = 0;
        int y = 0;
        input()->GetCursorPos(x, y);
        ScreenToLocal(x, y);
        bool click = pointer_.End(x, y);
        CancelInteraction();

        if (code == MOUSE_LEFT && click && IsCursorOverScene())
        {
            click_model_ = state_.get_model();
            click_x_ = x;
            click_y_ = y;
        }
    }
}

void CMaterialPreviewPanel::OnCursorMoved(int x, int y)
{
    if (!dragging_)
    {
        return;
    }

    if (drag_button_ == MOUSE_LEFT && !pointer_.Move(x, y))
    {
        return;
    }

    float horizontal = static_cast<float>(x - last_x_);
    float vertical = static_cast<float>(y - last_y_);
    last_x_ = x;
    last_y_ = y;

    if (drag_button_ == MOUSE_LEFT)
    {
        state_.Orbit(-horizontal * kMouseDegreesPerPixel, vertical * kMouseDegreesPerPixel);
    }
    else
    {
        state_.Pan(-horizontal, vertical);
    }
}

void CMaterialPreviewPanel::OnMouseWheeled(int delta)
{
    if (IsCursorOverScene())
    {
        state_.Zoom(static_cast<float>(delta));
    }
}

void CMaterialPreviewPanel::CancelInteraction()
{
    pointer_.Cancel();
    click_model_.clear();

    dragging_ = false;

    if (input()->GetMouseCapture() == GetVPanel())
    {
        input()->SetMouseCapture(NULL);
    }
}

void CMaterialPreviewPanel::OnMouseCaptureLost()
{
    BaseClass::OnMouseCaptureLost();

    // Capture-loss messages can arrive after a newer press reacquires this panel.
    if (input()->GetMouseCapture() != GetVPanel())
    {
        CancelInteraction();
    }
}

void CMaterialPreviewPanel::OnKillFocus()
{
    CancelInteraction();
    BaseClass::OnKillFocus();
}

} // namespace material_editor
