#include "GameUi.h"
#include "OptionsSubMouse.h"
#include "KeyToggleCheckButton.h"
#include "CvarNegateCheckButton.h"
#include "CvarToggleCheckButton.h"
#include "CvarSlider.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <stdio.h>
#include <vgui_controls/TextEntry.h>

COptionsSubMouse::COptionsSubMouse(vgui2::Panel *parent) : PropertyPage(parent, NULL)
{
    m_pReverseMouseCheckBox = new CCvarNegateCheckButton(this, "ReverseMouse", "#GameUI_ReverseMouse", "m_pitch");
    m_pMouseLookCheckBox = new CKeyToggleCheckButton(this, "MouseLook", "#GameUI_MouseLook", "in_mlook", "mlook");
    m_pMouseFilterCheckBox = new CCvarToggleCheckButton(this, "MouseFilter", "#GameUI_MouseFilter", "m_filter");
    m_pMouseRawInputCheckBox = new CCvarToggleCheckButton(this, "RawInput", "#GameUI_RawInput", "m_rawinput");

    m_pJoystickCheckBox = new CCvarToggleCheckButton(this, "Joystick", "#GameUI_Joystick", "joystick");
    m_pJoystickLookCheckBox = new CKeyToggleCheckButton(this, "JoystickLook", "#GameUI_JoystickLook", "in_jlook", "jlook");
    m_pMouseSensitivitySlider = new CCvarSlider(this, "Slider", "#GameUI_MouseSensitivity", 0.2f, 20.0f, "sensitivity");

    m_pMouseSensitivityLabel = new vgui2::TextEntry(this, "SensitivityLabel");
    m_pMouseSensitivityLabel->AddActionSignalTarget(this);

    m_pAutoAimCheckBox = new CCvarToggleCheckButton(this, "Auto-Aim", "#GameUI_AutoAim", "sv_aim");

    LoadControlSettings("Resource\\OptionsSubMouse.res");

    UpdateSensitivityLabel(engine->pfnGetCvarFloat("sensitivity"));
}

COptionsSubMouse::~COptionsSubMouse(void)
{
}

void COptionsSubMouse::OnPageShow(void)
{
}

void COptionsSubMouse::OnResetData(void)
{
    m_pReverseMouseCheckBox->Reset();
    m_pMouseLookCheckBox->Reset();
    m_pMouseFilterCheckBox->Reset();
    m_pMouseRawInputCheckBox->Reset();
    m_pJoystickCheckBox->Reset();
    m_pJoystickLookCheckBox->Reset();
    m_pMouseSensitivitySlider->Reset();
    m_pAutoAimCheckBox->Reset();
}

void COptionsSubMouse::OnApplyChanges(void)
{
    m_pReverseMouseCheckBox->ApplyChanges();
    m_pMouseLookCheckBox->ApplyChanges();
    m_pMouseFilterCheckBox->ApplyChanges();
    m_pMouseRawInputCheckBox->ApplyChanges();
    m_pJoystickCheckBox->ApplyChanges();
    m_pJoystickLookCheckBox->ApplyChanges();
    m_pAutoAimCheckBox->ApplyChanges();

    BoundSensitivityValue();
    m_pMouseSensitivitySlider->ApplyChanges();
}

void COptionsSubMouse::BoundSensitivityValue() {
    char buf[64];
    m_pMouseSensitivityLabel->GetText(buf, 64);
    float fValue = std::clamp((float)atof(buf), 0.2f, 20.0f);
    m_pMouseSensitivitySlider->SetSliderValue(fValue);
    UpdateSensitivityLabel(fValue);
}

void COptionsSubMouse::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    m_pMouseSensitivityLabel->SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
}

void COptionsSubMouse::OnControlModified(Panel *panel)
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));

    if (panel == m_pMouseSensitivitySlider && m_pMouseSensitivitySlider->HasBeenModified())
        UpdateSensitivityLabel(m_pMouseSensitivitySlider->GetSliderValue());
}

void COptionsSubMouse::OnTextChanged(Panel *panel)
{
    if (panel == m_pMouseSensitivityLabel)
    {
        char buf[64];
        m_pMouseSensitivityLabel->GetText(buf, 64);

        float fValue = (float)atof(buf);
        m_pMouseSensitivitySlider->SetSliderValue(fValue);

        PostActionSignal(new KeyValues("ApplyButtonEnable"));
    }
}

void COptionsSubMouse::UpdateSensitivityLabel(float value)
{
    char buf[64];
    Q_snprintf(buf, sizeof(buf), " %.2f", value);
    m_pMouseSensitivityLabel->SetText(buf);
}