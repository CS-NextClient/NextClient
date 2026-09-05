#include "OptionsSubGame.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <GameUi.h>
#include <KeyValues.h>
#include <crosshair/crosshair.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Tooltip.h>

#include "Controls/PixelPanel.h"

namespace
{
    struct MoveButtonDesc
    {
        PreviewMove move;
        const char* field_name;
        const char* label;
        const char* command;
    };

    constexpr MoveButtonDesc kMoveButtons[] = {
        {PreviewMove::kRun, "MoveRun", "#GameUI_BobPreviewRun", "MoveRun"},
        {PreviewMove::kWalk, "MoveWalk", "#GameUI_BobPreviewWalk", "MoveWalk"},
        {PreviewMove::kStrafe, "MoveStrafe", "#GameUI_BobPreviewStrafe", "MoveStrafe"},
        {PreviewMove::kIdle, "MoveIdle", "#GameUI_BobPreviewIdle", "MoveIdle"},
    };

    // the crosshair keeps its own color, so the picker edits it without an alpha bar
    constexpr char kCrosshairColorParam[] = "crosshair";

    // captions of crosshair::kSizes, in its order
    constexpr const char* kCrosshairSizeLabels[crosshair::kSizeCount] = {
        "#GameUI_Auto",
        "#GameUI_Small",
        "#GameUI_Medium",
        "#GameUI_Large",
        "#GameUI_ExtraSmall",
    };

    constexpr char kPresetCommandFormat[] = "Preset%d";

    constexpr int kSliderTall = 36;
    constexpr int kComboTall = 22;
    constexpr int kCheckTall = 24;

    constexpr int kMargin = 6;
    constexpr int kGap = 8;
    constexpr int kDefaultsTall = 24;
    constexpr int kPresetsWide = 150;
    constexpr int kPresetTall = 22;
    constexpr int kPresetGap = 4;
    constexpr int kMoveButtonTall = 20;
    constexpr int kMoveButtonGap = 3;

    // room a button sized from its caption gets around the text
    constexpr int kButtonPadding = 16;

    constexpr int kPreviewMinTall = 120;
    constexpr int kPreviewMaxTall = 240;

    Color Opaque(Color color)
    {
        return Color(color.r(), color.g(), color.b(), 255);
    }

    Color Lighten(Color color, int delta)
    {
        return Color(
            std::min(255, color.r() + delta),
            std::min(255, color.g() + delta),
            std::min(255, color.b() + delta),
            color.a());
    }

    // "r g b" as the cvar carries it; fallback for anything shorter
    ncl_math::Color ParseColor(const char* text, const ncl_math::Color& fallback)
    {
        int r = 0;
        int g = 0;
        int b = 0;

        if (text == nullptr || sscanf(text, "%d %d %d", &r, &g, &b) != 3)
            return fallback;

        return {std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255)};
    }

    ncl_math::Color DefaultCrosshairColor()
    {
        return ParseColor(cvars::kCrosshairColor.value, ncl_math::Color{});
    }

    class CMoveButton : public vgui2::Button
    {
        DECLARE_CLASS_SIMPLE(CMoveButton, vgui2::Button);

    public:
        CMoveButton(vgui2::Panel* parent, const char* panel_name, const char* text) :
            BaseClass(parent, panel_name, text)
        {
        }

        void ApplySchemeSettings(vgui2::IScheme* scheme) override
        {
            BaseClass::ApplySchemeSettings(scheme);

            Color frame_bg = GetSchemeColor("Frame.BgColor", Color(76, 88, 68, 255), scheme);

            // a transparent scheme background would let the page through
            Color base_bg = GetSchemeColor("Button.BgColor", frame_bg, scheme);
            if (base_bg.a() == 0)
                base_bg = frame_bg;

            base_bg = Opaque(base_bg);

            Color text = GetSchemeColor("Button.TextColor", GetFgColor(), scheme);
            Color text_hover = GetSchemeColor("Button.ArmedTextColor", text, scheme);
            Color text_active = GetSchemeColor("Button.DepressedTextColor", text, scheme);

            // The schemes disagree on button backgrounds (ClassicPlus arms with "Blank",
            // Source defines none at all), so the states are derived from scheme surface
            // colors and forced opaque.
            SetPaintBackgroundEnabled(true);
            SetDefaultColor(text, base_bg);
            SetArmedColor(text_hover, Lighten(base_bg, 18));
            SetDepressedColor(text_active, Lighten(base_bg, 40));
        }
    };
}

COptionsSubGame::COptionsSubGame(vgui2::Panel* parent) :
    BaseClass(parent, nullptr)
{
    BuildShell();
    BuildCrosshairTab();
    BuildBobbingTab();
    BuildModelTab();
    BuildInertiaTab();
    BuildCameraTab();

    tabs_->AddPage(crosshair_page_, kCrosshairTabName);
    tabs_->AddPage(bobbing_page_, "#GameUI_GameTabBobbing");
    tabs_->AddPage(model_page_, "#GameUI_GameTabModel");
    tabs_->AddPage(inertia_page_, "#GameUI_GameTabInertia");
    tabs_->AddPage(camera_page_, "#GameUI_GameTabCamera");
}

