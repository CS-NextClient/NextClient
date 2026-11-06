#pragma once

#include <string>

#include <vgui/MouseCode.h>
#include <vgui_controls/Panel.h>

#include "GameUi/MaterialEditor/MaterialPreviewState.h"
#include "GameUi/MaterialEditor/MaterialPreviewPointer.h"

class ScenePreviewInterface;
namespace render
{
    class MaterialEditInterface;
} // namespace render

namespace vgui2
{
    class Label;
} // namespace vgui2

namespace material_editor
{
class CMaterialPreviewPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CMaterialPreviewPanel, vgui2::Panel);

private:
    MaterialPreviewState& state_;
    ScenePreviewInterface& preview_;
    render::MaterialEditInterface& editor_;
    vgui2::Label* status_{};
    std::string load_error_;
    bool comparing_{};
    bool dragging_{};
    MaterialPreviewPointer pointer_;
    std::string click_model_;
    int click_x_{};
    int click_y_{};
    vgui2::MouseCode drag_button_{};
    int last_x_{};
    int last_y_{};

public:
    CMaterialPreviewPanel(vgui2::Panel* parent, MaterialPreviewState& state, ScenePreviewInterface& preview, render::MaterialEditInterface& editor);

    void Paint() override;
    void PerformLayout() override;

    void OnMousePressed(vgui2::MouseCode code) override;
    void OnMouseReleased(vgui2::MouseCode code) override;
    void OnCursorMoved(int x, int y) override;
    void OnMouseWheeled(int delta) override;
    void OnKillFocus() override;
    void OnMouseCaptureLost() override;
    void SetComparison(bool comparing);
    void SetLoadError(const char* message);

    void CancelInteraction();

private:

    int get_scene_height();
    bool IsCursorOverScene();
};

} // namespace material_editor
