#ifndef OPTIONSSUBMULTIPLAYER_H
#define OPTIONSSUBMULTIPLAYER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>
#include <SetinfoTextEntry.h>

class CLabeledCommandComboBox;
class CBitmapImagePanel;

class CCvarToggleCheckButton;
class CCvarTextEntry;
class CCvarSlider;

class CMultiplayerAdvancedDialog;

class COptionsSubMultiplayer : public vgui2::PropertyPage
{
    DECLARE_CLASS_SIMPLE(COptionsSubMultiplayer, vgui2::PropertyPage);

public:
    COptionsSubMultiplayer(vgui2::Panel *parent);
    ~COptionsSubMultiplayer();

public:
    virtual vgui2::Panel *CreateControlByName(const char *controlName);

protected:
    virtual void OnPageShow();
    virtual void OnResetData();
    virtual void OnApplyChanges();
    virtual void OnCommand(const char *command);

private:
    void InitModelList(CLabeledCommandComboBox *cb);
    void InitLogoList(CLabeledCommandComboBox *cb);
    void InitLogoColorEntries();
    void RemapModel();
    void RemapLogo();

private:
    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
    MESSAGE_FUNC_PARAMS(OnSliderMoved, "SliderMoved", data);
    MESSAGE_FUNC(OnApplyButtonEnable, "ControlModified");

private:
    void ColorForName(char const *pszColorName, int &r, int &g, int &b);
    void RemapLogoPalette(char *filename, int r, int g, int b);

private:
    CCvarTextEntry *m_pNameTextEntry;
    CSetinfoTextEntry *m_pPasswordTextEntry;

    CBitmapImagePanel *m_pLogoImage;
    CLabeledCommandComboBox *m_pLogoList;
    CLabeledCommandComboBox *m_pColorList;
    char m_LogoName[128];

    CCvarToggleCheckButton *m_pHighQualityModelCheckBox;

    vgui2::Dar<CCvarToggleCheckButton *> m_cvarToggleCheckButtons;

    int m_nLogoR, m_nLogoG, m_nLogoB;

    vgui2::DHANDLE<CMultiplayerAdvancedDialog> m_hMultiplayerAdvancedDialog;
};

#endif