void COptionsSubGame::BuildShell()
{
    preview_ = new CBobPreviewPanel(this, "GamePreview");

    for (const MoveButtonDesc& desc : kMoveButtons)
    {
        vgui2::Button* button = new CMoveButton(this, desc.field_name, desc.label);
        button->SetCommand(desc.command);
        move_buttons_[static_cast<int>(desc.move)] = button;
    }

    presets_caption_ = new vgui2::Label(this, "PresetsCaption", "#GameUI_GamePresets");
    presets_caption_->SetContentAlignment(vgui2::Label::a_west);

    for (int i = 0; i < static_cast<int>(preset_buttons_.size()); i++)
    {
        char command[16];
        Q_snprintf(command, sizeof(command), kPresetCommandFormat, i);

        preset_buttons_[i] = new vgui2::Button(this, command, "");
        preset_buttons_[i]->SetCommand(command);
        preset_buttons_[i]->SetVisible(false);
    }

    defaults_button_ = new vgui2::Button(this, "Defaults", "#GameUI_ViewDefaultsBtn");
    defaults_button_->SetCommand("Defaults");

    tabs_ = new vgui2::PropertySheet(this, "GameTabs");
}

CCvarSlider* COptionsSubGame::AddSlider(CSettingsGridPage* page, const char* name, const char* caption, float min, float max, const char* cvar, const char* format, vgui2::TextEntry*& text)
{
    CCvarSlider* slider = new CCvarSlider(page, name, caption, min, max, cvar);

    char text_name[64];
    Q_snprintf(text_name, sizeof(text_name), "%sText", name);
    text = new vgui2::TextEntry(page, text_name);

    // the tab pages own the build groups, so this page does not become an action signal
    // target through LoadControlSettings and the controls must register it explicitly
    slider->AddActionSignalTarget(this);
    text->AddActionSignalTarget(this);

    const char* default_value = cvars::FindDefault(cvar);
    if (default_value)
        slider->SetDefaultValue(static_cast<float>(atof(default_value)));

    rows_.push_back(SliderRow{slider, text, format});

    return slider;
}

CCvarToggleCheckButton* COptionsSubGame::AddCheck(CSettingsGridPage* page, const char* name, const char* caption, const char* cvar)
{
    CCvarToggleCheckButton* check = new CCvarToggleCheckButton(page, name, caption, cvar);
    check->AddActionSignalTarget(this);

    const char* default_value = cvars::FindDefault(cvar);
    if (default_value)
        check->SetDefaultValue(atoi(default_value) != 0);

    checks_.push_back(check);

    return check;
}

void COptionsSubGame::BuildCrosshairTab()
{
    crosshair_page_ = new CSettingsGridPage(this, "CrosshairPage");

    crosshair_type_combo_ = new CLabeledCommandComboBox(crosshair_page_, "CrosshairType");
    crosshair_size_combo_ = new CLabeledCommandComboBox(crosshair_page_, "CrosshairSize");
    crosshair_type_combo_->AddActionSignalTarget(this);
    crosshair_size_combo_->AddActionSignalTarget(this);

    crosshair_color_swatch_ = new CPixelPanel(crosshair_page_, "CrosshairColorSwatch", 2, 2);
    crosshair_color_swatch_->SetMouseInputEnabled(false);

    crosshair_color_button_ = new vgui2::Button(crosshair_page_, "CrosshairColorPick", "#GameUI_CrosshairColorPick");
    crosshair_color_button_->SetCommand("PickCrosshairColor");
    crosshair_color_button_->AddActionSignalTarget(this);

    crosshair_translucent_check_ = AddCheck(crosshair_page_, "CrosshairTranslucent", "#GameUI_Translucent", cvars::kCrosshairTranslucent.name);
    crosshair_dynamic_check_ = AddCheck(crosshair_page_, "CrosshairDynamic", "#GameUI_CrosshairDynamic", cvars::kDynamicCrosshair.name);

    crosshair_page_->AddCell("#GameUI_CrosshairType", crosshair_type_combo_, nullptr, kComboTall);
    crosshair_page_->AddCell("#GameUI_CrosshairSize", crosshair_size_combo_, nullptr, kComboTall);
    crosshair_page_->AddCell("#GameUI_CrosshairColor", crosshair_color_button_, crosshair_color_swatch_, kComboTall);
    crosshair_page_->AddWideCell(crosshair_translucent_check_, kCheckTall);
    crosshair_page_->AddWideCell(crosshair_dynamic_check_, kCheckTall);

    Tab tab{crosshair_page_, {}, PreviewMove::kIdle, PreviewDemo::kNone, [this] { ResetCrosshairToDefaults(); }};

    tab.presets = {
        {"#GameUI_PresetClassic", [this] {
            ResetCrosshairToDefaults();
        }},
        {"#GameUI_PresetCrosshairDot", [this] {
            crosshair_type_combo_->ActivateItem(crosshair::kTypeDot);
            crosshair_dynamic_check_->SetSelected(false);
        }},
        {"#GameUI_PresetCrosshairStatic", [this] {
            crosshair_dynamic_check_->SetSelected(false);
        }},
    };

    tab_table_.push_back(std::move(tab));
}

