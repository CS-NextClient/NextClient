#include "VideoAdvancedDialog.h"

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>

CVideoAdvancedDialog::CVideoAdvancedDialog(vgui2::Panel *parent) :
    BaseClass(parent, "CVideoAdvancedDialog")
{
    SetBounds(0, 0, 372, 150);

    SetTitle("#GameUI_VideoAdvanced", true);
    MoveToCenterOfScreen();
    SetSizeable(false);
    SetMoveable(true);
    SetDeleteSelfOnClose(true);

    m_pApplyButton = new vgui2::Button(this, "Apply", "#GameUI_Apply");

    m_pFovWidescreenAdjust = new CCvarToggleCheckButton(this, "FovFixCheckbox", "", "fov_horplus");
    m_pFovWidescreenAdjust->AddActionSignalTarget(this);

    m_pFovSlider = new CCvarSlider(this, "FovAngle", "#GameUI_FovAngle", 70.f, 100.f, "fov_angle");
    m_pFovText = new vgui2::TextEntry(this, "FovAngleText");
    m_pFovText->AddActionSignalTarget(this);


    m_pFovViewmodelAutoCheckbox = new vgui2::CheckButton(this, "ViewmodelFovAutoCheckbox", "");
    m_pFovViewmodelAutoCheckbox->AddActionSignalTarget(this);

    m_pFovViewmodelSlider = new CCvarSlider(this, "FovViewmodelAngle", "#GameUI_FovViewModelAngle", 70.f, 100.f, "viewmodel_fov");
    m_pFovViemodelText = new vgui2::TextEntry(this, "FovViewmodelAngleText");
    m_pFovViemodelText->AddActionSignalTarget(this);

    m_pFovLerpSlider = new CCvarSlider(this, "FovLerpTime", "#GameUI_FovLerpTime", 0.f, 0.8f, "fov_lerp");
    m_pFovLerpText = new vgui2::TextEntry(this, "FovLerpTimeText");
    m_pFovLerpText->AddActionSignalTarget(this);

    LoadControlSettings("Resource\\VideoAvancedDialog.res");

    m_pSettings_ = GetSettings();
    LoadValues();

    m_pApplyButton->SetEnabled(false);
}

CVideoAdvancedDialog::~CVideoAdvancedDialog()
{
    if (m_pSettings_)
    {
        m_pSettings_->deleteThis();
        m_pSettings_ = nullptr;
    }
}

void CVideoAdvancedDialog::Activate()
{
    BaseClass::Activate();

    vgui2::input()->SetAppModalSurface(GetVPanel());
}

void CVideoAdvancedDialog::OnClose()
{
    BaseClass::OnClose();

    vgui2::input()->ReleaseAppModalSurface();
}

void CVideoAdvancedDialog::OnCommand(const char *command)
{
    if (!stricmp(command, "Ok"))
    {
        SaveValues();
        OnClose();
        return;
    }

    if (!stricmp(command, "Apply"))
    {
        m_pApplyButton->SetEnabled(false);
        SaveValues();
        return;
    }

    BaseClass::OnCommand(command);
}

void CVideoAdvancedDialog::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (code == vgui2::KEY_ESCAPE)
    {
        Close();
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}

void CVideoAdvancedDialog::OnCheckButtonChecked(Panel *panel)
{
    OnControlModified(panel);
}

