#include "HudRadarDialog.h"
#include "../GameUi.h"

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/QueryBox.h>
#include <KeyValues.h>
#include <next_client_mini/radar_cvars.h>

#include <algorithm>

using namespace vgui2;

namespace
{
    struct RadarDialogCvar
    {
        const char* field; // checkbox control name; nullptr for slider/combo cvars
        const char* cvar;
    };

    // Every cvar the dialog edits, in the order mirrored by m_snapshot. Drives the
    // checkbox creation, the snapshot capture/restore around live in-game editing
    // and the "Use Defaults" reset. Defaults and ranges come from the shared
    // definition table (radar_cvars.h) the client registers from.
    constexpr RadarDialogCvar kRadarCvars[CHudRadarDialog::kSnapshotCount] = {
        {nullptr, "hud_radar_mode"},
        {nullptr, "hud_radar_map_mode"},
        {nullptr, "hud_radar_scale"},
        {nullptr, "hud_radar_map_scale"},
        {nullptr, "hud_radar_opacity"},
        {"RadarRound", "hud_radar_round"},
        {"FullMap", "hud_radar_fullmap"},
        {"Rotation", "hud_radar_rotate"},
        {"Names", "hud_radar_names"},
        {"Health", "hud_radar_health"},
        {"Tracks", "hud_radar_tracks"},
        {"Objectives", "hud_radar_objectives"},
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
            if (RadarCvarDef_Get(def.cvar).name == nullptr)
            {
                return false;
            }
        }
        return true;
    }
    static_assert(AllKnown());

    constexpr RadarCvarDef kScaleCvar = RadarCvarDef_Get("hud_radar_scale");
    constexpr RadarCvarDef kMapScaleCvar = RadarCvarDef_Get("hud_radar_map_scale");
    constexpr RadarCvarDef kOpacityCvar = RadarCvarDef_Get("hud_radar_opacity");
    constexpr RadarCvarDef kMapModeCvar = RadarCvarDef_Get("hud_radar_map_mode");

    float DefaultValue(const char* cvar)
    {
        return (float)atof(RadarCvarDef_Get(cvar).default_value);
    }

    // Combo row (0-based) for a hud_radar_map_mode value, clamped to the valid range so
    // the dialog previews the same mode the radar renders (the client clamps identically).
    int MapModeRow(float value)
    {
        int mode = std::clamp((int)value, (int)kMapModeCvar.min_value, (int)kMapModeCvar.max_value);
        return mode - 1;
    }

    bool g_loading = false;
} // namespace

CHudRadarDialog::CHudRadarDialog(Panel* parent) :
    BaseClass(parent, "CHudRadarDialog")
{
    m_bLiveApply = false;
    SetBounds(0, 0, 380, 470);
    SetTitle("#GameUI_HudRadar_Title", true);
    MoveToCenterOfScreen();
    SetSizeable(false);
    SetMoveable(true);
    SetDeleteSelfOnClose(true);

    m_pApplyButton = new Button(this, "Apply", "Apply");

    m_pMode = new ComboBox(this, "RadarMode", 2, false);
    m_pMode->AddItem("#GameUI_HudRadar_Classic", nullptr);
    m_pMode->AddItem("#GameUI_HudRadar_Advanced", nullptr);
    m_pMode->AddActionSignalTarget(this);

    m_pMapMode = new ComboBox(this, "MapMode", 6, false);
    m_pMapMode->AddItem("#GameUI_HudRadar_MapMode_Overview", nullptr);
    m_pMapMode->AddItem("#GameUI_HudRadar_MapMode_OverviewFlat", nullptr);
    m_pMapMode->AddItem("#GameUI_HudRadar_MapMode_Generated", nullptr);
    m_pMapMode->AddItem("#GameUI_HudRadar_MapMode_GeneratedFlat", nullptr);
    m_pMapMode->AddItem("#GameUI_HudRadar_MapMode_Slice", nullptr);
    m_pMapMode->AddItem("#GameUI_HudRadar_MapMode_SliceFlat", nullptr);
    m_pMapMode->AddActionSignalTarget(this);

    m_pRadarScale = new CCvarSlider(this, "RadarScale", "", kScaleCvar.min_value, kScaleCvar.max_value, kScaleCvar.name);
    m_pMapScale = new CCvarSlider(this, "MapScale", "", kMapScaleCvar.min_value, kMapScaleCvar.max_value, kMapScaleCvar.name);
    m_pOpacity = new CCvarSlider(this, "Opacity", "", kOpacityCvar.min_value, kOpacityCvar.max_value, kOpacityCvar.name);
    m_pRadarScale->AddActionSignalTarget(this);
    m_pMapScale->AddActionSignalTarget(this);
    m_pOpacity->AddActionSignalTarget(this);

    m_pRadarScaleText = new TextEntry(this, "RadarScaleText");
    m_pMapScaleText = new TextEntry(this, "MapScaleText");
    m_pOpacityText = new TextEntry(this, "OpacityText");
    m_pRadarScaleText->SetEditable(false);
    m_pMapScaleText->SetEditable(false);
    m_pOpacityText->SetEditable(false);

    int check = 0;
    for (const RadarDialogCvar& def : kRadarCvars)
    {
        if (!def.field)
        {
            continue;
        }
        m_pChecks[check] = new CCvarToggleCheckButton(this, def.field, "", def.cvar);
        m_pChecks[check]->AddActionSignalTarget(this);
        check++;
    }

    LoadControlSettings("Resource\\HudRadarDialog.res");

    // Selection-only: the combo boxes must not accept typed text.
    m_pMode->SetEditable(false);
    m_pMapMode->SetEditable(false);

    // The .res tick captions ("0.50" etc.) are coerced to "%f" (0.500000) by KeyValues
    // float detection; set them explicitly as clean strings.
    char tick_lo[16], tick_hi[16];
    V_sprintf_safe(tick_lo, "%.1f", kScaleCvar.min_value);
    V_sprintf_safe(tick_hi, "%.1f", kScaleCvar.max_value);
    m_pRadarScale->SetTickCaptions(tick_lo, tick_hi);
    V_sprintf_safe(tick_lo, "%.1f", kMapScaleCvar.min_value);
    V_sprintf_safe(tick_hi, "%.1f", kMapScaleCvar.max_value);
    m_pMapScale->SetTickCaptions(tick_lo, tick_hi);

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

    // In a level the frame floats over the game: re-centre it after the reparent so it
    // does not cover the radar.
    if (m_bLiveApply)
    {
        MoveToCenterOfScreen();
    }
}

