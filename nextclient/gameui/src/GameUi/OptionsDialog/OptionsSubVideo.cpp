#include "GameUi.h"
//#include "BaseUI.h"
#include "OptionsSubVideo.h"
#include "CvarSlider.h"
#include "CvarToggleCheckButton.h"
#include "igameuifuncs.h"
//#include "modes.h"
#include "ModInfo.h"
#include "KeyToggleCheckButton.h"

#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <KeyValues.h>
#include <vgui/ILocalize.h>
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#pragma warning(disable: 4101)

inline bool IsWideScreen ( int width, int height )
{
    // 16:9 or 16:10 is widescreen :)
    if ( (width * 9) == ( height * 16.0f ) || (width * 5.0) == ( height * 8.0 ))
        return true;

    return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubVideo::COptionsSubVideo(vgui2::Panel *parent) : PropertyPage(parent, NULL)
{
    memset( &m_OrigSettings, 0, sizeof( m_OrigSettings ) );
    memset( &m_CurrentSettings, 0, sizeof( m_CurrentSettings ) );

    m_pBrightnessSlider = new CCvarSlider( this, "Brightness", "#GameUI_Brightness",
                                           0.0f, 2.0f, "brightness" );

    m_pGammaSlider = new CCvarSlider( this, "Gamma", "#GameUI_Gamma",
                                      1.0f, 3.0f, "gamma" );

    GetVidSettings();

    m_pMode = new vgui2::ComboBox(this, "Resolution", 6, false);

    m_pAspectRatio = new vgui2::ComboBox( this, "AspectRatio", 2, false );

    m_pDetailTextures = new CCvarToggleCheckButton( this, "DetailTextures", "#GameUI_DetailTextures", "r_detailtextures" );
    m_pDetailTextures->SetVisible(false);

    m_pVsync = new CCvarToggleCheckButton( this, "VSync", "#GameUI_VSync", "gl_vsync" );
    m_pVsync->SetVisible(true);

    wchar_t *unicodeText = g_pVGuiLocalize->Find("#GameUI_AspectNormal");
    g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, m_pszAspectName[0], 32);
    unicodeText = g_pVGuiLocalize->Find("#GameUI_AspectWide");
    g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, m_pszAspectName[1], 32);

    int iNormalItemID = m_pAspectRatio->AddItem( m_pszAspectName[0], NULL );
    int iWideItemID = m_pAspectRatio->AddItem( m_pszAspectName[1], NULL );

    m_bStartWidescreen = IsWideScreen( m_CurrentSettings.w, m_CurrentSettings.h );
    if ( m_bStartWidescreen )
    {
        m_pAspectRatio->ActivateItem( iWideItemID );
    }
    else
    {
        m_pAspectRatio->ActivateItem( iNormalItemID );
    }

    // disable render selection until new render will be ready
    m_pRenderer = nullptr;

    // load up the renderer display names
//    unicodeText = g_pVGuiLocalize->Find("#GameUI_OpenGL");
//    g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, m_pszRenderName, 32);

//    m_pRenderer = new vgui2::ComboBox( this, "Renderer", 3, false ); // "#GameUI_Renderer"
//    m_pRenderer->AddItem( m_pszRenderName, NULL );
//    m_pRenderer->ActivateItemByRow( 0 );

    m_pColorDepth = new vgui2::ComboBox( this, "ColorDepth", 2, false );
    m_pColorDepth->AddItem("#GameUI_MediumBitDepth", NULL);
    m_pColorDepth->AddItem("#GameUI_HighBitDepth", NULL);
    m_pColorDepth->SetVisible( false ); // default hide

    m_pWindowed = new vgui2::CheckButton( this, "Windowed", "#GameUI_Windowed" );
    m_pWindowed->SetSelected(m_CurrentSettings.windowed != 0);
    m_pWindowed->SetVisible(true);

    m_pHDModels = new vgui2::CheckButton( this, "HDModels", "#GameUI_HDModels" );
    m_pHDModels->SetSelected(m_CurrentSettings.hdmodels != 0);
    m_pHDModels->SetVisible(false);

    m_pAddonsFolder = new vgui2::CheckButton( this, "AddonsFolder", "#GameUI_AddonsFolder" );
    m_pAddonsFolder->SetSelected(m_CurrentSettings.addons_folder != 0);
    m_pAddonsFolder->SetVisible(false);

    m_pLowVideoDetail = new vgui2::CheckButton( this, "LowVideoDetail", "#GameUI_LowVideoDetail" );
    m_pLowVideoDetail->SetSelected(m_CurrentSettings.vid_level == 0);
    m_pLowVideoDetail->SetVisible(true);

    LoadControlSettings("Resource\\OptionsSubVideo.res");
    PrepareResolutionList();

    bool detailTexturesSupported = engine->pfnGetCvarFloat( "r_detailtexturessupported" ) > 0;
    if ( ModInfo().GetDetailedTexture() )
    {
        if ( !detailTexturesSupported )
            m_pDetailTextures->SetEnabled( false );
    }
    else
    {
        m_pDetailTextures->SetVisible( false );
    }
}

