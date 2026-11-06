#pragma once

#include <vgui_controls/QueryBox.h>

#include "GameUi/MaterialEditor/MaterialTransitionController.h"

namespace material_editor
{
class CMaterialChangeQuery : public vgui2::QueryBox
{
    DECLARE_CLASS_SIMPLE(CMaterialChangeQuery, vgui2::QueryBox);

private:
    vgui2::Button* save_{};
    TransitionToken_t token_{};

public:
    CMaterialChangeQuery(vgui2::Panel* owner, TransitionToken_t token);
    void PerformLayout() override;
    void OnCommand(const char* command) override;
    void OnCloseFrameButtonPressed() override;

private:
    KeyValues* CreateResponse(TransitionChoice choice) const;
};

} // namespace material_editor