void CHudRadarDialog::OnClose()
{
    BaseClass::OnClose();
    input()->ReleaseAppModalSurface();

    // If we were shown over the running game, tear that down so the game - and, in
    // spectator, its letterbox and bottom menu - is restored. No-op otherwise.
    GameUI().EndPanelOverGame();
}

void CHudRadarDialog::LoadValues()
{
    g_loading = true;

    m_pMode->ActivateItemByRow(engine->pfnGetCvarFloat("hud_radar_mode") != 0.0f ? 1 : 0);

    m_pMapMode->ActivateItemByRow(MapModeRow(engine->pfnGetCvarFloat("hud_radar_map_mode")));

    m_pRadarScale->Reset();
    m_pMapScale->Reset();
    m_pOpacity->Reset();

    for (int i = 0; i < kCheckCount; i++)
    {
        m_pChecks[i]->Reset();
    }

    UpdateTexts();
    UpdateEnabledStates();

    g_loading = false;
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
}

void CHudRadarDialog::ApplyValues()
{
    m_pRadarScale->ApplyChanges();
    m_pMapScale->ApplyChanges();
    m_pOpacity->ApplyChanges();

    for (int i = 0; i < kCheckCount; i++)
    {
        m_pChecks[i]->ApplyChanges();
    }

    char cmd[64];
    Q_snprintf(cmd, sizeof(cmd), "hud_radar_mode %d\n", m_pMode->GetActiveItem() != 0 ? 1 : 0);
    engine->pfnClientCmd(cmd);

    Q_snprintf(cmd, sizeof(cmd), "hud_radar_map_mode %d\n", m_pMapMode->GetActiveItem() + 1);
    engine->pfnClientCmd(cmd);
}

void CHudRadarDialog::ResetToDefaults()
{
    // Preview the defaults in the controls without touching the cvars: pfnClientCmd is
    // buffered, so writing the cvars and reloading here would read the still-unchanged
    // values back and lose the defaults. They are committed the normal way on OK/Apply.
    g_loading = true;

    m_pMode->ActivateItemByRow(DefaultValue("hud_radar_mode") != 0.0f ? 1 : 0);

    m_pMapMode->ActivateItemByRow(MapModeRow(DefaultValue("hud_radar_map_mode")));

    m_pRadarScale->SetSliderValue(DefaultValue("hud_radar_scale"));
    m_pMapScale->SetSliderValue(DefaultValue("hud_radar_map_scale"));
    m_pOpacity->SetSliderValue(DefaultValue("hud_radar_opacity"));

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

    g_loading = false;

    // Enable Apply so the previewed defaults can be committed; in a level apply them
    // live like any other control change.
    MarkModified();
    if (m_bLiveApply)
    {
        ApplyValues();
    }
}

void CHudRadarDialog::MarkModified()
{
    if (!g_loading)
    {
        m_pApplyButton->SetEnabled(true);
    }
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

// Full-map view ignores rotation (see ComputeTransform), so grey out that control while
// it is enabled. Map scale still applies (it zooms the full-map view), so it stays live.
void CHudRadarDialog::UpdateEnabledStates()
{
    CCvarToggleCheckButton* fullmap = FindCheck("hud_radar_fullmap");
    bool full = fullmap && fullmap->IsSelected();

    if (CCvarToggleCheckButton* rotation = FindCheck("hud_radar_rotate"))
    {
        rotation->SetEnabled(!full);
    }
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

// In a level, edits apply to the radar live, so Esc must not silently discard them.
// Ask whether to keep (Apply) or revert to the snapshot (Discard) before closing.
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
            RestoreSnapshot(); // revert live edits made while in a level
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
    if (m_bLiveApply && !g_loading)
    {
        ApplyValues();
    }
}

void CHudRadarDialog::OnCheckButtonChecked(Panel*)
{
    UpdateEnabledStates();
    MarkModified();
    if (m_bLiveApply && !g_loading)
    {
        ApplyValues();
    }
}

void CHudRadarDialog::OnTextChanged(Panel*)
{
    MarkModified();
    if (m_bLiveApply && !g_loading)
    {
        ApplyValues();
    }
}
