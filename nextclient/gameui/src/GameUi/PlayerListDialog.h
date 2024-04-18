#ifndef GAMEUI_PLAYERLISTDIALOG_H
#define GAMEUI_PLAYERLISTDIALOG_H

#include <vgui_controls/Frame.h>

namespace vgui2
{
    class ListPanel;
    class Button;
}

/**
*	Dialog that lists players and enables the local player to mute them.
*/
class CPlayerListDialog : public vgui2::Frame
{
public:
    DECLARE_CLASS_SIMPLE( CPlayerListDialog, vgui2::Frame );

public:
    CPlayerListDialog( vgui2::Panel* parent );

    void RefreshPlayerProperties();

    MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

    void Activate() override;

    void ToggleMuteStateOfSelectedUser();

    void OnCommand( const char* command ) override;

private:
    vgui2::ListPanel* m_pPlayerList;
    vgui2::Button* m_pMuteButton;

private:
    CPlayerListDialog( const CPlayerListDialog& ) = delete;
    CPlayerListDialog& operator=( const CPlayerListDialog& ) = delete;
};

#endif //GAMEUI_PLAYERLISTDIALOG_H