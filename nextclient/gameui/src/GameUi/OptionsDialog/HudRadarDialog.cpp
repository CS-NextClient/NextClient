#include "HudRadarDialog.h"

#include <algorithm>
#include <iterator>

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/Tooltip.h>
#include <KeyValues.h>
#include <next_client_mini/radar_cvars.h>
#include <goldsrc/client_api.h>

#include "GameUi/Controls/cvar_context_menu.h"
#include "GameUi/GameUi.h"

using namespace vgui2;

namespace
{
    class RadarCvarComboBox : public ComboBox
    {
        DECLARE_CLASS_SIMPLE(RadarCvarComboBox, ComboBox);

    private:
        const char* cvar_{};
        int default_row_{};
        Menu* context_menu_{};

    public:
        RadarCvarComboBox(
            Panel* parent,
            const char* name,
            int rows,
            const char* cvar,
            int default_row
        ) :
            BaseClass(parent, name, rows, false),
            cvar_(cvar),
            default_row_(default_row)
        {
            // The arrow otherwise consumes right clicks before the combo box receives them.
            GetComboButton()->SetMouseInputEnabled(false);
        }

        void OnMousePressed(MouseCode code) override
        {
            if (code == MOUSE_RIGHT && IsEnabled() && IsCursorOver())
            {
                HideMenu();
                CvarContextMenu_Show(this, context_menu_, cvar_, true);

                return;
            }

            BaseClass::OnMousePressed(code);
        }

    private:
        MESSAGE_FUNC(OnResetToDefault, "ResetToDefault")
        {
            ActivateItemByRow(default_row_);
        }
    };

    class RadarSliderValueEntry : public TextEntry
    {
        DECLARE_CLASS_SIMPLE(RadarSliderValueEntry, TextEntry);

    private:
        CCvarSlider* slider_{};

    public:
        RadarSliderValueEntry(Panel* parent, const char* name, CCvarSlider* slider) :
            BaseClass(parent, name),
            slider_(slider)
        {
            SetEditable(false);
        }

        void OnMousePressed(MouseCode code) override
        {
            if (code == MOUSE_RIGHT && IsEnabled())
            {
                slider_->OnMousePressed(code);

                return;
            }

            BaseClass::OnMousePressed(code);
        }
    };

    struct RadarDialogCvar
    {
        const char* field; // checkbox control name; nullptr for slider/combo cvars
        const char* cvar;
    };

    // Order must mirror m_snapshot.
    constexpr RadarDialogCvar kRadarCvars[CHudRadarDialog::kSnapshotCount] = {
        {nullptr, "hud_radar_mode"},
        {nullptr, "hud_radar_map_mode"},
        {nullptr, "hud_radar_scale"},
        {nullptr, "hud_radar_map_scale"},
        {nullptr, "hud_radar_opacity"},
        {nullptr, "hud_radar_name_size"},
        {"RadarRound", "hud_radar_round"},
        {"FullMap", "hud_radar_fullmap"},
        {"MapSmoothing", "hud_radar_map_smoothing"},
        {"MapShift", "hud_radar_map_shift"},
        {"ShiftScoreboard", "hud_radar_shift_scoreboard"},
        {"Rotation", "hud_radar_rotate"},
        {"Names", "hud_radar_names"},
        {"Health", "hud_radar_health"},
        {"Tracks", "hud_radar_tracks"},
        {"BlurBackground", "hud_radar_bg"},
    };

    constexpr int CountChecks()
    {
        int count = 0;
        for (const RadarDialogCvar& def : kRadarCvars)
        {
            if (def.field)
            {
                count++;
            }
        }
        return count;
    }
    static_assert(CountChecks() == CHudRadarDialog::kCheckCount);

    constexpr bool AllKnown()
    {
        for (const RadarDialogCvar& def : kRadarCvars)
        {
            if (radar::FindCvar(def.cvar).name == nullptr)
            {
                return false;
            }
        }
        return true;
    }
    static_assert(AllKnown());

    constexpr cvars::CvarDefinition kScaleCvar = radar::FindCvar("hud_radar_scale");
    constexpr cvars::CvarDefinition kMapScaleCvar = radar::FindCvar("hud_radar_map_scale");
    constexpr cvars::CvarDefinition kOpacityCvar = radar::FindCvar("hud_radar_opacity");
    constexpr cvars::CvarDefinition kNameSizeCvar = radar::FindCvar("hud_radar_name_size");

    float DefaultValue(const char* cvar)
    {
        return (float)atof(radar::FindCvar(cvar).default_value);
    }

    int MapModeRow(float value)
    {
        RadarMapMode mode = RadarMapMode_FromCvar(value);
        auto item = std::find_if(std::begin(kRadarMapModeDefs), std::end(kRadarMapModeDefs), [mode](const RadarMapModeDef& def) {
            return def.mode == mode;
        });

        return static_cast<int>(item - std::begin(kRadarMapModeDefs));
    }

} // namespace

