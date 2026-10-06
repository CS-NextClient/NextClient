#include "OptionsDialog.h"

#include <GameUi.h>

#include "vgui_controls/PropertySheet.h"


#include "OptionsSubMultiplayer.h"
#include "OptionsSubGame.h"
#if NEXTCLIENT_WITH_PLUGINS
#include "PluginLocalization.h"
#include "PluginSettingsPage.h"
#include <nextclient/runtime.h>
#include <vgui_controls/MessageBox.h>
#endif
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubAudio.h"
#include "OptionsSubVoice.h"
#include "OptionsSubVideo.h"
#include "OptionsSubMiscellaneous.h"

#include "ModInfo.h"

#include "KeyValues.h"

#undef PostMessage

COptionsDialog::COptionsDialog(vgui2::Panel* parent) :
    COverflowPropertyDialog(parent, "OptionsDialog")
{
    SetBounds(0, 0, 599, 466);
    SetSizeable(false);
    SetTitle("#GameUI_Options", true);

    m_pOptionsSubMultiplayer = NULL;
    m_pOptionsSubGame = NULL;
    m_pOptionsSubKeyboard = NULL;
    m_pOptionsSubMouse = NULL;
    m_pOptionsSubAudio = NULL;
    m_pOptionsSubVideo = NULL;
    m_pOptionsSubVoice = NULL;
    m_pOptionsSubMiscellaneous = NULL;

    if ((ModInfo().IsMultiplayerOnly() && !ModInfo().IsSinglePlayerOnly()) ||
        (!ModInfo().IsMultiplayerOnly() && !ModInfo().IsSinglePlayerOnly()))
        m_pOptionsSubMultiplayer = new COptionsSubMultiplayer(this);

    m_pOptionsSubGame = new COptionsSubGame(this);
    m_pOptionsSubKeyboard = new COptionsSubKeyboard(this);
    m_pOptionsSubMouse = new COptionsSubMouse(this);
    m_pOptionsSubAudio = new COptionsSubAudio(this);
    m_pOptionsSubVideo = new COptionsSubVideo(this);

    if (!ModInfo().IsSinglePlayerOnly())
    {
        m_pOptionsSubVoice = new COptionsSubVoice(this);
    }

    m_pOptionsSubMiscellaneous = new OptionsSubMiscellaneous(this);

#if NEXTCLIENT_WITH_PLUGINS
    auto plugins = PluginSettings_Parse(nc_runtime_ui());
    m_pluginSettings = PluginSettingsSnapshot(plugins);
    auto controlsFor = [&](const std::string& tab, const std::string& owner) {
        tao::json::value controls = tao::json::empty_array;
        for (const auto& p : plugins.get_array())
            if (owner.empty() || p.at("id") == owner)
                for (auto c : p.at("controls").get_array())
                    if (c.at("tab") == tab)
                    {
                        c["owner"] = p.at("id");
                        controls.push_back(std::move(c));
                    }
        return controls;
    };
    bool hasExtendedBuiltin = false;
#endif
    auto builtin = [&](const char* id, vgui2::PropertyPage* page, const char* title) {
        if (!page)
            return;
#if NEXTCLIENT_WITH_PLUGINS
        auto controls = controlsFor(id, "");
        if (!controls.get_array().empty())
        {
            auto* extended = new CPluginSettingsPage(this, controls, m_pluginSettings, page);
            m_pluginPages.push_back(extended);
            page = extended;
            hasExtendedBuiltin = true;
        }
#endif
        AddPage(page, title);
        m_tabNames.Insert(id, page);
    };
    builtin("multiplayer", m_pOptionsSubMultiplayer, "#GameUI_Multiplayer");
    builtin("game", m_pOptionsSubGame, "#GameUI_Game");
#if NEXTCLIENT_WITH_PLUGINS
    // Plugin-created tabs sit next to the built-in top-level settings tabs.
    for (const auto& p : plugins.get_array())
        for (const auto& t : p.at("tabs").get_array())
        {
            auto id = p.at("id").get_string() + "." + t.at("id").get_string();
            auto* page = new CPluginSettingsPage(this, controlsFor(t.at("id").get_string(), p.at("id").get_string()), m_pluginSettings);
            m_pluginPages.push_back(page);
            AddPage(page, PluginTitle(id, PluginLocalized(t)).c_str());
            m_tabNames.Insert(id.c_str(), page);
        }
#endif
    builtin("keyboard", m_pOptionsSubKeyboard, "#GameUI_Keyboard");
    builtin("mouse", m_pOptionsSubMouse, "#GameUI_Mouse");
    builtin("audio", m_pOptionsSubAudio, "#GameUI_Audio");
    builtin("video", m_pOptionsSubVideo, "#GameUI_Video");
    builtin("voice", m_pOptionsSubVoice, "#GameUI_Voice");
    builtin("miscellaneous", m_pOptionsSubMiscellaneous, "#GameUI_Miscellaneous");
#if NEXTCLIENT_WITH_PLUGINS
    if (hasExtendedBuiltin)
        SetTall(GetTall() + 28);
#endif

    SetApplyButtonVisible(true);
    GetPropertySheet()->SetTabWidth(84);
    InvalidateLayout(true);
    FitPageNavigation();
}