void COptionsSubGame::BuildBobbingTab()
{
    bobbing_page_ = new CSettingsGridPage(this, "BobbingPage");

    style_combo_ = new vgui2::ComboBox(bobbing_page_, "BobStyle", 3, false);
    style_combo_->AddItem("#GameUI_BobStyleClassic", KeyValues::AutoDelete(new KeyValues("", "style", view_bob::kStyleClassic)));
    style_combo_->AddItem("#GameUI_BobStyleClassicSway", KeyValues::AutoDelete(new KeyValues("", "style", view_bob::kStyleClassicSway)));
    style_combo_->AddItem("#GameUI_BobStyleModern", KeyValues::AutoDelete(new KeyValues("", "style", view_bob::kStyleModern)));
    style_combo_->AddActionSignalTarget(this);

    bob_slider_ = AddSlider(bobbing_page_, "BobAmount", "#GameUI_BobAmount", 0.0f, 0.05f, cvars::kBob.name, " %.3f", bob_text_);
    bob_slider_->SetScale(1000.0f, 3);

    cycle_slider_ = AddSlider(bobbing_page_, "BobCycle", "#GameUI_BobCycle", 0.1f, 2.0f, cvars::kBobCycle.name, " %.2f", cycle_text_);
    up_slider_ = AddSlider(bobbing_page_, "BobUp", "#GameUI_BobUp", 0.05f, 0.95f, cvars::kBobUp.name, " %.2f", up_text_);
    amt_vert_slider_ = AddSlider(bobbing_page_, "BobAmtVert", "#GameUI_BobAmtVert", 0.0f, 0.4f, cvars::kBobAmtVert.name, " %.2f", amt_vert_text_);
    amt_lat_slider_ = AddSlider(bobbing_page_, "BobAmtLat", "#GameUI_BobAmtLat", 0.0f, 0.8f, cvars::kBobAmtLat.name, " %.2f", amt_lat_text_);

    lower_slider_ = AddSlider(bobbing_page_, "BobLowerAmt", "#GameUI_BobLowerAmt", 0.0f, 30.0f, cvars::kBobLowerAmt.name, " %.0f", lower_text_);
    lower_slider_->SetScale(1.0f, 0);

    bobbing_page_->AddCell("#GameUI_BobStyle", style_combo_, nullptr, kComboTall);
    bobbing_page_->AddCell("#GameUI_BobCycle", cycle_slider_, cycle_text_, kSliderTall);
    bobbing_page_->AddCell("#GameUI_BobUp", up_slider_, up_text_, kSliderTall);
    bobbing_page_->AddCell("#GameUI_BobAmount", bob_slider_, bob_text_, kSliderTall);
    bobbing_page_->AddCell("#GameUI_BobAmtVert", amt_vert_slider_, amt_vert_text_, kSliderTall);
    bobbing_page_->AddCell("#GameUI_BobAmtLat", amt_lat_slider_, amt_lat_text_, kSliderTall);
    bobbing_page_->AddCell("#GameUI_BobLowerAmt", lower_slider_, lower_text_, kSliderTall);

    Tab tab{bobbing_page_, {}, PreviewMove::kRun, PreviewDemo::kNone, [this] {
        style_combo_->ActivateItemByRow(atoi(cvars::kBobStyle.value));
    }};

    tab.presets = {
        {"#GameUI_PresetClassic", [this] {
            style_combo_->ActivateItemByRow(view_bob::kStyleClassic);
            ResetSlider(bob_slider_);
            ResetSlider(cycle_slider_);
            ResetSlider(up_slider_);
        }},
        {"#GameUI_PresetModern", [this] {
            style_combo_->ActivateItemByRow(view_bob::kStyleModern);
            ResetSlider(cycle_slider_);
            ResetSlider(up_slider_);
            ResetSlider(amt_vert_slider_);
            ResetSlider(amt_lat_slider_);
            ResetSlider(lower_slider_);
        }},
        {"#GameUI_PresetNone", [this] {
            // every amplitude, and not the style: the weapon then stands still whichever
            // style the player keeps selected
            SetSliderValue(bob_slider_, 0.0f);
            SetSliderValue(amt_vert_slider_, 0.0f);
            SetSliderValue(amt_lat_slider_, 0.0f);
            SetSliderValue(lower_slider_, 0.0f);
        }},
    };

    tab_table_.push_back(std::move(tab));
}

void COptionsSubGame::BuildModelTab()
{
    model_page_ = new CSettingsGridPage(this, "ModelPage");

    offset_x_slider_ = AddSlider(model_page_, "OffsetX", "#GameUI_ViewmodelOffsetX", -8.0f, 8.0f, cvars::kViewmodelOffsetX.name, " %.2f", offset_x_text_);
    offset_y_slider_ = AddSlider(model_page_, "OffsetY", "#GameUI_ViewmodelOffsetY", -8.0f, 8.0f, cvars::kViewmodelOffsetY.name, " %.2f", offset_y_text_);
    offset_z_slider_ = AddSlider(model_page_, "OffsetZ", "#GameUI_ViewmodelOffsetZ", -8.0f, 8.0f, cvars::kViewmodelOffsetZ.name, " %.2f", offset_z_text_);

    viewmodel_fov_slider_ = AddSlider(model_page_, "ViewmodelFov", "#GameUI_ViewmodelFov", 70.0f, 100.0f, cvars::kViewmodelFov.name, " %.0f", viewmodel_fov_text_);
    viewmodel_fov_slider_->SetScale(1.0f, 0);

    disable_shift_check_ = AddCheck(model_page_, "DisableShift", "#GameUI_ViewmodelDisableShift", cvars::kViewmodelDisableShift.name);

    model_page_->AddCell("#GameUI_ViewmodelOffsetX", offset_x_slider_, offset_x_text_, kSliderTall);
    model_page_->AddCell("#GameUI_ViewmodelOffsetY", offset_y_slider_, offset_y_text_, kSliderTall);
    model_page_->AddCell("#GameUI_ViewmodelOffsetZ", offset_z_slider_, offset_z_text_, kSliderTall);
    model_page_->AddCell("#GameUI_ViewmodelFov", viewmodel_fov_slider_, viewmodel_fov_text_, kSliderTall);
    model_page_->AddWideCell(disable_shift_check_, kCheckTall);

    Tab tab{model_page_, {}, PreviewMove::kIdle, PreviewDemo::kNone, nullptr};

    tab.presets = {
        {"#GameUI_PresetDefault", [this] {
            ResetSlider(offset_x_slider_);
            ResetSlider(offset_y_slider_);
            ResetSlider(offset_z_slider_);
            ResetSlider(viewmodel_fov_slider_);
        }},
        {"#GameUI_PresetModelCentered", [this] {
            SetSliderValue(offset_x_slider_, -1.5f);
            SetSliderValue(offset_y_slider_, 1.0f);
            SetSliderValue(offset_z_slider_, 0.5f);
        }},
        {"#GameUI_PresetModelWide", [this] {
            SetSliderValue(viewmodel_fov_slider_, 100.0f);
        }},
    };

    tab_table_.push_back(std::move(tab));
}

