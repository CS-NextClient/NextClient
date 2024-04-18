#include "OptionsSubMiscellaneous.h"
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/MessageDialog.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <GameUi.h>
#include <steam/steam_api.h>
#include <nitro_utils/string_utils.h>
#include "../IServerBrowserEx.h"
#include "vgui_controls/ImagePanel.h"

OptionsSubMiscellaneous::OptionsSubMiscellaneous(vgui2::Panel *parent) :
    PropertyPage(parent, nullptr)
{
    miscellaneous_settings_ = GetSettings();

    color_scheme_ = new vgui2::ComboBox(this, "ColorScheme", 5, false);
    PrepareColorSchemesList();

    server_browser_init_tab_ = new vgui2::ComboBox(this, "ServerBrowserInitTab", 5, false);
    server_browser_init_tab_->AddItem("#ServerBrowser_InternetTab", nullptr);
    server_browser_init_tab_->AddItem("#ServerBrowser_FavoritesTab", nullptr);
    server_browser_init_tab_->AddItem("#ServerBrowser_UniqueTab", nullptr);
    server_browser_init_tab_->AddItem("#ServerBrowser_HistoryTab", nullptr);
    server_browser_init_tab_->AddItem("#ServerBrowser_LanTab", nullptr);
    int tab = miscellaneous_settings_->GetInt(OptionsSubMiscellaneous::kServerBrowserInitialTabKey, (int)ServerBrowserTab::Internet);
    tab = std::clamp(tab, (int)ServerBrowserTab::Internet, (int)ServerBrowserTab::LAN);
    server_browser_init_tab_->ActivateItemByRow(tab);

    disable_auto_open_server_browser_ = new vgui2::CheckButton(this, "DisableAutoOpenServerBrowser", "#GameUI_DisableAutoOpenServerBrowser");
    disable_auto_open_server_browser_->SetSelected(miscellaneous_settings_->GetBool(kDisableAutoOpenServerBrowserKey));

    LoadControlSettings("Resource\\OptionsSubMiscellaneous.res");
}

OptionsSubMiscellaneous::~OptionsSubMiscellaneous()
{
    if (miscellaneous_settings_)
    {
        miscellaneous_settings_->deleteThis();
        miscellaneous_settings_ = nullptr;
    }
}

void OptionsSubMiscellaneous::OnResetData()
{
    PrepareColorSchemesList();
}

void OptionsSubMiscellaneous::OnApplyChanges()
{
    miscellaneous_settings_->SetBool(kDisableAutoOpenServerBrowserKey, disable_auto_open_server_browser_->IsSelected());
    miscellaneous_settings_->SetInt(kServerBrowserInitialTabKey, server_browser_init_tab_->GetActiveItem());

    bool color_scheme_apllied = ApplyColorScheme();

    miscellaneous_settings_->SaveToFile(g_pFullFileSystem, kUserSaveDataPath, "GAMECONFIG");

    if (color_scheme_apllied)
    {
        engine->pfnClientCmd("fmod stop\n");
        engine->pfnClientCmd("_restart\n");
    }
}

void OptionsSubMiscellaneous::OnCheckButtonChecked(KeyValues* params)
{
    OnDataChanged();
}

void OptionsSubMiscellaneous::OnDataChanged()
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void OptionsSubMiscellaneous::OnTextChanged(Panel* panel, const char* text)
{
    if (panel == color_scheme_)
        OnDataChanged();
}

bool OptionsSubMiscellaneous::ApplyColorScheme() const
{
    KeyValues* scheme_data = color_scheme_->GetActiveItemUserData();
    if (!scheme_data)
        return false;

    const char* selected_scheme = scheme_data->GetString("Scheme");
    const char* saved_scheme = miscellaneous_settings_->GetString(kSchemeKey);

    if (*saved_scheme == '\0' && std::strcmp(selected_scheme, kDefaultScheme) == 0
        || std::strcmp(saved_scheme, selected_scheme) == 0)
    {
        return false;
    }

    miscellaneous_settings_->SetString(kSchemeKey, selected_scheme);

    return true;
}

void OptionsSubMiscellaneous::PrepareColorSchemesList() const
{
    color_scheme_->DeleteAllItems();

    std::string current_scheme_file = miscellaneous_settings_->GetString(kSchemeKey, kDefaultScheme);

    FileFindHandle_t find_handle = 0;
    const char* scheme_file = g_pFullFileSystem->FindFirst("resource/schemes/*.res", &find_handle, "GAME");

    while (scheme_file != nullptr)
    {
        if (g_pFullFileSystem->FindIsDirectory(find_handle))
        {
            scheme_file = g_pFullFileSystem->FindNext(find_handle);
            continue;
        }

        std::string scheme_name = MakeSchemeName(scheme_file);
        int item_id = color_scheme_->AddItem(scheme_name.c_str(), KeyValues::AutoDelete(new KeyValues("", "Scheme", scheme_file)));

        if (current_scheme_file == scheme_file)
            color_scheme_->ActivateItem(item_id);

        scheme_file = g_pFullFileSystem->FindNext(find_handle);
    }
}

std::string OptionsSubMiscellaneous::MakeSchemeName(std::string_view scheme_path)
{
    std::string name(scheme_path);

    if (name.starts_with("TrackerScheme_"))
        name.erase(0, sizeof("TrackerScheme_") - 1);

    if (name.ends_with(".res"))
        name.erase(name.size() - sizeof(".res") + 1);

    for (auto it = name.begin(); it != name.end(); it++)
    {
        if (std::isupper(*it) && it != name.begin())
        {
            it = name.insert(it, ' ');
            it++;
        }
    }

    return name;
}

KeyValues* OptionsSubMiscellaneous::GetSettings()
{
    KeyValues* key_values = new KeyValues("MiscellaneousSettings");
    key_values->LoadFromFile(g_pFullFileSystem, kUserSaveDataPath, "GAMECONFIG");

    return key_values;
}