void COptionsSubVideo::PrepareResolutionList( void )
{
    vmode_t *plist = NULL;
    int count = 0;
    bool bFoundWidescreen = false;
    int nItemsAdded = 0;

    g_pGameUIFuncs->GetVideoModes( &plist, &count );

    // Get selected resolution in list (not current game resolution)
    vmode_t lastSelectedResolution{};
    char szSelectedResolution[256];
    m_pMode->GetItemText(m_pMode->GetActiveItem(), szSelectedResolution, sizeof(szSelectedResolution));
    if (szSelectedResolution[0] != '\0')
        sscanf(szSelectedResolution, "%i x %i", &lastSelectedResolution.iWidth, &lastSelectedResolution.iHeight );

    // Clean up before filling the info again.
    m_pMode->DeleteAllItems();

    int selectedItemID = -1;
    int nearestItemID = -1; int minWidthDiff = INT_MAX;
    for (int i = 0; i < count; i++, plist++)
    {
        // exclude obscenely small resolutions :)
        if (plist->iWidth < 640 || plist->iHeight < 480)
            continue;

        char sz[ 256 ];
        sprintf( sz, "%i x %i", plist->iWidth, plist->iHeight );

        int itemID = -1;
        if ( IsWideScreen( plist->iWidth, plist->iHeight ) )
        {
            if (m_bStartWidescreen)
            {
                itemID = m_pMode->AddItem( sz, NULL );
                nItemsAdded++;
            }

            bFoundWidescreen = true;
        }
        else
        {
            if (!m_bStartWidescreen)
            {
                itemID = m_pMode->AddItem( sz, NULL);
                nItemsAdded++;
            }
        }

        if (itemID == -1)
            continue;

        if ( plist->iWidth == m_CurrentSettings.w &&
             plist->iHeight == m_CurrentSettings.h )
        {
            selectedItemID = itemID;
        }

        int widthDiff = std::abs(plist->iWidth - lastSelectedResolution.iWidth);
        if (widthDiff < minWidthDiff)
        {
            minWidthDiff = widthDiff;
            nearestItemID = itemID;
        }
    }

    m_pAspectRatio->SetEnabled( bFoundWidescreen );

    if ( selectedItemID != -1 )
    {
        m_pMode->ActivateItem( selectedItemID );
    }
    else if (nearestItemID != -1)
    {
        m_pMode->ActivateItem( nearestItemID );
    }
    else
    {
        m_pMode->ActivateItem( 0 );
    }

    if ( nItemsAdded == 0 && count != 0 )
    {
        m_bStartWidescreen = !m_bStartWidescreen;
        m_pAspectRatio->ActivateItem( ( m_pAspectRatio->GetActiveItem() + 1 ) % 2 );
        PrepareResolutionList();
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubVideo::~COptionsSubVideo()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnResetData()
{
    // reset data
    RevertVidSettings();

    // reset UI elements
    m_pBrightnessSlider->Reset();
    m_pGammaSlider->Reset();
    m_pWindowed->SetSelected(m_CurrentSettings.windowed);
    m_pHDModels->SetSelected(m_CurrentSettings.hdmodels);
    m_pAddonsFolder->SetSelected(m_CurrentSettings.addons_folder);
    m_pLowVideoDetail->SetSelected(m_CurrentSettings.vid_level);
    m_pDetailTextures->Reset();
    m_pVsync->Reset();

    SetCurrentResolutionComboItem();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetCurrentResolutionComboItem()
{
    vmode_t *plist = NULL;
    int count = 0;
    g_pGameUIFuncs->GetVideoModes( &plist, &count );

    int resolution = -1;
    for ( int i = 0; i < count; i++, plist++ )
    {
        if ( plist->iWidth == m_CurrentSettings.w &&
             plist->iHeight == m_CurrentSettings.h )
        {
            resolution = i;
            break;
        }
    }

    if (resolution != -1)
    {
        char sz[256];
        sprintf(sz, "%i x %i", plist->iWidth, plist->iHeight);
        m_pMode->SetText(sz);
    }

    if (m_CurrentSettings.bpp > 16)
    {
        m_pColorDepth->ActivateItemByRow(1);
    }
    else
    {
        m_pColorDepth->ActivateItemByRow(0);
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnApplyChanges()
{
    bool bChanged = m_pBrightnessSlider->HasBeenModified() || m_pGammaSlider->HasBeenModified();

    m_pBrightnessSlider->ApplyChanges();
    m_pGammaSlider->ApplyChanges();
    m_pVsync->ApplyChanges();

    ApplyVidSettings(bChanged);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::GetVidSettings()
{
    // Get original settings
    CVidSettings *p = &m_OrigSettings;

    g_pGameUIFuncs->GetCurrentVideoMode( &p->w, &p->h, &p->bpp );
    g_pGameUIFuncs->GetCurrentRenderer(p->renderer, 128, &p->windowed, &p->hdmodels, &p->addons_folder, &p->vid_level);

    strlwr( p->renderer );

    m_CurrentSettings = m_OrigSettings;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::RevertVidSettings()
{
    m_CurrentSettings = m_OrigSettings;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::ApplyVidSettings(bool bForceRefresh)
{
    // Retrieve text from active controls and parse out strings
    if ( m_pMode )
    {
        char sz[256], colorDepth[256];
        m_pMode->GetText(sz, 256);
        m_pColorDepth->GetText(colorDepth, sizeof(colorDepth));

        int w, h;
        sscanf( sz, "%i x %i", &w, &h );
        m_CurrentSettings.w = w;
        m_CurrentSettings.h = h;
        if (strstr(colorDepth, "32"))
        {
            m_CurrentSettings.bpp = 32;
        }
        else
        {
            m_CurrentSettings.bpp = 16;
        }
    }

    if ( m_pRenderer )
    {
        char sz[ 256 ];
        m_pRenderer->GetText(sz, sizeof(sz));
        strcpy( m_CurrentSettings.renderer, "gl" );
    }

    if ( m_pWindowed )
    {
        bool checked = m_pWindowed->IsSelected();
        m_CurrentSettings.windowed = checked ? 1 : 0;
    }

    if ( m_pHDModels )
    {
        bool checked = m_pHDModels->IsSelected();
        m_CurrentSettings.hdmodels = checked ? 1 : 0;
    }

    if ( m_pAddonsFolder )
    {
        bool checked = m_pAddonsFolder->IsSelected();
        m_CurrentSettings.addons_folder = checked ? 1 : 0;
    }

    if ( m_pLowVideoDetail )
    {
        bool checked = m_pLowVideoDetail->IsSelected();
        m_CurrentSettings.vid_level = checked ? 1 : 0;
    }

    if ( memcmp( &m_OrigSettings, &m_CurrentSettings, sizeof( CVidSettings ) ) == 0 && !bForceRefresh)
    {
        return;
    }

    CVidSettings *p = &m_CurrentSettings;

    char szCmd[ 256 ];
    // Set mode
    sprintf( szCmd, "_setvideomode %i %i %i\n", p->w, p->h, p->bpp );
    engine->pfnClientCmd( szCmd );

    // Set renderer
    sprintf( szCmd, "_setrenderer %s %s\n", p->renderer, p->windowed ? "windowed" : "fullscreen" );
    engine->pfnClientCmd(szCmd);
    sprintf( szCmd, "_sethdmodels %d\n", p->hdmodels );
    engine->pfnClientCmd(szCmd);
    sprintf( szCmd, "_setaddons_folder %d\n", p->addons_folder );
    engine->pfnClientCmd(szCmd);
    sprintf( szCmd, "_set_vid_level %d\n", p->vid_level );
    engine->pfnClientCmd(szCmd);

    // Force restart of entire engine
    engine->pfnClientCmd("fmod stop\n");
    engine->pfnClientCmd("_restart\n");
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnButtonChecked(KeyValues *data)
{
    int state = data->GetInt("state");
    Panel* pPanel = (Panel*) data->GetPtr("panel", NULL);

    if (pPanel == m_pWindowed)
    {
        if (state != m_CurrentSettings.windowed)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pHDModels)
    {
        if (state != m_CurrentSettings.hdmodels)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pAddonsFolder)
    {
        if (state != m_CurrentSettings.addons_folder)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pLowVideoDetail)
    {
        if (state != m_CurrentSettings.vid_level)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pDetailTextures)
    {
        OnDataChanged();
    }

    if (pPanel == m_pVsync)
    {
        OnDataChanged();
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnTextChanged(Panel *pPanel, const char *pszText)
{
    if (pPanel == m_pMode)
    {
        char sz[ 256 ];
        sprintf(sz, "%i x %i", m_CurrentSettings.w, m_CurrentSettings.h);

        if (strcmp(pszText, sz))
        {
            OnDataChanged();
        }
    }
    else if (pPanel == m_pRenderer)
    {
        OnDataChanged();
    }
    else if (pPanel == m_pAspectRatio )
    {
        if ( strcmp(pszText, m_pszAspectName[m_bStartWidescreen] ) )
        {
            m_bStartWidescreen = !m_bStartWidescreen;
            PrepareResolutionList();
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnDataChanged()
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsSubVideo::OnCommand(const char *command)
{
    if (!stricmp(command, "Advanced"))
    {
        if (!m_hVideoAdvancedDialog.Get())
            m_hVideoAdvancedDialog = new CVideoAdvancedDialog(this);

        m_hVideoAdvancedDialog->Activate();
    }

    BaseClass::OnCommand(command);
}