void COptionsSubGame::BuildInertiaTab()
{
    inertia_page_ = new CSettingsGridPage(this, "InertiaPage");

    lag_style_combo_ = new vgui2::ComboBox(inertia_page_, "LagStyle", 3, false);
    lag_style_combo_->AddItem("#GameUI_ViewLagOff", KeyValues::AutoDelete(new KeyValues("", "style", 0)));
    lag_style_combo_->AddItem("#GameUI_ViewLagHL2", KeyValues::AutoDelete(new KeyValues("", "style", 1)));
    lag_style_combo_->AddItem("#GameUI_ViewLagCSS", KeyValues::AutoDelete(new KeyValues("", "style", 2)));
    lag_style_combo_->AddActionSignalTarget(this);

    lag_scale_slider_ = AddSlider(inertia_page_, "LagScale", "#GameUI_ViewLagScale", 0.0f, 5.0f, cvars::kViewmodelLagScale.name, " %.2f", lag_scale_text_);

    lag_speed_slider_ = AddSlider(inertia_page_, "LagSpeed", "#GameUI_ViewLagSpeed", 1.0f, 20.0f, cvars::kViewmodelLagSpeed.name, " %.1f", lag_speed_text_);
    lag_speed_slider_->SetScale(10.0f, 1);

    inertia_page_->AddCell("#GameUI_ViewLagStyle", lag_style_combo_, nullptr, kComboTall);
    inertia_page_->AddCell("#GameUI_ViewLagScale", lag_scale_slider_, lag_scale_text_, kSliderTall);
    inertia_page_->AddCell("#GameUI_ViewLagSpeed", lag_speed_slider_, lag_speed_text_, kSliderTall);

    Tab tab{inertia_page_, {}, PreviewMove::kIdle, PreviewDemo::kLag, [this] {
        lag_style_combo_->ActivateItemByRow(atoi(cvars::kViewmodelLagStyle.value));
    }};

    tab.presets = {
        {"#GameUI_PresetOff", [this] {
            lag_style_combo_->ActivateItemByRow(0);
        }},
        {"#GameUI_ViewLagHL2", [this] {
            lag_style_combo_->ActivateItemByRow(1);
            ResetSlider(lag_scale_slider_);
            ResetSlider(lag_speed_slider_);
        }},
        {"#GameUI_ViewLagCSS", [this] {
            lag_style_combo_->ActivateItemByRow(2);
            ResetSlider(lag_scale_slider_);
        }},
    };

    tab_table_.push_back(std::move(tab));
}

void COptionsSubGame::BuildCameraTab()
{
    camera_page_ = new CSettingsGridPage(this, "CameraPage");

    roll_angle_slider_ = AddSlider(camera_page_, "RollAngle", "#GameUI_RollAngle", 0.0f, 10.0f, cvars::kRollAngle.name, " %.1f", roll_angle_text_);
    roll_angle_slider_->SetScale(10.0f, 1);

    roll_speed_slider_ = AddSlider(camera_page_, "RollSpeed", "#GameUI_RollSpeed", 10.0f, 400.0f, cvars::kRollSpeed.name, " %.0f", roll_speed_text_);
    roll_speed_slider_->SetScale(1.0f, 0);

    camera_move_scale_slider_ = AddSlider(camera_page_, "CameraMoveScale", "#GameUI_CameraMoveScale", 0.0f, 2.0f, cvars::kCameraMovementScale.name, " %.2f", camera_move_scale_text_);
    camera_move_interp_slider_ = AddSlider(camera_page_, "CameraMoveInterp", "#GameUI_CameraMoveInterp", 0.0f, 0.5f, cvars::kCameraMovementInterp.name, " %.2f", camera_move_interp_text_);

    camera_check_ = AddCheck(camera_page_, "CameraBob", "#GameUI_BobCamera", cvars::kBobCamera.name);
    camera_check_->GetTooltip()->SetText("#GameUI_BobCameraTooltip");

    camera_page_->AddCell("#GameUI_RollAngle", roll_angle_slider_, roll_angle_text_, kSliderTall);
    camera_page_->AddCell("#GameUI_RollSpeed", roll_speed_slider_, roll_speed_text_, kSliderTall);
    camera_page_->AddCell("#GameUI_CameraMoveScale", camera_move_scale_slider_, camera_move_scale_text_, kSliderTall);
    camera_page_->AddCell("#GameUI_CameraMoveInterp", camera_move_interp_slider_, camera_move_interp_text_, kSliderTall);
    camera_page_->AddWideCell(camera_check_, kCheckTall);

    Tab tab{camera_page_, {}, PreviewMove::kStrafe, PreviewDemo::kWeaponSwitch, nullptr};

    tab.presets = {
        {"#GameUI_PresetCameraCalm", [this] {
            ResetSlider(roll_angle_slider_);
            SetSliderValue(camera_move_scale_slider_, 0.0f);
            ResetSlider(camera_move_interp_slider_);
        }},
        {"#GameUI_PresetCameraQuake", [this] {
            SetSliderValue(roll_angle_slider_, 2.0f);
            ResetSlider(roll_speed_slider_);
        }},
        {"#GameUI_PresetCameraCinematic", [this] {
            SetSliderValue(camera_move_scale_slider_, 1.5f);
            SetSliderValue(camera_move_interp_slider_, 0.1f);
        }},
    };

    tab_table_.push_back(std::move(tab));
}

