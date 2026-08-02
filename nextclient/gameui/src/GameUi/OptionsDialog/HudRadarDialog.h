#pragma once

#include <vgui_controls/Frame.h>
#include "CvarToggleCheckButton.h"
#include "CvarSlider.h"

namespace vgui2
{
    class ComboBox;
    class TextEntry;
    class Button;
} // namespace vgui2

class CHudRadarDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CHudRadarDialog, vgui2::Frame);

public:
    static constexpr int kCheckCount = 8;
    static constexpr int kSnapshotCount = 6 + kCheckCount; // 6 sliders/combos + toggles

    explicit CHudRadarDialog(vgui2::Panel* parent);

    void Activate() override;
    void OnClose() override;
    void OnCommand(const char* command) override;
    void OnKeyCodeTyped(vgui2::KeyCode code) override;

    MESSAGE_FUNC_PTR(OnControlModified, "ControlModified", panel);
    MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel);
    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

private:
    void LoadValues();
    void ApplyValues();
    void ResetToDefaults();
    void UpdateTexts();
    void MarkModified();
    void UpdateEnabledStates();
    void SetControlsSilent(bool silent);
    CCvarToggleCheckButton* FindCheck(const char* cvar) const;
    void TakeSnapshot();
    void RestoreSnapshot();
    bool HasChanges() const;
    void ShowCloseConfirm();

    vgui2::ComboBox* m_pMode;
    vgui2::ComboBox* m_pMapMode;

    CCvarSlider* m_pRadarScale;
    CCvarSlider* m_pMapScale;
    CCvarSlider* m_pOpacity;
    CCvarSlider* m_pNameSize;
    vgui2::TextEntry* m_pRadarScaleText;
    vgui2::TextEntry* m_pMapScaleText;
    vgui2::TextEntry* m_pOpacityText;
    vgui2::TextEntry* m_pNameSizeText;

    CCvarToggleCheckButton* m_pChecks[kCheckCount];

    vgui2::Button* m_pApplyButton;

    // Radar cvar values captured when the dialog opens. When editing in a level
    // (m_bLiveApply) changes apply to the radar immediately, and Cancel restores this
    // snapshot to undo them.
    float m_snapshot[kSnapshotCount];
    bool m_bLiveApply;
};