CHudRadarDialog::CHudRadarDialog(Panel* parent) :
    BaseClass(parent, "CHudRadarDialog")
{
    m_bLiveApply = false;
    SetBounds(0, 0, 380, 496);
    SetTitle("#GameUI_HudRadar_Title", true);
    SetSizeable(false);
    SetMoveable(true);
    SetDeleteSelfOnClose(true);

    m_pApplyButton = new Button(this, "Apply", "Apply");

    m_pMode = new RadarCvarComboBox(
        this,
        "RadarMode",
        2,
        "hud_radar_mode",
        DefaultValue("hud_radar_mode") != 0.0f ? 1 : 0
    );
    m_pMode->AddItem("#GameUI_HudRadar_Classic", nullptr);
    m_pMode->AddItem("#GameUI_HudRadar_Advanced", nullptr);
    m_pMode->AddActionSignalTarget(this);

    m_pMapMode = new RadarCvarComboBox(
        this,
        "MapMode",
        static_cast<int>(std::size(kRadarMapModeDefs)),
        "hud_radar_map_mode",
        MapModeRow(DefaultValue("hud_radar_map_mode"))
    );

    for (const RadarMapModeDef& def : kRadarMapModeDefs)
    {
        m_pMapMode->AddItem(def.label, nullptr);
    }

    m_pMapMode->AddActionSignalTarget(this);

    m_pRadarScale = new CCvarSlider(this, "RadarScale", "", kScaleCvar.min_value, kScaleCvar.max_value, kScaleCvar.name);
    m_pMapScale = new CCvarSlider(this, "MapScale", "", kMapScaleCvar.min_value, kMapScaleCvar.max_value, kMapScaleCvar.name);
    m_pOpacity = new CCvarSlider(this, "Opacity", "", kOpacityCvar.min_value, kOpacityCvar.max_value, kOpacityCvar.name);
    m_pNameSize = new CCvarSlider(this, "NameSize", "", kNameSizeCvar.min_value, kNameSizeCvar.max_value, kNameSizeCvar.name);
    m_pRadarScale->SetDefaultValue(DefaultValue(kScaleCvar.name));
    m_pMapScale->SetDefaultValue(DefaultValue(kMapScaleCvar.name));
    m_pOpacity->SetDefaultValue(DefaultValue(kOpacityCvar.name));
    m_pNameSize->SetDefaultValue(DefaultValue(kNameSizeCvar.name));

    m_pRadarScale->AddActionSignalTarget(this);
    m_pMapScale->AddActionSignalTarget(this);
    m_pOpacity->AddActionSignalTarget(this);
    m_pNameSize->AddActionSignalTarget(this);

    m_pRadarScaleText = new RadarSliderValueEntry(this, "RadarScaleText", m_pRadarScale);
    m_pMapScaleText = new RadarSliderValueEntry(this, "MapScaleText", m_pMapScale);
    m_pOpacityText = new RadarSliderValueEntry(this, "OpacityText", m_pOpacity);
    m_pNameSizeText = new RadarSliderValueEntry(this, "NameSizeText", m_pNameSize);

    int check = 0;
    for (const RadarDialogCvar& def : kRadarCvars)
    {
        if (!def.field)
        {
            continue;
        }
        m_pChecks[check] = new CCvarToggleCheckButton(this, def.field, "", def.cvar);
        m_pChecks[check]->SetDefaultValue(DefaultValue(def.cvar) != 0.0f);
        m_pChecks[check]->AddActionSignalTarget(this);
        check++;
    }

    LoadControlSettings("Resource\\HudRadarDialog.res");
    FindCheck("hud_radar_map_shift")->GetTooltip()->SetText("#GameUI_HudRadar_MapShiftTooltip");

    m_pMode->SetEditable(false);
    m_pMapMode->SetEditable(false);

    // KeyValues float detection coerces the .res tick captions ("0.50") to "%f" (0.500000).
    char tick_lo[16], tick_hi[16];
    V_sprintf_safe(tick_lo, "%.1f", kScaleCvar.min_value);
    V_sprintf_safe(tick_hi, "%.1f", kScaleCvar.max_value);
    m_pRadarScale->SetTickCaptions(tick_lo, tick_hi);
    V_sprintf_safe(tick_lo, "%.2f", kMapScaleCvar.min_value);
    V_sprintf_safe(tick_hi, "%.2f", kMapScaleCvar.max_value);
    m_pMapScale->SetTickCaptions(tick_lo, tick_hi);
    V_sprintf_safe(tick_lo, "%.1f", kNameSizeCvar.min_value);
    V_sprintf_safe(tick_hi, "%.1f", kNameSizeCvar.max_value);
    m_pNameSize->SetTickCaptions(tick_lo, tick_hi);

    LoadValues();
    m_pApplyButton->SetEnabled(false);
}