void COptionsSubGame::PerformLayout()
{
    BaseClass::PerformLayout();

    int wide, tall;
    GetSize(wide, tall);

    int content_wide = wide - 2 * kMargin;
    if (content_wide <= 0)
        return;

    // The scene is framed like the player's screen, so the preview takes the shape of the
    // video mode and the movement buttons stand beside it rather than under it.
    float aspect = CBobPreviewPanel::get_screen_aspect();

    int available_tall = tall - 2 * kMargin - 2 * kGap - kDefaultsTall;

    // the tab strip's height is the scheme's: the sheet lays its page out below the strip, and
    // the offset is read back after a forced layout rather than assumed
    tabs_->InvalidateLayout(true);

    int page_x = 0;
    int tab_strip_tall = 0;

    if (tabs_->GetActivePage())
        tabs_->GetActivePage()->GetPos(page_x, tab_strip_tall);

    // the tabs take the room their tallest page needs, the preview keeps the rest
    int pages_tall = 0;

    for (const Tab& tab : tab_table_)
    {
        pages_tall = std::max(pages_tall, tab.page->MeasurePreferredTall(content_wide));
    }

    int preview_tall = std::clamp(available_tall - pages_tall - tab_strip_tall, kPreviewMinTall, kPreviewMaxTall);
    int preview_wide = static_cast<int>(preview_tall * aspect);

    // movement buttons on the left of the scene, presets of the active tab on the right;
    // the buttons are as wide as the widest of their captions
    int move_button_wide = 0;

    for (vgui2::Button* button : move_buttons_)
    {
        int caption_wide, caption_tall;
        button->GetContentSize(caption_wide, caption_tall);
        move_button_wide = std::max(move_button_wide, caption_wide + kButtonPadding);
    }

    int side_columns_wide = move_button_wide + kPresetsWide + 2 * kGap;
    if (preview_wide + side_columns_wide > content_wide)
    {
        preview_wide = content_wide - side_columns_wide;
        preview_tall = static_cast<int>(preview_wide / aspect);
    }

    int cluster_x = kMargin + (content_wide - preview_wide - side_columns_wide) / 2;

    int buttons_count = static_cast<int>(move_buttons_.size());
    int buttons_tall = buttons_count * kMoveButtonTall + (buttons_count - 1) * kMoveButtonGap;
    int buttons_y = kMargin + (preview_tall - buttons_tall) / 2;

    for (int i = 0; i < buttons_count; i++)
    {
        move_buttons_[i]->SetBounds(
            cluster_x,
            buttons_y + i * (kMoveButtonTall + kMoveButtonGap),
            move_button_wide,
            kMoveButtonTall);
    }

    int preview_x = cluster_x + move_button_wide + kGap;
    preview_->SetBounds(preview_x, kMargin, preview_wide, preview_tall);

    int presets_x = preview_x + preview_wide + kGap;
    int presets_count = 0;

    for (vgui2::Button* button : preset_buttons_)
    {
        if (button->IsVisible())
            presets_count++;
    }

    int caption_wide, presets_caption_tall;
    presets_caption_->GetContentSize(caption_wide, presets_caption_tall);

    int presets_tall = presets_caption_tall + presets_count * kPresetTall + std::max(0, presets_count - 1) * kPresetGap;
    int presets_y = kMargin + (preview_tall - presets_tall) / 2;

    presets_caption_->SetBounds(presets_x, presets_y, kPresetsWide, presets_caption_tall);

    int preset_y = presets_y + presets_caption_tall;

    for (vgui2::Button* button : preset_buttons_)
    {
        if (!button->IsVisible())
            continue;

        button->SetBounds(presets_x, preset_y, kPresetsWide, kPresetTall);
        preset_y += kPresetTall + kPresetGap;
    }

    int defaults_y = tall - kMargin - kDefaultsTall;
    int tabs_y = kMargin + preview_tall + kGap;

    tabs_->SetBounds(kMargin, tabs_y, content_wide, defaults_y - kGap - tabs_y);

    int defaults_wide, defaults_caption_tall;
    defaults_button_->GetContentSize(defaults_wide, defaults_caption_tall);
    defaults_button_->SetBounds(kMargin, defaults_y, defaults_wide + kButtonPadding, kDefaultsTall);
}

