#include "ShareSettingsDialog.h"

#include <optional>
#include <string>
#include <utility>

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

#include "SettingsShare.h"

using namespace vgui2;

namespace
{
    constexpr int kWide = 420;
    constexpr int kTall = 250;
    constexpr int kLeft = 12;
    constexpr int kTop = 34;
    constexpr int kRowTall = 24;
    constexpr int kButtonWide = 120;

    constexpr const char* kSectionCaptions[] = {
        "#GameUI_GameTabCrosshair",
        "#GameUI_GameTabBobbing",
        "#GameUI_GameTabModel",
        "#GameUI_GameTabInertia",
        "#GameUI_GameTabCamera",
    };

    static_assert(std::size(kSectionCaptions) == settings_code::kSectionCount);
}

CShareSettingsDialog::CShareSettingsDialog(Panel* parent, std::function<void()> apply_page, std::function<void()> reload_page) :
    BaseClass(parent, "ShareSettingsDialog"),
    apply_page_(std::move(apply_page)),
    reload_page_(std::move(reload_page))
{
    SetTitle("#GameUI_ShareSettingsTitle", true);
    SetBounds(0, 0, kWide, kTall);
    SetSizeable(false);
    SetDeleteSelfOnClose(true);

    note_ = new Label(this, "Note", "#GameUI_ShareSettingsNote");

    for (int i = 0; i < settings_code::kSectionCount; i++)
    {
        section_checks_[i] = new CheckButton(this, settings_code::kSectionNames[i], kSectionCaptions[i]);
        section_checks_[i]->SetSelected(true);
    }

    code_field_ = new TextEntry(this, "Code");
    code_field_->AddActionSignalTarget(this);

    status_ = new Label(this, "Status", "");

    new Button(this, "Copy", "#GameUI_ShareSettingsCopy", this, "copy");
    new Button(this, "Apply", "#GameUI_ShareSettingsApply", this, "apply");
    new Button(this, "Close", "#GameUI_Close", this, "close");
}

void CShareSettingsDialog::Activate()
{
    MoveToCenterOfScreen();
    BaseClass::Activate();

    previous_modal_ = input()->GetAppModalSurface();
    input()->SetAppModalSurface(GetVPanel());
}

void CShareSettingsDialog::OnClose()
{
    input()->ReleaseAppModalSurface();

    if (previous_modal_ != 0)
    {
        input()->SetAppModalSurface(previous_modal_);
        previous_modal_ = 0;
    }

    BaseClass::OnClose();
}

void CShareSettingsDialog::PerformLayout()
{
    BaseClass::PerformLayout();

    int y = kTop;

    note_->SetBounds(kLeft, y, kWide - 2 * kLeft, kRowTall);
    y += kRowTall + 4;

    // two columns: three checks on the left, two on the right
    int column_wide = (kWide - 2 * kLeft) / 2;

    for (int i = 0; i < settings_code::kSectionCount; i++)
    {
        int column = i / 3;
        int row = i % 3;
        section_checks_[i]->SetBounds(kLeft + column * column_wide, y + row * kRowTall, column_wide, kRowTall);
    }

    y += 3 * kRowTall + 8;

    code_field_->SetBounds(kLeft, y, kWide - 2 * kLeft, kRowTall);
    y += kRowTall + 4;

    status_->SetBounds(kLeft, y, kWide - 2 * kLeft, kRowTall);

    int buttons_y = kTall - kLeft - kRowTall;
    const char* buttons[] = {"Copy", "Apply", "Close"};

    for (int i = 0; i < 3; i++)
    {
        if (Panel* button = FindChildByName(buttons[i]))
            button->SetBounds(kLeft + i * (kButtonWide + 8), buttons_y, kButtonWide, kRowTall);
    }
}

void CShareSettingsDialog::OnCommand(const char* command)
{
    if (V_stricmp(command, "copy") == 0)
    {
        CopyCode();
        return;
    }

    if (V_stricmp(command, "apply") == 0)
    {
        ApplyCode();
        return;
    }

    if (V_stricmp(command, "close") == 0)
    {
        Close();
        return;
    }

    BaseClass::OnCommand(command);
}

void CShareSettingsDialog::OnTextChanged(Panel* panel)
{
    if (panel != code_field_ || filling_field_)
        return;

    char code[128];
    code_field_->GetText(code, sizeof(code));

    std::optional<settings_code::Decoded> decoded = settings_code::Decode(code);

    status_->SetText(decoded || code[0] == '\0' ? "" : "#GameUI_ShareSettingsInvalid");
    UpdateSectionChecks(decoded ? decoded->sections : settings_code::kAllSections);
}

uint8_t CShareSettingsDialog::CheckedSections() const
{
    uint8_t sections = 0;

    for (int i = 0; i < settings_code::kSectionCount; i++)
    {
        if (section_checks_[i]->IsSelected())
            sections |= 1 << i;
    }

    return sections;
}

void CShareSettingsDialog::UpdateSectionChecks(uint8_t sections)
{
    for (int i = 0; i < settings_code::kSectionCount; i++)
        section_checks_[i]->SetEnabled((sections >> i) & 1);
}

void CShareSettingsDialog::CopyCode()
{
    apply_page_();

    std::string code = settings_code::Encode(settings_share::ReadCvars(), CheckedSections());

    filling_field_ = true;
    code_field_->SetText(code.c_str());
    filling_field_ = false;

    UpdateSectionChecks(settings_code::kAllSections);

    system()->SetClipboardText(code.c_str(), static_cast<int>(code.size()));
    status_->SetText("#GameUI_ShareSettingsCopied");
}

void CShareSettingsDialog::ApplyCode()
{
    char code[128];
    code_field_->GetText(code, sizeof(code));

    std::optional<settings_code::Decoded> decoded = settings_code::Decode(code);
    if (!decoded)
    {
        status_->SetText("#GameUI_ShareSettingsInvalid");
        return;
    }

    decoded->sections &= CheckedSections();

    settings_share::WriteCvars(*decoded);
    reload_page_();

    status_->SetText("#GameUI_ShareSettingsApplied");
}
