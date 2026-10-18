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

using namespace vgui2;

namespace
{
    constexpr int kWide = 420;
    constexpr int kTall = 330;
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

CShareSettingsDialog::CShareSettingsDialog(Panel* parent, std::function<settings_code::Values()> read_page, std::function<void(const settings_code::Decoded&)> write_page) :
    BaseClass(parent, "ShareSettingsDialog"),
    read_page_(std::move(read_page)),
    write_page_(std::move(write_page))
{
    SetTitle("#GameUI_ShareSettingsTitle", true);
    SetBounds(0, 0, kWide, kTall);
    SetSizeable(false);
    SetDeleteSelfOnClose(true);

    own_caption_ = new Label(this, "OwnCaption", "#GameUI_ShareSettingsOwnCode");

    own_field_ = new TextEntry(this, "OwnCode");
    own_field_->SetEditable(false);

    paste_caption_ = new Label(this, "PasteCaption", "#GameUI_ShareSettingsPasteCode");

    for (int i = 0; i < settings_code::kSectionCount; i++)
    {
        section_checks_[i] = new CheckButton(this, settings_code::kSectionNames[i], kSectionCaptions[i]);
        section_checks_[i]->SetSelected(true);
    }

    code_field_ = new TextEntry(this, "Code");
    code_field_->AddActionSignalTarget(this);

    status_ = new Label(this, "Status", "");

    UpdateSectionChecks(0);

    new Button(this, "Copy", "#GameUI_ShareSettingsCopy", this, "copy");
    new Button(this, "Apply", "#GameUI_ShareSettingsApply", this, "apply");
    new Button(this, "Close", "#GameUI_Close", this, "close");
}

void CShareSettingsDialog::Activate()
{
    MoveToCenterOfScreen();
    BaseClass::Activate();

    ShowOwnCode();

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
    int content_wide = kWide - 2 * kLeft;

    own_caption_->SetBounds(kLeft, y, content_wide, kRowTall);
    y += kRowTall;

    own_field_->SetBounds(kLeft, y, content_wide - kButtonWide - 8, kRowTall);
    if (Panel* copy = FindChildByName("Copy"))
    {
        copy->SetBounds(kLeft + content_wide - kButtonWide, y, kButtonWide, kRowTall);
    }
    y += kRowTall + 16;

    paste_caption_->SetBounds(kLeft, y, content_wide, kRowTall);
    y += kRowTall;

    code_field_->SetBounds(kLeft, y, content_wide, kRowTall);
    y += kRowTall + 4;

    // two columns: three checks on the left, two on the right
    int column_wide = content_wide / 2;

    for (int i = 0; i < settings_code::kSectionCount; i++)
    {
        int column = i / 3;
        int row = i % 3;
        section_checks_[i]->SetBounds(kLeft + column * column_wide, y + row * kRowTall, column_wide, kRowTall);
    }

    y += 3 * kRowTall + 4;

    status_->SetBounds(kLeft, y, content_wide, kRowTall);

    int buttons_y = kTall - kLeft - kRowTall;
    const char* buttons[] = {"Apply", "Close"};

    for (int i = 0; i < 2; i++)
    {
        if (Panel* button = FindChildByName(buttons[i]))
        {
            button->SetBounds(kLeft + content_wide - (2 - i) * kButtonWide - (1 - i) * 8, buttons_y, kButtonWide, kRowTall);
        }
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
    if (panel != code_field_)
    {
        return;
    }

    char code[128];
    code_field_->GetText(code, sizeof(code));

    std::optional<settings_code::Decoded> decoded = settings_code::Decode(code);

    status_->SetText(decoded || code[0] == '\0' ? "" : "#GameUI_ShareSettingsInvalid");
    UpdateSectionChecks(decoded ? decoded->sections : 0);
}

uint8_t CShareSettingsDialog::CheckedSections() const
{
    uint8_t sections = 0;

    for (int i = 0; i < settings_code::kSectionCount; i++)
    {
        if (section_checks_[i]->IsSelected())
        {
            sections |= 1 << i;
        }
    }

    return sections;
}

void CShareSettingsDialog::UpdateSectionChecks(uint8_t sections)
{
    for (int i = 0; i < settings_code::kSectionCount; i++)
    {
        section_checks_[i]->SetEnabled((sections >> i) & 1);
    }
}

void CShareSettingsDialog::ShowOwnCode()
{
    own_field_->SetText(settings_code::Encode(read_page_(), settings_code::kAllSections).c_str());
}

void CShareSettingsDialog::CopyCode()
{
    char code[128];
    own_field_->GetText(code, sizeof(code));

    system()->SetClipboardText(code, V_strlen(code));
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
    if (decoded->sections == 0)
    {
        status_->SetText("#GameUI_ShareSettingsNoTabs");
        return;
    }

    write_page_(*decoded);
    ShowOwnCode();

    status_->SetText("#GameUI_ShareSettingsApplied");
}