// The page is not rebuilt when the video mode changes under it, and the preview is shaped
// from that mode.
void COptionsSubGame::OnScreenSizeChanged(int old_wide, int old_tall)
{
    BaseClass::OnScreenSizeChanged(old_wide, old_tall);

    InvalidateLayout();
}

void COptionsSubGame::ShowCrosshairTab()
{
    tabs_->SetActivePage(crosshair_page_);
}

void COptionsSubGame::OnResetData()
{
    for (const SliderRow& row : rows_)
    {
        row.slider->Reset();
        UpdateTextFromSlider(row);
    }

    for (CCvarToggleCheckButton* check : checks_)
    {
        check->Reset();
    }

    InitCrosshairTypeItems();
    InitCrosshairSizeItems();
    LoadCrosshairColor();

    int style = std::clamp(static_cast<int>(engine->pfnGetCvarFloat(cvars::kBobStyle.name)), view_bob::kStyleClassic, view_bob::kStyleModern);
    style_combo_->ActivateItemByRow(style);

    int lag_style = std::clamp(static_cast<int>(engine->pfnGetCvarFloat(cvars::kViewmodelLagStyle.name)), 0, 2);
    lag_style_combo_->ActivateItemByRow(lag_style);

    saved_crosshair_color_ = crosshair_color_;
    saved_style_ = style;
    saved_lag_style_ = lag_style;

    SetPreviewMove(PreviewMove::kRun);

    UpdateControlStates();
    SyncPreview();
}

void COptionsSubGame::OnApplyChanges()
{
    for (const SliderRow& row : rows_)
    {
        row.slider->ApplyChanges();
    }

    for (CCvarToggleCheckButton* check : checks_)
    {
        check->ApplyChanges();
    }

    crosshair_type_combo_->ApplyChanges();
    crosshair_size_combo_->ApplyChanges();

    // Written only where the page moved them: the dialog applies every page whenever any
    // tab is accepted, and the controls that own a cvar leave it alone until it changes.
    if (crosshair_color_ != saved_crosshair_color_)
    {
        char command[128];
        Q_snprintf(command, sizeof(command), "%s \"%d %d %d\"\n",
            cvars::kCrosshairColor.name, crosshair_color_.r, crosshair_color_.g, crosshair_color_.b);
        engine->pfnClientCmd(command);

        saved_crosshair_color_ = crosshair_color_;
    }

    char value[8];

    if (GetSelectedStyle() != saved_style_)
    {
        saved_style_ = GetSelectedStyle();

        Q_snprintf(value, sizeof(value), "%d", saved_style_);
        engine->Cvar_Set(cvars::kBobStyle.name, value);
    }

    if (GetSelectedLagStyle() != saved_lag_style_)
    {
        saved_lag_style_ = GetSelectedLagStyle();

        Q_snprintf(value, sizeof(value), "%d", saved_lag_style_);
        engine->Cvar_Set(cvars::kViewmodelLagStyle.name, value);
    }
}

void COptionsSubGame::OnPageShow()
{
    BaseClass::OnPageShow();

    // The page stops thinking while another options tab is up, so the tab it comes back
    // to has to look new again for its movement and demo to be picked.
    last_active_page_ = nullptr;
}

void COptionsSubGame::OnThink()
{
    BaseClass::OnThink();

    Panel* active_page = tabs_->GetActivePage();
    const Tab* tab = FindTab(active_page);

    if (active_page != last_active_page_)
    {
        last_active_page_ = active_page;

        SetPreviewMove(tab ? tab->move : PreviewMove::kIdle);
        UpdatePresetButtons();
    }

    preview_->SetDemo(tab ? tab->demo : PreviewDemo::kNone);
}

void COptionsSubGame::OnCommand(const char* command)
{
    if (!stricmp(command, "PickCrosshairColor"))
    {
        ncl_math::ColorF current = crosshair_color_.ToFloat();
        ncl_math::ColorF defaults_color = DefaultCrosshairColor().ToFloat();

        float rgba[4] = {current.r, current.g, current.b, 1.0f};
        float defaults[4] = {defaults_color.r, defaults_color.g, defaults_color.b, 1.0f};

        CColorPickerDialog* picker = new CColorPickerDialog(this, this, kCrosshairColorParam, rgba, defaults, false);
        picker->Activate();

        // opening over the scene would hide the crosshair being colored, so the window
        // goes under the preview
        int preview_x = 0;
        int preview_y = 0;
        preview_->LocalToScreen(preview_x, preview_y);

        int preview_wide = 0;
        int preview_tall = 0;
        preview_->GetSize(preview_wide, preview_tall);

        int picker_wide = 0;
        int picker_tall = 0;
        picker->GetSize(picker_wide, picker_tall);

        int workspace_x = 0;
        int workspace_y = 0;
        int workspace_wide = 0;
        int workspace_tall = 0;
        vgui2::surface()->GetWorkspaceBounds(workspace_x, workspace_y, workspace_wide, workspace_tall);

        int picker_y = std::min(
            preview_y + preview_tall + kGap,
            workspace_y + workspace_tall - picker_tall - kMargin);

        picker->SetPos(workspace_x + (workspace_wide - picker_wide) / 2, std::max(workspace_y, picker_y));
        return;
    }

    if (!stricmp(command, "Defaults"))
    {
        SetDefaults();
        return;
    }

    for (const MoveButtonDesc& desc : kMoveButtons)
    {
        if (!stricmp(command, desc.command))
        {
            SetPreviewMove(desc.move);
            return;
        }
    }

    int preset = 0;
    if (sscanf(command, kPresetCommandFormat, &preset) == 1)
    {
        ApplyPreset(preset);
        return;
    }

    BaseClass::OnCommand(command);
}

