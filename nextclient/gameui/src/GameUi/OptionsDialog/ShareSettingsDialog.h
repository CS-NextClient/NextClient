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

// Turns the Game tab into a share code and back: the checks pick the sections a code is
// made of, and of a pasted code, the sections that get applied.
class CShareSettingsDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CShareSettingsDialog, vgui2::Frame);

public:
    // read_page returns what the tab shows, applied or not; reload_page reads the cvars back
    // into the tab after a code has changed them.
    CShareSettingsDialog(vgui2::Panel* parent, std::function<settings_code::Values()> read_page, std::function<void()> reload_page);

    void Activate() override;
    void OnClose() override;
    void OnCommand(const char* command) override;
    void PerformLayout() override;

    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

private:
    std::function<settings_code::Values()> read_page_;
    std::function<void()> reload_page_;

    vgui2::Label* note_{};
    std::array<vgui2::CheckButton*, settings_code::kSectionCount> section_checks_{};
    vgui2::TextEntry* code_field_{};
    vgui2::Label* status_{};

    // As with the color picker: restored on every close, the title bar's button included.
    vgui2::VPANEL previous_modal_{};

    uint8_t CheckedSections() const;
    void UpdateSectionChecks(uint8_t sections);
    void CopyCode();
    void ApplyCode();
};
