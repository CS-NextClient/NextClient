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
#include "IGameUIFuncs.h"

namespace vgui2
{
    class CheckButton;
    class ComboBox;
}

namespace render
{
    class ShaderRendererInterface;
} // namespace render
class CCvarSlider;
class CCvarToggleCheckButton;

//-----------------------------------------------------------------------------
// Purpose: Video Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubVideo : public vgui2::PropertyPage
{
    DECLARE_CLASS_SIMPLE( COptionsSubVideo, vgui2::PropertyPage );

public:
    COptionsSubVideo(vgui2::Panel* parent, std::shared_ptr<nitro_utils::FileConfigProvider> config, render::ShaderRendererInterface* renderer);
    ~COptionsSubVideo();

    virtual void OnResetData();
    virtual void OnApplyChanges();
    virtual void OnCommand(const char *command);
    void OnThink() override;

private:
    struct CVidSettings
    {
        int			w, h;
        int			bpp;
        int			windowed;
        int			hdmodels;
        int			addons_folder;
        int			vid_level;
        int			disable_multitexture;
        int			stretch_aspect;
        int msaa_samples;
        char		renderer[ 128 ];
    };

    std::shared_ptr<nitro_utils::FileConfigProvider> m_pUserConfig;
    render::ShaderRendererInterface* m_Renderer;

    CVidSettings		m_OrigSettings;
    CVidSettings		m_CurrentSettings;

    void		GetVidSettings();
    void		RevertVidSettings();
    void		ApplyVidSettings(bool bForceRefresh);

    void        SetCurrentResolutionComboItem();
    void UpdateShaderRenderingAvailability();
    void UpdateMsaaAvailability();
    void ResetMsaaSelection();

    MESSAGE_FUNC( OnDataChanged, "ControlModified" );
    MESSAGE_FUNC_PARAMS( OnButtonChecked, "CheckButtonChecked", panel);
    MESSAGE_FUNC_PTR_CHARPTR( OnTextChanged, "TextChanged", panel, text );
    void		PrepareResolutionList( void );

    vgui2::ComboBox *m_pMode;
    vgui2::ComboBox *m_pRenderer;
    vgui2::ComboBox *m_pColorDepth;
    vgui2::CheckButton *m_pWindowed;
    vgui2::ComboBox *m_pAspectRatio;
    vgui2::ComboBox* m_pMsaa{};
    CCvarToggleCheckButton *m_pDetailTextures;
    CCvarToggleCheckButton *m_pVsync;
    CCvarToggleCheckButton* m_pShaderRenderer{};
    vgui2::CheckButton *m_pHDModels;
    vgui2::CheckButton *m_pAddonsFolder;
    vgui2::CheckButton *m_pLowVideoDetail;
    vgui2::CheckButton *m_pDisableMultitexture;
    vgui2::CheckButton *m_pStretchAspect;

    CCvarSlider		*m_pBrightnessSlider;
    CCvarSlider		*m_pGammaSlider;

    char            m_pszRenderName[32];
    char            m_pszAspectName[2][32];

    int             m_iStartResolution;
    bool			m_bStartWidescreen;

    vgui2::DHANDLE<CVideoAdvancedDialog> m_hVideoAdvancedDialog;
};



#endif // OPTIONS_SUB_VIDEO_H