COptionsDialog::~COptionsDialog(void) {}

bool COptionsDialog::OnOK(bool applyOnly)
{
#if NEXTCLIENT_WITH_PLUGINS
    tao::json::value values = tao::json::empty_array;
    const PluginSettingsSnapshot current(PluginSettings_Parse(nc_runtime_ui()));
    for (auto* page : m_pluginPages)
        page->Collect(values, current);
    if (!values.get_array().empty())
    {
        const char* error = nc_runtime_settings(tao::json::to_string(values).c_str());
        if (*error)
        {
            auto* box = new vgui2::MessageBox(PluginToken("#NextPlugins_Title").c_str(), PluginWide(PluginDiagnostic(error)).c_str(), this);
            box->DoModal();
            return false;
        }
    }
    m_pluginSettings = PluginSettingsSnapshot(PluginSettings_Parse(nc_runtime_ui()));
#endif
    const bool applied = BaseClass::OnOK(applyOnly);
    if (applied)
        FitPageNavigation();
    return applied;
}

void COptionsDialog::ResetAllData()
{
#if NEXTCLIENT_WITH_PLUGINS
    m_pluginSettings = PluginSettingsSnapshot(PluginSettings_Parse(nc_runtime_ui()));
#endif
    BaseClass::ResetAllData();
}

void COptionsDialog::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (!GameUI().IsInLevel() && code == vgui2::KEY_ESCAPE)
    {
        Close();
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}

void COptionsDialog::Activate(void)
{
    bool was_visible = IsVisible();

    if (!was_visible)
        FitPageNavigation();
    BaseClass::Activate();

    if (!was_visible)
    {
        if (m_pOptionsSubMultiplayer)
        {
            OpenTab("multiplayer");
        }
        else
        {
            OpenTab("keyboard");
        }
        ResetAllData();
        EnableApplyButton(false);
    }
}

void COptionsDialog::OpenTab(const char* tabName)
{
    int index = m_tabNames.Find(tabName);

    if (index != m_tabNames.InvalidIndex())
    {
        auto page = m_tabNames[index];
        if (GetActivePage() != page)
            GetPropertySheet()->SetActivePage(page);
    }
}

void COptionsDialog::OpenCrosshairSettings()
{
    OpenTab("game");

    if (m_pOptionsSubGame)
    {
        // Reveal every enclosing sheet, including the Standard section of an
        // extended built-in page, before selecting its Crosshair subpage.
        for (vgui2::Panel* page = m_pOptionsSubGame; page->GetParent(); page = page->GetParent())
            if (auto* sheet = dynamic_cast<vgui2::PropertySheet*>(page->GetParent()))
                sheet->SetActivePage(page);
        m_pOptionsSubGame->ShowCrosshairTab();
    }
}

void COptionsDialog::OnClose(void)
{
    BaseClass::OnClose();
}

void COptionsDialog::OnGameUIHidden(void)
{
    for (int i = 0; i < GetChildCount(); i++)
    {
        Panel* pChild = GetChild(i);

        if (pChild)
            PostMessage(pChild, new KeyValues("GameUIHidden"));
    }
}
