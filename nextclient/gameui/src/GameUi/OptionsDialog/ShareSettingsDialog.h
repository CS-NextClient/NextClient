#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include <vgui_controls/Frame.h>

#include <settings_code/settings_code.h>

namespace vgui2
{
    class CheckButton;
    class Label;
    class TextEntry;
} // namespace vgui2

// Shows the Game tab as a share code with every section, and loads a pasted code into the
// tab; the checks pick which of the pasted code's sections are loaded.
class CShareSettingsDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CShareSettingsDialog, vgui2::Frame);

public:
    // read_page returns what the tab shows, applied or not; write_page puts a code's sections
    // into the tab's controls.
    CShareSettingsDialog(vgui2::Panel* parent, std::function<settings_code::Values()> read_page, std::function<void(const settings_code::Decoded&)> write_page);

    void Activate() override;
    void OnClose() override;
    void OnCommand(const char* command) override;
    void PerformLayout() override;

    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

private:
    std::function<settings_code::Values()> read_page_;
    std::function<void(const settings_code::Decoded&)> write_page_;

    vgui2::Label* own_caption_{};
    vgui2::TextEntry* own_field_{};
    vgui2::Label* paste_caption_{};
    std::array<vgui2::CheckButton*, settings_code::kSectionCount> section_checks_{};
    vgui2::TextEntry* code_field_{};
    vgui2::Label* status_{};

    // As with the color picker: restored on every close, the title bar's button included.
    vgui2::VPANEL previous_modal_{};

    uint8_t CheckedSections() const;
    void UpdateSectionChecks(uint8_t sections);
    void ShowOwnCode();
    void CopyCode();
    void ApplyCode();
};
