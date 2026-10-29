#include "GameUi/Controls/DeveloperToolsPanel.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include <next_engine_mini/perf_cvars.h>
#include <tier1/strtools.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>

using namespace vgui2;

namespace
{
    struct MonitorButton
    {
        const cvars::CvarDefinition& cvar;
        const char* caption;
        const char* modes[4];
    };

    constexpr MonitorButton kMonitorButtons[] = {
        {performance::kPerfCvarDefs[0],
         "#GameUI_DevTools_Performance",
         {"#GameUI_DevTools_Off", "#GameUI_DevTools_Numbers", "#GameUI_DevTools_Grouped", "#GameUI_DevTools_Plots"}},
        {performance::kPerfCvarDefs[3], "#GameUI_DevTools_Network", {"#GameUI_DevTools_Off", "#GameUI_DevTools_On"}},
        {performance::kPerfCvarDefs[5],
         "#GameUI_DevTools_Batching",
         {"#GameUI_DevTools_Off", "#GameUI_DevTools_Summary", "#GameUI_DevTools_Details"}},
        {performance::kPerfCvarDefs[4],
         "#GameUI_DevTools_Map",
         {"#GameUI_DevTools_Off", "#GameUI_DevTools_Counters", "#GameUI_DevTools_Objects"}}
    };

    class DeveloperButton : public Button
    {
        DECLARE_CLASS_SIMPLE(DeveloperButton, Button);

    public:
        DeveloperButton(Panel* parent, const char* name, const char* caption, Panel* receiver, const char* command) :
            BaseClass(parent, name, caption, receiver, command)
        {}

        void ApplySchemeSettings(IScheme* scheme) override
        {
            BaseClass::ApplySchemeSettings(scheme);
            Color text = GetSchemeColor("DeveloperTools.TextColor", Color(255, 255, 255, 255), scheme);
            Color normal = GetSchemeColor("DeveloperTools.ButtonBgColor", Color(60, 60, 60, 255), scheme);
            Color armed = GetSchemeColor("DeveloperTools.ArmedBgColor", Color(90, 90, 90, 255), scheme);
            Color pressed = GetSchemeColor("DeveloperTools.DepressedBgColor", Color(40, 40, 40, 255), scheme);
            SetDefaultColor(text, normal);
            SetArmedColor(text, armed);
            SetDepressedColor(text, pressed);
            SetSelectedColor(text, pressed);
            SetPaintBackgroundEnabled(true);
            SetPaintBackgroundType(0);
        }
    };

    int ReadMode(goldsrc::cl_enginefunc_t& engine, const MonitorButton& definition)
    {
        float value = engine.pfnGetCvarFloat(definition.cvar.name);
        return std::isfinite(value) ? static_cast<int>(std::clamp(value, definition.cvar.min_value, definition.cvar.max_value)) : 0;
    }

    void SetCaption(ILocalize& localize, Label* label, const char* token, const wchar_t* value)
    {
        wchar_t text[256];
        localize.ConstructString(text, sizeof(text), localize.Find(token), 1, value);
        label->SetText(text);
    }
} // namespace

CDeveloperToolsPanel::CDeveloperToolsPanel(Panel* parent, goldsrc::cl_enginefunc_t& engine, ILocalize& localize) :
    BaseClass(parent, "DeveloperTools"),
    engine_(engine),
    localize_(localize)
{
    SetProportional(false);
    SetZPos(100);
    SetVisible(false);
    indicator_ = new Label(this, "DeveloperMode", "");

    editor_ = new DeveloperButton(this, "MaterialEditor", "#GameUI_MaterialEditor", parent, "OpenMaterialEditor");

    for (size_t index = 0; index < monitors_.size(); index++)
    {
        const MonitorButton& definition = kMonitorButtons[index];
        monitors_[index] = new DeveloperButton(this, definition.cvar.name, "", this, definition.cvar.name);
    }
}

void CDeveloperToolsPanel::Refresh(bool menu_available, bool authoring_available)
{
    float developer = engine_.pfnGetCvarFloat("developer");
    SetVisible(menu_available && std::isfinite(developer) && developer != 0.0f);

    if (!IsVisible())
    {
        return;
    }

    if (refresh_text_ || developer_value_ != developer)
    {
        wchar_t value[32];
        V_snwprintf(value, std::size(value), L"%g", developer);
        SetCaption(localize_, indicator_, "#GameUI_DevTools_DeveloperMode", value);
        developer_value_ = developer;
    }

    editor_->SetEnabled(authoring_available);

    for (size_t index = 0; index < monitors_.size(); index++)
    {
        const MonitorButton& definition = kMonitorButtons[index];
        int mode = ReadMode(engine_, definition);
        monitors_[index]->SetEnabled(engine_.pfnGetCvarPointer(definition.cvar.name) != nullptr);

        if (refresh_text_ || modes_[index] != mode)
        {
            SetCaption(localize_, monitors_[index], definition.caption, localize_.Find(definition.modes[mode]));
            modes_[index] = mode;
        }
    }

    refresh_text_ = false;
}

void CDeveloperToolsPanel::PerformLayout()
{
    BaseClass::PerformLayout();
    int inset = 6;
    int row_height = 24;
    int gap = 4;
    indicator_->SetBounds(inset, inset, GetWide() - inset * 2, row_height);
    editor_->SetBounds(inset, inset + row_height + gap, GetWide() - inset * 2, row_height);

    for (size_t index = 0; index < monitors_.size(); index++)
    {
        monitors_[index]->SetBounds(inset, inset + (static_cast<int>(index) + 2) * (row_height + gap), GetWide() - inset * 2, row_height);
    }
}

void CDeveloperToolsPanel::ApplySchemeSettings(IScheme* scheme)
{
    BaseClass::ApplySchemeSettings(scheme);
    SetBgColor(GetSchemeColor("DeveloperTools.BgColor", Color(32, 32, 32, 255), scheme));
    refresh_text_ = true;
}

void CDeveloperToolsPanel::OnCommand(const char* command)
{
    for (const MonitorButton& definition : kMonitorButtons)
    {
        if (V_stricmp(command, definition.cvar.name) == 0)
        {
            int mode = ReadMode(engine_, definition);
            int next = mode < static_cast<int>(definition.cvar.max_value) ? mode + 1 : 0;
            engine_.Cvar_SetValue(definition.cvar.name, static_cast<float>(next));
            return;
        }
    }

    BaseClass::OnCommand(command);
}