void COptionsSubGame::SetPreviewMove(PreviewMove move)
{
    preview_move_ = move;

    for (int i = 0; i < static_cast<int>(move_buttons_.size()); i++)
    {
        move_buttons_[i]->ForceDepressed(i == static_cast<int>(move));
    }

    preview_->SetMoveMode(move);
}

void COptionsSubGame::OnCheckButtonChecked(Panel* panel)
{
    SyncPreview();
    NotifyDataChanged();
}

void COptionsSubGame::OnControlModified(Panel* panel)
{
    for (const SliderRow& row : rows_)
    {
        if (panel == row.slider)
        {
            UpdateTextFromSlider(row);
            break;
        }
    }

    SyncPreview();
    NotifyDataChanged();
}

void COptionsSubGame::OnTextChanged(Panel* panel)
{
    if (panel == style_combo_ || panel == lag_style_combo_)
    {
        UpdateControlStates();
        SyncPreview();
        NotifyDataChanged();
        return;
    }

    if (panel == crosshair_type_combo_ || panel == crosshair_size_combo_)
    {
        SyncPreview();
        NotifyDataChanged();
        return;
    }

    for (const SliderRow& row : rows_)
    {
        if (panel == row.text)
        {
            UpdateSliderFromText(row);
            SyncPreview();
            NotifyDataChanged();
            return;
        }
    }
}

const COptionsSubGame::Tab* COptionsSubGame::FindTab(vgui2::Panel* page) const
{
    for (const Tab& tab : tab_table_)
    {
        if (tab.page == page)
            return &tab;
    }

    return nullptr;
}

void COptionsSubGame::SetDefaults()
{
    Panel* active_page = tabs_->GetActivePage();

    for (const SliderRow& row : rows_)
    {
        if (row.slider->GetParent() == active_page)
            ResetSlider(row.slider);
    }

    for (CCvarToggleCheckButton* check : checks_)
    {
        if (check->GetParent() == active_page)
            check->ResetToDefaultValue();
    }

    const Tab* tab = FindTab(active_page);
    if (tab && tab->reset_defaults)
        tab->reset_defaults();

    UpdateControlStates();
    SyncPreview();
    NotifyDataChanged();
}

void COptionsSubGame::ResetCrosshairToDefaults()
{
    crosshair_type_combo_->ActivateItem(std::clamp(atoi(cvars::kCrosshairType.value), 0, crosshair::kTypeCount - 1));
    crosshair_size_combo_->ActivateItem(crosshair::SizeIndex(cvars::kCrosshairSize.value));
    SetCrosshairColor(DefaultCrosshairColor());
    crosshair_translucent_check_->ResetToDefaultValue();
    crosshair_dynamic_check_->ResetToDefaultValue();
}

void COptionsSubGame::SetSliderValue(CCvarSlider* slider, float value)
{
    slider->SetSliderValue(value);

    for (const SliderRow& row : rows_)
    {
        if (row.slider == slider)
        {
            UpdateTextFromSlider(row);
            break;
        }
    }
}

void COptionsSubGame::ResetSlider(CCvarSlider* slider)
{
    if (!slider->ResetToDefaultValue())
        return;

    for (const SliderRow& row : rows_)
    {
        if (row.slider == slider)
        {
            UpdateTextFromSlider(row);
            break;
        }
    }
}

void COptionsSubGame::ApplyPreset(int index)
{
    const Tab* tab = FindTab(tabs_->GetActivePage());

    if (!tab || index < 0 || index >= static_cast<int>(tab->presets.size()))
        return;

    tab->presets[index].apply();

    UpdateControlStates();
    SyncPreview();
    NotifyDataChanged();
}

void COptionsSubGame::UpdatePresetButtons()
{
    const Tab* tab = FindTab(tabs_->GetActivePage());
    int count = tab ? static_cast<int>(tab->presets.size()) : 0;

    for (int i = 0; i < static_cast<int>(preset_buttons_.size()); i++)
    {
        bool used = i < count;

        preset_buttons_[i]->SetVisible(used);

        if (used)
            preset_buttons_[i]->SetText(tab->presets[i].label);
    }

    presets_caption_->SetVisible(count > 0);

    InvalidateLayout();
}

int COptionsSubGame::GetSelectedStyle() const
{
    KeyValues* style_data = style_combo_->GetActiveItemUserData();

    return style_data ? style_data->GetInt("style") : view_bob::kStyleClassic;
}

int COptionsSubGame::GetSelectedLagStyle() const
{
    KeyValues* style_data = lag_style_combo_->GetActiveItemUserData();

    return style_data ? style_data->GetInt("style") : 0;
}

void COptionsSubGame::UpdateControlStates()
{
    // the two bob styles use disjoint amplitude settings, and the tab only has room
    // for one set at a time, so the unused one is hidden rather than disabled
    bool modern = GetSelectedStyle() == view_bob::kStyleModern;

    bob_slider_->SetVisible(!modern);
    bob_text_->SetVisible(!modern);

    amt_vert_slider_->SetVisible(modern);
    amt_vert_text_->SetVisible(modern);
    amt_lat_slider_->SetVisible(modern);
    amt_lat_text_->SetVisible(modern);
    lower_slider_->SetVisible(modern);
    lower_text_->SetVisible(modern);

    bobbing_page_->InvalidateLayout();

    // the tabs are sized from what their pages need
    InvalidateLayout();

    camera_check_->SetEnabled(!modern);

    int lag_style = GetSelectedLagStyle();

    lag_scale_slider_->SetEnabled(lag_style != 0);
    lag_scale_text_->SetEnabled(lag_style != 0);
    lag_speed_slider_->SetEnabled(lag_style == 1);
    lag_speed_text_->SetEnabled(lag_style == 1);
}

