#pragma once

#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/CheckButton.h>
#include <vector>
#include <string>

class OptionsSubMiscellaneous : public vgui2::PropertyPage
{
    DECLARE_CLASS_SIMPLE(OptionsSubMiscellaneous, vgui2::PropertyPage);

    vgui2::ComboBox* color_scheme_;
    vgui2::ComboBox* server_browser_init_tab_;
    vgui2::CheckButton* disable_auto_open_server_browser_;

    KeyValues* miscellaneous_settings_;

public:
    explicit OptionsSubMiscellaneous(vgui2::Panel *parent);
    ~OptionsSubMiscellaneous() override;

    void OnResetData() override;
    void OnApplyChanges() override;

private:
    MESSAGE_FUNC_PARAMS(OnCheckButtonChecked, "CheckButtonChecked", params);
    MESSAGE_FUNC(OnDataChanged, "ControlModified");
    MESSAGE_FUNC_PTR_CHARPTR(OnTextChanged, "TextChanged", panel, text);

    bool ApplyColorScheme() const;
    void PrepareColorSchemesList() const;

    static std::string MakeSchemeName(std::string_view scheme_path);

public:
    constexpr static char kUserSaveDataPath[] = "MiscellaneousSettings.vdf";

    constexpr static char kSchemeKey[] = "Scheme";
    constexpr static char kDisableAutoOpenServerBrowserKey[] = "DisableAutoOpenServerBrowser";
    constexpr static char kServerBrowserInitialTabKey[] = "ServerBrowserInitialTab";
    constexpr static char kSchemesPath[] = "resource/schemes";
    constexpr static char kDefaultScheme[] = "TrackerScheme_ClassicPlus.res";

    // TODO Сделать нормальный класс вместо KeyValues, чтоб было удобно было пользоваться
    static KeyValues* GetSettings();
};
