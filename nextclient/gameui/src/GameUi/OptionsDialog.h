#pragma once

#include "Controls/OverflowPropertyDialog.h"
#include "vgui_controls/KeyRepeat.h"
#include "utldict.h"
#include <vector>
#if NEXTCLIENT_WITH_PLUGINS
#include "PluginSettingsState.h"
#endif

class COptionsDialog : public COverflowPropertyDialog
{
    DECLARE_CLASS_SIMPLE(COptionsDialog, COverflowPropertyDialog);

    CUtlDict<vgui2::PropertyPage *> m_tabNames;

public:
    COptionsDialog(vgui2::Panel *parent);
    ~COptionsDialog(void);

    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OpenTab(const char *tabName);
    void OpenCrosshairSettings();
    bool OnOK(bool applyOnly) override;
    void ResetAllData();

public:
    void Activate(void);

public:
    void OnClose(void);

public:
    MESSAGE_FUNC(OnGameUIHidden, "GameUIHidden");

private:
    class COptionsSubMultiplayer *m_pOptionsSubMultiplayer;
    class COptionsSubGame *m_pOptionsSubGame;
#if NEXTCLIENT_WITH_PLUGINS
    std::vector<class CPluginSettingsPage *> m_pluginPages;
    PluginSettingsSnapshot m_pluginSettings;
    class CPluginSettingsGroupsPage *m_pluginGroups{};
    void RefreshPluginAvailability(const PluginSettingsSnapshot &current);
    MESSAGE_FUNC(OnPluginSettingsRefresh, "PluginSettingsRefresh");
#endif
    class COptionsSubKeyboard *m_pOptionsSubKeyboard;
    class COptionsSubMouse *m_pOptionsSubMouse;
    class COptionsSubAudio *m_pOptionsSubAudio;
    class COptionsSubVideo *m_pOptionsSubVideo;
    class COptionsSubVoice *m_pOptionsSubVoice;
    class OptionsSubMiscellaneous *m_pOptionsSubMiscellaneous;
};