void CVideoAdvancedDialog::OnControlModified(Panel *panel)
{
    bool viewmodel_checkbox_changed = false;

    if (panel == m_pFovViewmodelAutoCheckbox)
    {
        m_pFovViewmodelSlider->SetEnabled(!m_pFovViewmodelAutoCheckbox->IsSelected());

        if (m_pFovViewmodelAutoCheckbox->IsSelected())
        {
            m_pFovViewmodelSlider->SetValue(m_pFovSlider->GetValue());
            UpdateViewmodelFovTextFromSlider();
        }

        viewmodel_checkbox_changed = m_pSettings_->GetBool(kFovViewmodelAutoCheckboxKey, true) != m_pFovViewmodelAutoCheckbox->IsSelected();
    }
    else if (panel == m_pFovSlider)
    {
        UpdateFovTextFromSlider();

        if (m_pFovViewmodelAutoCheckbox->IsSelected())
        {
            m_pFovViewmodelSlider->SetValue(m_pFovSlider->GetValue());
            UpdateViewmodelFovTextFromSlider();
        }
    }
    else if (panel == m_pFovViewmodelSlider)
    {
        UpdateViewmodelFovTextFromSlider();
    }
    else if (panel == m_pFovLerpSlider)
    {
        UpdateFovLerpTextFromSlider();
    }

    m_pApplyButton->SetEnabled(m_pFovSlider->HasBeenModified() ||
                               m_pFovViewmodelSlider->HasBeenModified() ||
                               m_pFovLerpSlider->HasBeenModified() ||
                               m_pFovWidescreenAdjust->HasBeenModified() ||
                               viewmodel_checkbox_changed);
}

void CVideoAdvancedDialog::OnTextChanged(Panel *panel)
{
    if (panel == m_pFovText)
    {
        UpdateFovSliderFromText();

        if (m_pFovViewmodelAutoCheckbox->IsSelected())
        {
            m_pFovViewmodelSlider->SetValue(m_pFovSlider->GetValue());
            UpdateViewmodelFovTextFromSlider();
        }
    }
    else if (panel == m_pFovViemodelText)
    {
        UpdateViewmodelFovSliderFromText();
    }
    else if (panel == m_pFovLerpText)
    {
        UpdateFovLerpSliderFromText();
    }

    m_pApplyButton->SetEnabled(m_pFovSlider->HasBeenModified() ||
                               m_pFovViewmodelSlider->HasBeenModified() ||
                               m_pFovLerpSlider->HasBeenModified());
}

void CVideoAdvancedDialog::UpdateTextFromSlider(CCvarSlider* slider_from, vgui2::TextEntry* text_to, const char* format)
{
    char buf[64];
    Q_snprintf(buf, sizeof(buf), format, slider_from->GetSliderValue());
    text_to->SetText(buf);
}

void CVideoAdvancedDialog::UpdateSliderFromText(vgui2::TextEntry* text_from, CCvarSlider* slider_to)
{
    char buf[64];
    text_from->GetText(buf, 64);

    float fValue = (float)atof(buf);
    slider_to->SetSliderValue(fValue);
}

void CVideoAdvancedDialog::LoadValues()
{
    m_pFovWidescreenAdjust->Reset();

    m_pFovSlider->Reset();
    UpdateFovTextFromSlider();

    m_pFovViewmodelSlider->Reset();
    UpdateViewmodelFovTextFromSlider();

    m_pFovLerpSlider->Reset();
    UpdateFovLerpTextFromSlider();

    m_pFovViewmodelAutoCheckbox->SetSelected(m_pSettings_->GetBool(kFovViewmodelAutoCheckboxKey, true));
}

void CVideoAdvancedDialog::SaveValues()
{
    m_pFovWidescreenAdjust->ApplyChanges();
    m_pFovSlider->ApplyChanges();
    m_pFovViewmodelSlider->ApplyChanges();
    m_pFovLerpSlider->ApplyChanges();

    m_pSettings_->SetBool(kFovViewmodelAutoCheckboxKey, m_pFovViewmodelAutoCheckbox->IsSelected());
    m_pSettings_->SaveToFile(g_pFullFileSystem, kUserSaveDataPath, "GAMECONFIG");
}

KeyValues* CVideoAdvancedDialog::GetSettings()
{
    KeyValues* key_values = new KeyValues("VideoAdvancedSettings");
    key_values->LoadFromFile(g_pFullFileSystem, kUserSaveDataPath, "GAMECONFIG");

    return key_values;
}
