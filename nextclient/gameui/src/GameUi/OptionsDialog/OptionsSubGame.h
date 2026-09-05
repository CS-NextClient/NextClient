#pragma once

#include <array>
#include <functional>
#include <vector>

#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/TextEntry.h>

#include <cvars/cvar_defaults.h>
#include <view/view_bob.h>

#include "CvarToggleCheckButton.h"
#include "CvarSlider.h"
#include "LabeledCommandComboBox.h"
#include "ColorPickerDialog.h"
#include "SettingsGridPage.h"
#include "BobPreviewPanel.h"

class CPixelPanel;

// Gameplay options: a live first-person preview on top and a tab per category
// (crosshair, bobbing, weapon model, inertia, camera) underneath.
class COptionsSubGame : public vgui2::PropertyPage, public CColorPickerDialog::IListener
{
    DECLARE_CLASS_SIMPLE(COptionsSubGame, vgui2::PropertyPage);

    struct Preset
    {
        const char* label;
        std::function<void()> apply;
    };

    struct SliderRow
    {
        CCvarSlider* slider;
        vgui2::TextEntry* text;
        const char* format;
    };

    struct Tab
    {
        CSettingsGridPage* page;
        std::vector<Preset> presets;
        PreviewMove move;
        PreviewDemo demo;
        std::function<void()> reset_defaults;
    };

private:
    vgui2::Button* defaults_button_{};
    vgui2::Label* presets_caption_{};
    std::array<vgui2::Button*, 4> preset_buttons_{};

    vgui2::PropertySheet* tabs_{};
    std::vector<Tab> tab_table_;
    CSettingsGridPage* crosshair_page_{};
    CSettingsGridPage* bobbing_page_{};
    CSettingsGridPage* model_page_{};
    CSettingsGridPage* inertia_page_{};
    CSettingsGridPage* camera_page_{};

    // one preview movement button per PreviewMove value, the active one held depressed
    std::array<vgui2::Button*, 4> move_buttons_{};
    PreviewMove preview_move_{};
    vgui2::Panel* last_active_page_{};

    CLabeledCommandComboBox* crosshair_type_combo_{};
    CLabeledCommandComboBox* crosshair_size_combo_{};
    CPixelPanel* crosshair_color_swatch_{};
    vgui2::Button* crosshair_color_button_{};
    ncl_math::Color crosshair_color_{};

    // What the cvars held when the page was last read, so a setting the player did not
    // touch here is not written back over a change made from the console meanwhile.
    ncl_math::Color saved_crosshair_color_{};
    int saved_style_{};
    int saved_lag_style_{};
    CCvarToggleCheckButton* crosshair_translucent_check_{};
    CCvarToggleCheckButton* crosshair_dynamic_check_{};

    vgui2::ComboBox* style_combo_{};
    CCvarSlider* bob_slider_{};
    vgui2::TextEntry* bob_text_{};
    CCvarSlider* cycle_slider_{};
    vgui2::TextEntry* cycle_text_{};
    CCvarSlider* up_slider_{};
    vgui2::TextEntry* up_text_{};
    CCvarSlider* amt_vert_slider_{};
    vgui2::TextEntry* amt_vert_text_{};
    CCvarSlider* amt_lat_slider_{};
    vgui2::TextEntry* amt_lat_text_{};
    CCvarSlider* lower_slider_{};
    vgui2::TextEntry* lower_text_{};

    CCvarSlider* offset_x_slider_{};
    vgui2::TextEntry* offset_x_text_{};
    CCvarSlider* offset_y_slider_{};
    vgui2::TextEntry* offset_y_text_{};
    CCvarSlider* offset_z_slider_{};
    vgui2::TextEntry* offset_z_text_{};
    CCvarSlider* viewmodel_fov_slider_{};
    vgui2::TextEntry* viewmodel_fov_text_{};
    CCvarToggleCheckButton* disable_shift_check_{};

    vgui2::ComboBox* lag_style_combo_{};
    CCvarSlider* lag_scale_slider_{};
    vgui2::TextEntry* lag_scale_text_{};
    CCvarSlider* lag_speed_slider_{};
    vgui2::TextEntry* lag_speed_text_{};

    CCvarSlider* roll_angle_slider_{};
    vgui2::TextEntry* roll_angle_text_{};
    CCvarSlider* roll_speed_slider_{};
    vgui2::TextEntry* roll_speed_text_{};
    CCvarSlider* camera_move_scale_slider_{};
    vgui2::TextEntry* camera_move_scale_text_{};
    CCvarSlider* camera_move_interp_slider_{};
    vgui2::TextEntry* camera_move_interp_text_{};
    CCvarToggleCheckButton* camera_check_{};

    CBobPreviewPanel* preview_{};

    std::vector<SliderRow> rows_;
    std::vector<CCvarToggleCheckButton*> checks_;

public:
    constexpr static char kCrosshairTabName[] = "#GameUI_GameTabCrosshair";

    explicit COptionsSubGame(vgui2::Panel* parent);

    void ShowCrosshairTab();

    void PerformLayout() override;
    void OnPageShow() override;
    void OnResetData() override;
    void OnApplyChanges() override;
    void OnThink() override;
    void OnCommand(const char* command) override;

    MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel);
    MESSAGE_FUNC_PTR(OnControlModified, "ControlModified", panel);
    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);

    void OnColorPicked(const char* param, const float rgba[4]) override;

protected:
    void OnScreenSizeChanged(int old_wide, int old_tall) override;

private:
    void BuildShell();
    void BuildCrosshairTab();
    void BuildBobbingTab();
    void BuildModelTab();
    void BuildInertiaTab();
    void BuildCameraTab();

    // a slider with its value entry, registered with the page as its action signal target
    CCvarSlider* AddSlider(CSettingsGridPage* page, const char* name, const char* caption, float min, float max, const char* cvar, const char* format, vgui2::TextEntry*& text);
    CCvarToggleCheckButton* AddCheck(CSettingsGridPage* page, const char* name, const char* caption, const char* cvar);

    const Tab* FindTab(vgui2::Panel* page) const;
    void SetDefaults();
    void ResetCrosshairToDefaults();
    void ApplyPreset(int index);
    void UpdatePresetButtons();
    void SetSliderValue(CCvarSlider* slider, float value);
    void ResetSlider(CCvarSlider* slider);
    int GetSelectedStyle() const;
    int GetSelectedLagStyle() const;
    void SetPreviewMove(PreviewMove move);
    void UpdateControlStates();
    void NotifyDataChanged();
    void SyncPreview();
    void InitCrosshairTypeItems();
    void InitCrosshairSizeItems();
    void SetCrosshairColor(const ncl_math::Color& color);
    void LoadCrosshairColor();

    static void UpdateTextFromSlider(const SliderRow& row);
    static void UpdateSliderFromText(const SliderRow& row);
};