void COptionsSubGame::NotifyDataChanged()
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsSubGame::SyncPreview()
{
    view_bob::BobParams bob_params;
    bob_params.style = GetSelectedStyle();
    bob_params.bob = bob_slider_->GetSliderValue();
    bob_params.bob_cycle = cycle_slider_->GetSliderValue();
    bob_params.bob_up = up_slider_->GetSliderValue();
    bob_params.amt_vert = amt_vert_slider_->GetSliderValue();
    bob_params.amt_lat = amt_lat_slider_->GetSliderValue();
    bob_params.lower_amt = lower_slider_->GetSliderValue();
    bob_params.camera_bob = camera_check_->IsSelected();

    preview_->SetBobParams(bob_params);

    ViewTuningParams tuning;
    tuning.offset_x = offset_x_slider_->GetSliderValue();
    tuning.offset_y = offset_y_slider_->GetSliderValue();
    tuning.offset_z = offset_z_slider_->GetSliderValue();
    tuning.disable_shift = disable_shift_check_->IsSelected();
    tuning.viewmodel_fov = viewmodel_fov_slider_->GetSliderValue();
    tuning.lag_style = GetSelectedLagStyle();
    tuning.lag_scale = lag_scale_slider_->GetSliderValue();
    tuning.lag_speed = lag_speed_slider_->GetSliderValue();
    tuning.roll_angle = roll_angle_slider_->GetSliderValue();
    tuning.roll_speed = roll_speed_slider_->GetSliderValue();
    tuning.camera_move_scale = camera_move_scale_slider_->GetSliderValue();
    tuning.camera_move_interp = camera_move_interp_slider_->GetSliderValue();

    preview_->SetViewTuning(tuning);

    CrosshairParams crosshair;
    crosshair.type = crosshair_type_combo_->GetActiveItem();
    crosshair.color = crosshair_color_;
    crosshair.size_index = std::clamp(crosshair_size_combo_->GetActiveItem(), 0, crosshair::kSizeCount - 1);
    crosshair.translucent = crosshair_translucent_check_->IsSelected();
    crosshair.dynamic = crosshair_dynamic_check_->IsSelected();

    preview_->SetCrosshairParams(crosshair);
}

void COptionsSubGame::InitCrosshairTypeItems()
{
    crosshair_type_combo_->Reset();
    crosshair_type_combo_->DeleteAllItems();

    crosshair_type_combo_->AddItem("#GameUI_Crosshair_Cross", "cl_crosshair_type 0");
    crosshair_type_combo_->AddItem("#GameUI_Crosshair_TShape", "cl_crosshair_type 1");
    crosshair_type_combo_->AddItem("#GameUI_Crosshair_Circle", "cl_crosshair_type 2");
    crosshair_type_combo_->AddItem("#GameUI_Crosshair_Dot", "cl_crosshair_type 3");

    int type = static_cast<int>(engine->pfnGetCvarFloat(cvars::kCrosshairType.name));
    crosshair_type_combo_->SetInitialItem(std::clamp(type, 0, crosshair::kTypeCount - 1));
}

void COptionsSubGame::InitCrosshairSizeItems()
{
    crosshair_size_combo_->Reset();
    crosshair_size_combo_->DeleteAllItems();

    for (int i = 0; i < crosshair::kSizeCount; i++)
    {
        char command[64];
        Q_snprintf(command, sizeof(command), "%s %s", cvars::kCrosshairSize.name, crosshair::kSizes[i].name);
        crosshair_size_combo_->AddItem(kCrosshairSizeLabels[i], command);
    }

    crosshair_size_combo_->SetInitialItem(crosshair::SizeIndex(engine->pfnGetCvarString(cvars::kCrosshairSize.name)));
}

void COptionsSubGame::LoadCrosshairColor()
{
    SetCrosshairColor(ParseColor(engine->pfnGetCvarString(cvars::kCrosshairColor.name), DefaultCrosshairColor()));
}

void COptionsSubGame::SetCrosshairColor(const ncl_math::Color& color)
{
    crosshair_color_ = color;
    crosshair_color_swatch_->SetSolidColor(color);
}

void COptionsSubGame::OnColorPicked(const char* param, const float rgba[4])
{
    if (stricmp(param, kCrosshairColorParam) != 0)
        return;

    SetCrosshairColor(ncl_math::ColorF{rgba[0], rgba[1], rgba[2]}.ToBytes());

    SyncPreview();
    NotifyDataChanged();
}

void COptionsSubGame::UpdateTextFromSlider(const SliderRow& row)
{
    char buf[64];
    Q_snprintf(buf, sizeof(buf), row.format, row.slider->GetSliderValue());
    row.text->SetText(buf);
}

void COptionsSubGame::UpdateSliderFromText(const SliderRow& row)
{
    char buf[64];
    row.text->GetText(buf, sizeof(buf));

    row.slider->SetSliderValue(static_cast<float>(atof(buf)));
}
