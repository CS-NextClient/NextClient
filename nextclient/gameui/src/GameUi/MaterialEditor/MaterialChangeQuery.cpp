#include "GameUi/MaterialEditor/MaterialChangeQuery.h"

#include <vgui_controls/Button.h>
#include <tier1/strtools.h>

namespace material_editor
{

CMaterialChangeQuery::CMaterialChangeQuery(vgui2::Panel* owner, TransitionToken_t token) :
    BaseClass("#GameUI_MatEdit_UnsavedTitle", "#GameUI_MatEdit_UnsavedText", owner),
    token_(token)
{
    SetMinimumSize(440, 160);
    SetOKButtonText("#GameUI_MatEdit_Discard");
    SetOKCommand(CreateResponse(TransitionChoice::Discard));
    SetCancelCommand(CreateResponse(TransitionChoice::Cancel));
    AddActionSignalTarget(owner);
    save_ = new vgui2::Button(this, "SaveChanges", "#GameUI_MatEdit_Save", this, "save_changes");
}

void CMaterialChangeQuery::PerformLayout()
{
    BaseClass::PerformLayout();
    int width = (GetWide() - 48) / 3;
    int y = m_pOkButton->GetYPos();
    save_->SetBounds(12, y, width, 24);
    m_pOkButton->SetBounds(24 + width, y, width, 24);
    m_pCancelButton->SetBounds(36 + width * 2, y, width, 24);
}

void CMaterialChangeQuery::OnCommand(const char* command)
{
    if (V_stricmp(command, "save_changes") == 0)
    {
        PostActionSignal(CreateResponse(TransitionChoice::Save));
        Close();
        return;
    }

    BaseClass::OnCommand(command);
}

void CMaterialChangeQuery::OnCloseFrameButtonPressed()
{
    OnCommand("Cancel");
}

KeyValues* CMaterialChangeQuery::CreateResponse(TransitionChoice choice) const
{
    KeyValues* response = new KeyValues("MaterialTransitionResolved");
    response->SetUint64("token", token_);
    response->SetInt("choice", static_cast<int>(choice));

    return response;
}

} // namespace material_editor