void CHudRadarDialog::Activate()
{
    TakeSnapshot();
    const char* level = engine->pfnGetLevelName();
    m_bLiveApply = level && level[0];

    BaseClass::Activate();
    input()->SetAppModalSurface(GetVPanel());

    // Resource loading and reparenting must finish before positioning the dialog.
    MoveToCenterOfScreen();
}

void CHudRadarDialog::OnClose()
{
    input()->ReleaseAppModalSurface();
    GameUI().EndPanelOverGame();
    m_bLiveApply = false;
    BaseClass::OnClose();
}

// VGUI action signals are posted, so a flag cleared at the end of the call arrives too late.
// The combo boxes need their own silent setter: TextChanged is posted from a later message.
void CHudRadarDialog::SetControlsSilent(bool silent)
{
    m_pMode->SetSilentMode(silent);
    m_pMapMode->SetSilentMode(silent);

    m_pRadarScale->SetSilentMode(silent);
    m_pMapScale->SetSilentMode(silent);
    m_pOpacity->SetSilentMode(silent);
    m_pNameSize->SetSilentMode(silent);

    m_pRadarScaleText->SetSilentMode(silent);
    m_pMapScaleText->SetSilentMode(silent);
    m_pOpacityText->SetSilentMode(silent);
    m_pNameSizeText->SetSilentMode(silent);

    for (int i = 0; i < kCheckCount; i++)
    {
        m_pChecks[i]->SetSilentMode(silent);
    }
}

void CHudRadarDialog::LoadValues()
{
    SetControlsSilent(true);

    m_pMode->SilentActivateItemByRow(engine->pfnGetCvarFloat("hud_radar_mode") != 0.0f ? 1 : 0);

    m_pMapMode->SilentActivateItemByRow(MapModeRow(engine->pfnGetCvarFloat("hud_radar_map_mode")));

    m_pRadarScale->Reset();
    m_pMapScale->Reset();
    m_pOpacity->Reset();
    m_pNameSize->Reset();

    for (int i = 0; i < kCheckCount; i++)
    {
        m_pChecks[i]->Reset();
    }

    UpdateTexts();
    UpdateEnabledStates();

    SetControlsSilent(false);
}

void CHudRadarDialog::UpdateTexts()
{
    char buf[32];
    Q_snprintf(buf, sizeof(buf), " %.2f", m_pRadarScale->GetSliderValue());
    m_pRadarScaleText->SetText(buf);
    Q_snprintf(buf, sizeof(buf), " %.2f", m_pMapScale->GetSliderValue());
    m_pMapScaleText->SetText(buf);
    Q_snprintf(buf, sizeof(buf), " %.0f%%", m_pOpacity->GetSliderValue());
    m_pOpacityText->SetText(buf);
    Q_snprintf(buf, sizeof(buf), " %.2f", m_pNameSize->GetSliderValue());
    m_pNameSizeText->SetText(buf);
}

void CHudRadarDialog::ApplyValues()
{
    m_pRadarScale->ApplyChanges();
    m_pMapScale->ApplyChanges();
    m_pOpacity->ApplyChanges();
    m_pNameSize->ApplyChanges();

    for (int i = 0; i < kCheckCount; i++)
    {
        m_pChecks[i]->ApplyChanges();
    }

    char cmd[64];
    Q_snprintf(cmd, sizeof(cmd), "hud_radar_mode %d\n", m_pMode->GetActiveItem() != 0 ? 1 : 0);
    engine->pfnClientCmd(cmd);

    Q_snprintf(cmd, sizeof(cmd), "hud_radar_map_mode %d\n", static_cast<int>(kRadarMapModeDefs[m_pMapMode->GetActiveItem()].mode));
    engine->pfnClientCmd(cmd);
}

void CHudRadarDialog::ResetToDefaults()
{
    // Preview only: pfnClientCmd is buffered, so writing the cvars and reloading here would
    // read the still-unchanged values back. They are committed on OK/Apply.
    SetControlsSilent(true);

    m_pMode->SilentActivateItemByRow(DefaultValue("hud_radar_mode") != 0.0f ? 1 : 0);

    m_pMapMode->SilentActivateItemByRow(MapModeRow(DefaultValue("hud_radar_map_mode")));

    m_pRadarScale->SetSliderValue(DefaultValue("hud_radar_scale"));
    m_pMapScale->SetSliderValue(DefaultValue("hud_radar_map_scale"));
    m_pOpacity->SetSliderValue(DefaultValue("hud_radar_opacity"));
    m_pNameSize->SetSliderValue(DefaultValue("hud_radar_name_size"));

    int check = 0;
    for (const RadarDialogCvar& def : kRadarCvars)
    {
        if (!def.field)
        {
            continue;
        }
        m_pChecks[check]->SetSelected(DefaultValue(def.cvar) != 0.0f);
        check++;
    }

    UpdateTexts();
    UpdateEnabledStates();

    SetControlsSilent(false);

    MarkModified();
    if (m_bLiveApply)
    {
        ApplyValues();
    }
}

