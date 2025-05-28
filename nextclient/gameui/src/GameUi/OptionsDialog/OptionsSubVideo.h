//========= Copyright ?1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTIONS_SUB_VIDEO_H
#define OPTIONS_SUB_VIDEO_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/PropertyPage.h>
#include <nitro_utils/config/FileConfigProvider.h>
#include "VideoAdvancedDialog.h"
#include "igameuifuncs.h"

namespace vgui2
{
    class CheckButton;
    class ComboBox;
}

class CCvarSlider;
class CCvarToggleCheckButton;

//-----------------------------------------------------------------------------
// Purpose: Video Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubVideo : public vgui2::PropertyPage
{
    DECLARE_CLASS_SIMPLE( COptionsSubVideo, vgui2::PropertyPage );

public:
    COptionsSubVideo(vgui2::Panel *parent);
    ~COptionsSubVideo();

    virtual void OnResetData();
    virtual void OnApplyChanges();
    virtual void OnCommand(const char *command);

private:
    typedef vgui2::PropertyPage BaseClass;

    struct CVidSettings
    {
        int			w, h;
        int			bpp;
        int			windowed;
        int			hdmodels;
        int			addons_folder;
        int			vid_level;
        int			disable_multitexture;
        char		renderer[ 128 ];
    };

    std::shared_ptr<nitro_utils::FileConfigProvider> m_pUserConfig;

    CVidSettings		m_OrigSettings;
    CVidSettings		m_CurrentSettings;

    void		GetVidSettings();
    void		RevertVidSettings();
    void		ApplyVidSettings(bool bForceRefresh);

    void        SetCurrentResolutionComboItem();

    MESSAGE_FUNC( OnDataChanged, "ControlModified" );
    MESSAGE_FUNC_PARAMS( OnButtonChecked, "CheckButtonChecked", panel);
    MESSAGE_FUNC_PTR_CHARPTR( OnTextChanged, "TextChanged", panel, text );
    void		PrepareResolutionList( void );

    vgui2::ComboBox *m_pMode;
    vgui2::ComboBox *m_pRenderer;
    vgui2::ComboBox *m_pColorDepth;
    vgui2::CheckButton *m_pWindowed;
    vgui2::ComboBox *m_pAspectRatio;
    CCvarToggleCheckButton *m_pDetailTextures;
    CCvarToggleCheckButton *m_pVsync;
    vgui2::CheckButton *m_pHDModels;
    vgui2::CheckButton *m_pAddonsFolder;
    vgui2::CheckButton *m_pLowVideoDetail;
    vgui2::CheckButton *m_pDisableMultitexture;

    CCvarSlider		*m_pBrightnessSlider;
    CCvarSlider		*m_pGammaSlider;

    char            m_pszRenderName[32];
    char            m_pszAspectName[2][32];

    int             m_iStartResolution;
    bool			m_bStartWidescreen;

    vgui2::DHANDLE<CVideoAdvancedDialog> m_hVideoAdvancedDialog;
};



#endif // OPTIONS_SUB_VIDEO_H