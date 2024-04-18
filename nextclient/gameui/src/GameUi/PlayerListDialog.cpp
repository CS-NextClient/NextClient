#include <vgui/ILocalize.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ListPanel.h>
#include "GameUi.h"
#include <tier2/tier2.h>
#include "PlayerListDialog.h"

CPlayerListDialog::CPlayerListDialog( vgui2::Panel* parent )
    : BaseClass( parent, "PlayerListDialog", true )
{
    SetMinimumSize( 300, 240 );
    SetSize( 320, 240 );

    if( GameClientExports() )
    {
        wchar_t hostname[ 128 ];
        g_pVGuiLocalize->ConvertANSIToUnicode( GameClientExports()->GetServerHostName(), hostname, sizeof( hostname ) );

        wchar_t title[ 256 ];
        g_pVGuiLocalize->ConstructString(
            title, sizeof( title ),
            g_pVGuiLocalize->Find( "#GameUI_PlayerListDialogTitle" ), 1,
            hostname
        );

        SetTitle( title, true );
    }
    else
    {
        SetTitle( "#GameUI_CurrentPlayers", true );
    }

    m_pMuteButton = new vgui2::Button( this, "MuteButton", "" );
    m_pPlayerList = new vgui2::ListPanel( this, "PlayerList" );

    m_pPlayerList->AddColumnHeader( 0, "Name", "#GameUI_PlayerName", 128 );
    m_pPlayerList->AddColumnHeader( 1, "Properties", "#GameUI_Properties", 80 );
    m_pPlayerList->SetEmptyListText( "#GameUI_NoOtherPlayersInGame" );

    LoadControlSettings( "Resource/PlayerListDialog.res" );
}

void CPlayerListDialog::RefreshPlayerProperties()
{
    for( int i = 0; i < m_pPlayerList->GetItemCount(); ++i )
    {
        auto pItem = m_pPlayerList->GetItem( i );

        if( pItem )
        {
            const auto index = pItem->GetInt( "index" );

            auto pszName = engine->PlayerInfo_ValueForKey( index, "name" );

            if( pszName )
            {
                pItem->SetString( "name", pszName );

                bool muted = GameClientExports() && GameClientExports()->IsPlayerGameVoiceMuted( index );

                auto pszBot = engine->PlayerInfo_ValueForKey( index, "*bot" );

                if( pszBot && !stricmp( pszBot, "1" ) )
                {
                    pItem->SetString( "properties", "CPU Player" );
                }
                else
                {
                    if( muted )
                    {
                        pItem->SetString( "properties", "Muted" );
                    }
                    else
                    {
                        pItem->SetString( "properties", "" );
                    }
                }
            }
            else
            {
                pItem->SetString( "properties", "Disconnected" );
            }
        }
    }

    m_pPlayerList->RereadAllItems();
}

void CPlayerListDialog::OnItemSelected()
{
    RefreshPlayerProperties();

    if( m_pPlayerList->GetSelectedItemsCount() <= 0 )
    {
        m_pMuteButton->SetEnabled( false );
        m_pMuteButton->SetText( "#GameUI_MuteIngameVoice" );
        return;
    }

    auto pItem = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( 0 ) );

    bool bMutable = false;

    if( pItem )
    {
        auto pszBot = engine->PlayerInfo_ValueForKey( pItem->GetInt( "index", 0 ), "*bot" );

        if( !pszBot || stricmp( "1", pszBot ) )
            bMutable = true;
    }

    bool bMuted = false;

    if( engine->PlayerInfo_ValueForKey( pItem->GetInt( "index", 0 ), "name" ) &&
        bMutable &&
        GameClientExports() )
    {
        if( GameClientExports()->IsPlayerGameVoiceMuted( pItem->GetInt( "index", 0 ) ) )
        {
            m_pMuteButton->SetText( "#GameUI_UnmuteIngameVoice" );
            bMuted = true;
        }
    }
    else
    {
        bMutable = false;
    }

    if( !bMuted )
        m_pMuteButton->SetText( "#GameUI_MuteIngameVoice" );

    m_pMuteButton->SetEnabled( GameClientExports() && bMutable );
}

void CPlayerListDialog::Activate()
{
    BaseClass::Activate();

    m_pPlayerList->DeleteAllItems();

    const auto maxClients = engine->GetMaxClients();

    char szPlayerIndex[ 32 ];

    for( int i = 1; i <= maxClients; ++i )
    {
        snprintf( szPlayerIndex, ARRAYSIZE( szPlayerIndex ), "%d", i );

        //TODO: if the name is invalid this leaks - Solokiller
        auto pItem = new KeyValues( szPlayerIndex );

        auto pszName = engine->PlayerInfo_ValueForKey( i, "name" );

        if( pszName && *pszName )
        {
            pItem->SetString( "Name", pszName );
            pItem->SetInt( "index", i );

            m_pPlayerList->AddItem( pItem, 0, false, false );
        }
    }

    RefreshPlayerProperties();

    m_pPlayerList->SetSingleSelectedItem( m_pPlayerList->GetItemIDFromRow( 0 ) );

    OnItemSelected();
}

void CPlayerListDialog::ToggleMuteStateOfSelectedUser()
{
    if( GameClientExports() )
    {
        auto pItem = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( 0 ) );

        if( pItem )
        {
            const auto index = pItem->GetInt( "index", 0 );

            if( GameClientExports()->IsPlayerGameVoiceMuted( index ) )
            {
                GameClientExports()->UnmutePlayerGameVoice( index );
            }
            else
            {
                GameClientExports()->MutePlayerGameVoice( index );
            }

            RefreshPlayerProperties();

            OnItemSelected();
        }
    }
}

void CPlayerListDialog::OnCommand( const char* command )
{
    if( !stricmp( command, "Mute" ) )
        ToggleMuteStateOfSelectedUser();
    else
        BaseClass::OnCommand( command );
}