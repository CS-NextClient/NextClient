#pragma once

#include <vgui_controls/Frame.h>
#include "CvarToggleCheckButton.h"
#include "CvarSlider.h"

class CVideoAdvancedDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CVideoAdvancedDialog, vgui2::Frame);

public:
    explicit CVideoAdvancedDialog(vgui2::Panel *parent);
    ~CVideoAdvancedDialog() override;

    void Activate() override;
    void OnClose() override;
    void OnCommand(const char *command) override;
    void OnKeyCodeTyped(vgui2::KeyCode code) override;

    MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel);
    MESSAGE_FUNC_PTR(OnControlModified, "ControlModified", panel);
    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

private:
    void LoadValues();
    void SaveValues();

    void UpdateFovSliderFromText() { UpdateSliderFromText(m_pFovText, m_pFovSlider); }
    void UpdateViewmodelFovSliderFromText() { UpdateSliderFromText(m_pFovViemodelText, m_pFovViewmodelSlider); }
    void UpdateFovLerpSliderFromText() { UpdateSliderFromText(m_pFovLerpText, m_pFovLerpSlider); }

    void UpdateFovTextFromSlider() { UpdateTextFromSlider(m_pFovSlider, m_pFovText, " %.0f"); }
    void UpdateViewmodelFovTextFromSlider() { UpdateTextFromSlider(m_pFovViewmodelSlider, m_pFovViemodelText, " %.0f"); }
    void UpdateFovLerpTextFromSlider() { UpdateTextFromSlider(m_pFovLerpSlider, m_pFovLerpText); }

    static void UpdateSliderFromText(vgui2::TextEntry* text_from, CCvarSlider* slider_to);
    static void UpdateTextFromSlider(CCvarSlider* slider_from, vgui2::TextEntry* text_to, const char* format = " %.2f");

private:
    vgui2::Button*           m_pApplyButton;

    CCvarToggleCheckButton*  m_pFovWidescreenAdjust;

    CCvarSlider*             m_pFovSlider;
    vgui2::TextEntry*        m_pFovText;

    vgui2::CheckButton*      m_pFovViewmodelAutoCheckbox;
    CCvarSlider*             m_pFovViewmodelSlider;
    vgui2::TextEntry*        m_pFovViemodelText;

    CCvarSlider*             m_pFovLerpSlider;
    vgui2::TextEntry*        m_pFovLerpText;

    KeyValues*               m_pSettings_;

public:
    constexpr static char kUserSaveDataPath[] = "VideoAdvancedSettings.vdf";
    constexpr static char kFovViewmodelAutoCheckboxKey[] = "FovViewmodelAuto";

    static KeyValues* GetSettings();
};