void CHudRadarDialog::MarkModified()
{
    m_pApplyButton->SetEnabled(true);
}

CCvarToggleCheckButton* CHudRadarDialog::FindCheck(const char* cvar) const
{
    int check = 0;
    for (const RadarDialogCvar& def : kRadarCvars)
    {
        if (!def.field)
        {
            continue;
        }
        if (!stricmp(def.cvar, cvar))
        {
            return m_pChecks[check];
        }
        check++;
    }
    return nullptr;
}

void CHudRadarDialog::UpdateEnabledStates()
{
    CCvarToggleCheckButton* names = FindCheck("hud_radar_names");
    bool named = names && names->IsSelected();
    m_pNameSize->SetEnabled(named);
    m_pNameSizeText->SetEnabled(named);

    RadarMapMode mode = kRadarMapModeDefs[m_pMapMode->GetActiveItem()].mode;
    FindCheck("hud_radar_map_smoothing")->SetEnabled(mode == RadarMapMode::SliceFlat || mode == RadarMapMode::SliceTextured);
}

void CHudRadarDialog::TakeSnapshot()
{
    for (int i = 0; i < kSnapshotCount; i++)
    {
        m_snapshot[i] = engine->pfnGetCvarFloat(kRadarCvars[i].cvar);
    }
}

void CHudRadarDialog::RestoreSnapshot()
{
    char cmd[64];
    for (int i = 0; i < kSnapshotCount; i++)
    {
        Q_snprintf(cmd, sizeof(cmd), "%s %g\n", kRadarCvars[i].cvar, m_snapshot[i]);
        engine->pfnClientCmd(cmd);
    }
}

bool CHudRadarDialog::HasChanges() const
{
    for (int i = 0; i < kSnapshotCount; i++)
    {
        if (engine->pfnGetCvarFloat(kRadarCvars[i].cvar) != m_snapshot[i])
        {
            return true;
        }
    }
    return false;
}

// In a level, edits are already applied live, so Esc must not silently discard them.
void CHudRadarDialog::ShowCloseConfirm()
{
    QueryBox* box = new QueryBox("#GameUI_HudRadar_Title", "#GameUI_HudRadar_ConfirmClose", this);
    box->SetOKButtonText("#GameUI_HudRadar_ApplyChanges");
    box->SetCancelButtonText("#GameUI_HudRadar_Discard");
    box->SetOKCommand(new KeyValues("Command", "command", "ConfirmApply"));
    box->SetCancelCommand(new KeyValues("Command", "command", "ConfirmDiscard"));
    box->AddActionSignalTarget(this);
    box->DoModal();
}

void CHudRadarDialog::OnCommand(const char* command)
{
    if (!stricmp(command, "Ok"))
    {
        ApplyValues();
        OnClose();
        return;
    }
    if (!stricmp(command, "Apply"))
    {
        ApplyValues();
        // Re-baseline: Close/Discard must revert only edits made after this commit.
        TakeSnapshot();
        m_pApplyButton->SetEnabled(false);
        return;
    }
    if (!stricmp(command, "UseDefaults"))
    {
        ResetToDefaults();
        return;
    }
    if (!stricmp(command, "Close"))
    {
        if (m_bLiveApply)
        {
            RestoreSnapshot();
        }
        Close();
        return;
    }
    if (!stricmp(command, "ConfirmApply"))
    {
        Close(); // live edits are already applied; keep them
        return;
    }
    if (!stricmp(command, "ConfirmDiscard"))
    {
        RestoreSnapshot();
        Close();
        return;
    }
    BaseClass::OnCommand(command);
}

void CHudRadarDialog::OnKeyCodeTyped(KeyCode code)
{
    if (code == KEY_ESCAPE)
    {
        if (m_bLiveApply && HasChanges())
        {
            ShowCloseConfirm();
        }
        else
        {
            Close();
        }
        return;
    }
    BaseClass::OnKeyCodeTyped(code);
}

void CHudRadarDialog::OnControlModified(Panel*)
{
    UpdateTexts();
    MarkModified();
    if (m_bLiveApply)
    {
        ApplyValues();
    }
}

void CHudRadarDialog::OnCheckButtonChecked(Panel*)
{
    UpdateEnabledStates();
    MarkModified();
    if (m_bLiveApply)
    {
        ApplyValues();
    }
}

void CHudRadarDialog::OnTextChanged(Panel*)
{
    UpdateEnabledStates();
    MarkModified();
    if (m_bLiveApply)
    {
        ApplyValues();
    }
}
