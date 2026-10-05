#include "PluginsDialog.h"
#include "GameUi.h"
#include <nextclient/runtime.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurfaceNext.h>
#include <algorithm>
#include <tier2/tier2.h>
#include <KeyValues.h>
#include "PluginLocalization.h"
#include "Controls/WrappedLabel.h"
#include "PluginText.h"
#undef MessageBox

namespace
{
    class CPluginPermissionDialog : public vgui2::QueryBox
    {
    public:
        CPluginPermissionDialog(const wchar_t* text, vgui2::Panel* parent) :
            QueryBox(PluginToken("#NextPlugins_PermissionTitle").c_str(), L"", parent)
        {
            m_pMessageLabel->SetVisible(false);
            text_ = new vgui2::TextEntry(this, "Permissions");
            text_->SetMultiline(true);
            text_->SetEditable(false);
            text_->SetVerticalScrollbar(true);
            text_->SetText(text);
            int wide, tall;
            vgui2::surface()->GetScreenSize(wide, tall);
            SetSize(std::min(600, wide - 40), std::min(380, tall - 40));
            SetPos((wide - GetWide()) / 2, (tall - GetTall()) / 2);
        }
        void ApplySchemeSettings(vgui2::IScheme* scheme) override
        {
            vgui2::Frame::ApplySchemeSettings(scheme);
        }
        void PerformLayout() override
        {
            vgui2::Frame::PerformLayout();
            text_->SetBounds(12, 32, GetWide() - 24, GetTall() - 82);
            const int buttonWidth = std::min(220, (GetWide() - 36) / 2);
            m_pOkButton->SetBounds(GetWide() - buttonWidth * 2 - 20, GetTall() - 38, buttonWidth, 26);
            m_pCancelButton->SetBounds(GetWide() - buttonWidth - 12, GetTall() - 38, buttonWidth, 26);
        }

    private:
        vgui2::TextEntry* text_;
    };
} // namespace

CPluginsDialog::CPluginsDialog(vgui2::Panel* parent) :
    BaseClass(parent, "PluginsDialog", true)
{
    SetTitle("#NextPlugins_Title", true);
    SetSize(760, 550);
    SetMinimumSize(600, 460);
    SetDeleteSelfOnClose(false);
    list_ = new vgui2::ListPanel(this, "Plugins");
    list_->AddActionSignalTarget(this);
    list_->AddColumnHeader(0, "name", "#NextPlugins_Name", 180);
    list_->AddColumnHeader(1, "version", "#NextPlugins_Version", 70);
    list_->AddColumnHeader(2, "author", "#NextPlugins_Author", 140);
    list_->AddColumnHeader(3, "status", "#NextPlugins_Status", 225);
    for (int column = 0; column < 4; ++column)
        list_->SetColumnSortable(column, false);
    list_->SetEmptyListText("#NextPlugins_Empty");
    details_ = new vgui2::TextEntry(this, "Details");
    details_->SetEditable(false);
    details_->SetMultiline(true);
    details_->SetVerticalScrollbar(true);
    notice_ = new CWrappedLabel(this, "Notice", "#NextPlugins_Trust");
    notice_->SetWrap(true);
    changes_ = new CWrappedLabel(this, "Changes", "#NextPlugins_ChangesDetected");
    changes_->SetWrap(true);
    changes_->SetVisible(false);
    toggle_ = new vgui2::Button(this, "Toggle", "#NextPlugins_Enable", this, "Toggle");
    up_ = new vgui2::Button(this, "Up", "#NextPlugins_Up", this, "Up");
    down_ = new vgui2::Button(this, "Down", "#NextPlugins_Down", this, "Down");
    recommended_ = new vgui2::Button(this, "Recommended", "#NextPlugins_Recommended", this, "Recommended");
    ok_ = new vgui2::Button(this, "OK", "#GameUI_OK", this, "OK");
    ok_->SetEnabled(false);
    cancel_ = new vgui2::Button(this, "Cancel", "#GameUI_Cancel", this, "Cancel");
}
void CPluginsDialog::Activate()
{
    ++consentRequest_;
    if (permissionDialog_.Get())
        permissionDialog_->Close();
    int wide, tall;
    vgui2::surface()->GetScreenSize(wide, tall);
    SetMinimumSize(560, 420);
    SetSize(std::min(GetWide(), wide - 20), std::min(GetTall(), tall - 20));
    auto catalog = tao::json::from_string(nc_runtime_catalog());
    rows_ = catalog.at("plugins");
    initialSelection_ = Selection();
    safeMode_ = catalog.at("safe_mode").get_boolean();
    recoveryPending_ = nc_runtime_recovery_pending() != 0;
    diagnostic_ = PluginDiagnostic(catalog.at("error").get_string());
    if (safeMode_)
        diagnostic_ = PluginTokenText("#NextPlugins_SafeMode") + "\n" + diagnostic_;
    if (recoveryPending_)
        diagnostic_ += "\n" + PluginTokenText("#NextPlugins_AcknowledgeRecovery");
    UpdateDeveloperMode();
    Refresh();
    BaseClass::Activate();
}
void CPluginsDialog::UpdateDeveloperMode()
{
    const bool developerMode = engine && engine->pfnGetCvarFloat("developer") > 0;
    if (developerMode == (detailsTabs_ != nullptr))
        return;
    if (developerMode)
    {
        detailsTabs_ = new vgui2::PropertySheet(this, "DetailsTabs");
        detailsTabs_->AddPage(details_, "#NextPlugins_Details");
        diagnostics_ = new vgui2::TextEntry(detailsTabs_, "Diagnostics");
        diagnostics_->SetEditable(false);
        diagnostics_->SetMultiline(true);
        diagnostics_->SetVerticalScrollbar(true);
        detailsTabs_->AddPage(diagnostics_, "#NextPlugins_Diagnostics");
    }
    else
    {
        // Return to the ordinary details view even if Diagnostics was selected.
        detailsTabs_->RemovePage(details_);
        details_->RemoveActionSignalTarget(detailsTabs_);
        details_->SetParent(this);
        details_->SetVisible(true);
        detailsTabs_->SetVisible(false);
        detailsTabs_->MarkForDeletion();
        detailsTabs_ = nullptr;
        diagnostics_ = nullptr;
    }
    diagnosticsSelected_ = false;
    InvalidateLayout();
}
void CPluginsDialog::OnThink()
{
    BaseClass::OnThink();
    if (!IsVisible())
        return;
    UpdateDeveloperMode();
    const bool selected = detailsTabs_ && detailsTabs_->GetActivePage() == diagnostics_;
    if (selected && !diagnosticsSelected_)
        RefreshDiagnostics(tao::json::from_string(nc_runtime_stats()));
    diagnosticsSelected_ = selected;
}
void CPluginsDialog::RefreshDiagnostics(const tao::json::value& catalog)
{
    if (!diagnostics_)
        return;
    std::string info;
    if (auto resources = catalog.find("resources"))
        info = PluginTokenText("#NextPlugins_Resources") + " " +
               std::to_string(resources->at("host_memory_estimate").as<uint64_t>() / 1024) + " / " +
               std::to_string(resources->at("host_memory_limit").as<uint64_t>() / 1024) + " KiB; " +
               std::to_string(resources->at("frame_callback_ms").as<double>()) + " / " +
               std::to_string(resources->at("frame_budget_ms").as<unsigned>()) + " ms";
    const int n = Selected();
    if (n >= 0 && n < static_cast<int>(rows_.get_array().size()))
    {
        const auto& selected = rows_.at(static_cast<size_t>(n));
        info += "\n\n" + PluginMetadataText(selected, "name", PluginLanguage());
        // Match the live row by identity: pending load-order edits reorder rows_.
        for (const auto& row : catalog.at("plugins").get_array())
            if (row.at("file") == selected.at("file") && row.at("hash") == selected.at("hash"))
            {
                if (auto resources = row.find("resources"))
                {
                    info += "\n" + PluginTokenText("#NextPlugins_CallbackTiming");
                    for (const auto& [category, timing] : resources->at("callbacks").get_object())
                        if (timing.at("count").as<uint64_t>())
                            info += "\n" + category + ": " + std::to_string(timing.at("last_ms").as<double>()) + " / " +
                                    std::to_string(timing.at("max_ms").as<double>()) + " ms";
                }
                break;
            }
    }
    else
        info += "\n\n" + PluginTokenText("#NextPlugins_SelectDiagnostics");
    diagnostics_->SetText(PluginWide(info).c_str());
}
int CPluginsDialog::Selected() const
{
    if (list_->GetSelectedItemsCount() != 1)
        return -1;
    auto* row = list_->GetItem(list_->GetSelectedItem(0));
    return row ? row->GetInt("index", -1) : -1;
}
std::string CPluginsDialog::SelectionJson() const
{
    tao::json::value selection = tao::json::empty_array;
    for (const auto& row : rows_.get_array())
        selection.push_back(
            tao::json::value{
                {"file", row.at("file")}, {"hash", row.at("hash")}, {"enabled", row.at("enabled")}, {"consent", row.at("consent")}
            }
        );
    return tao::json::to_string(selection);
}
std::vector<PluginSelection> CPluginsDialog::Selection() const
{
    std::vector<PluginSelection> selection;
    for (const auto& row : rows_.get_array())
        selection.push_back({row.at("file").get_string(), row.at("enabled").get_boolean()});
    return selection;
}
void CPluginsDialog::Refresh(int selected)
{
    const auto selection = Selection();
    const bool changed = selection != initialSelection_;
    changes_->SetVisible(changed);
    ok_->SetEnabled(PluginCanConfirmSelection(initialSelection_, selection, recoveryPending_));
    InvalidateLayout();
    list_->DeleteAllItems();
    for (size_t n = 0; n < rows_.get_array().size(); ++n)
    {
        const auto& r = rows_.at(n);
        auto* kv = new KeyValues("plugin");
        kv->SetInt("index", static_cast<int>(n));
        kv->SetWString("name", PluginWide(PluginMetadataText(r, "name", PluginLanguage())).c_str());
        for (const char* key : {"version", "author"})
            kv->SetWString(key, PluginWide(r.at(key).get_string()).c_str());
        std::wstring state = PluginToken(PluginStatusToken(
            initialSelection_, selection, n, r.at("running").get_boolean(), !r.at("error").get_string().empty(), safeMode_
        ));
        kv->SetWString("status", state.c_str());
        int id = list_->AddItem(kv, 0, false, false);
        kv->deleteThis();
        if (static_cast<int>(n) == selected)
            list_->SetSingleSelectedItem(id);
    }
    OnItemSelected();
}
void CPluginsDialog::OnItemSelected()
{
    int n = Selected();
    bool valid = n >= 0 && n < static_cast<int>(rows_.get_array().size());
    toggle_->SetEnabled(valid);
    up_->SetEnabled(valid && n > 0);
    down_->SetEnabled(valid && n + 1 < static_cast<int>(rows_.get_array().size()));
    std::string info = diagnostic_;
    if (valid)
    {
        const auto& r = rows_.at(static_cast<size_t>(n));
        toggle_->SetText(r.at("enabled").get_boolean() ? "#NextPlugins_Disable" : "#NextPlugins_Enable");
        toggle_->SetEnabled(r.at("error").get_string().empty() || r.at("enabled").get_boolean());
        info += "\n" + PluginMetadataText(r, "description", PluginLanguage()) + "\n\n" + PluginTokenText("#NextPlugins_File") + " " +
                r.at("file").get_string() + "\n";
        if (!r.at("approved").get_boolean())
            info += PluginTokenText("#NextPlugins_Unapproved") + "\n";
        info += PluginDiagnostic(r.at("error").get_string()) + "\n" + PluginDiagnostic(r.at("warning").get_string());
        info += "\n" + PluginPermissionText(r, [](const auto& token) { return PluginTokenText(token.c_str()); });
    }
    // Recommendations are advisory. Surface violations without changing the
    // user's order or introducing an additional restart confirmation.
    auto advice = tao::json::from_string(nc_runtime_recommend(SelectionJson().c_str()));
    if (auto warning = advice.find("warning"))
        info += "\n" + PluginDiagnostic(warning->get_string());
    details_->SetText(PluginWide(info).c_str());
    if (detailsTabs_ && detailsTabs_->GetActivePage() == diagnostics_)
        RefreshDiagnostics(tao::json::from_string(nc_runtime_stats()));
}
void CPluginsDialog::OnCommand(const char* command)
{
    int n = Selected();
    if (!Q_stricmp(command, "OK"))
    {
        const bool changed = Selection() != initialSelection_;
        if (!PluginCanConfirmSelection(initialSelection_, Selection(), recoveryPending_))
            return;
        const char* error = changed ? nc_runtime_save(SelectionJson().c_str()) : nc_runtime_acknowledge_recovery();
        if (*error)
        {
            auto* box = new vgui2::MessageBox(PluginToken("#NextPlugins_Title").c_str(), PluginWide(PluginDiagnostic(error)).c_str(), this);
            box->DoModal();
            return;
        }
        recoveryPending_ = false;
        if (changed)
            engine->pfnClientCmd("fmod stop\n_restart\n");
        Close();
    }
    else if (!Q_stricmp(command, "Cancel"))
        Close();
    else if (!Q_stricmp(command, "Recommended"))
    {
        auto result = tao::json::from_string(nc_runtime_recommend(SelectionJson().c_str()));
        if (auto error = result.find("error"))
            diagnostic_ = PluginDiagnostic(error->get_string());
        else
        {
            rows_ = result.at("plugins");
            diagnostic_ = PluginDiagnostic(result.at("warning").get_string());
        }
        Refresh();
    }
    else if (n >= 0 && !Q_stricmp(command, "Toggle"))
    {
        auto& r = rows_.at(static_cast<size_t>(n));
        if (!r.at("enabled").get_boolean() && !r.at("approved").get_boolean() && !r.at("consent").get_boolean())
        {
            if (!r.at("error").get_string().empty())
                return;
            const auto text = PluginConsentText(r, PluginLanguage(), [](const auto& token) { return PluginTokenText(token.c_str()); });
            auto* box = new CPluginPermissionDialog(PluginWide(text).c_str(), this);
            box->SetOKButtonText("#NextPlugins_AllowEnable");
            box->SetCancelButtonText("#GameUI_Cancel");
            auto* accepted = new KeyValues("PluginPermissionAccepted");
            accepted->SetString("file", r.at("file").get_string().c_str());
            accepted->SetString("hash", r.at("hash").get_string().c_str());
            accepted->SetInt("request", ++consentRequest_);
            box->SetOKCommand(accepted);
            box->AddActionSignalTarget(this);
            permissionDialog_ = box;
            box->DoModal();
            return;
        }
        r["enabled"] = !r.at("enabled").get_boolean();
        Refresh(n);
    }
    else if (n > 0 && !Q_stricmp(command, "Up"))
    {
        std::swap(rows_.at(static_cast<size_t>(n)), rows_.at(static_cast<size_t>(n - 1)));
        Refresh(n - 1);
    }
    else if (n >= 0 && n + 1 < static_cast<int>(rows_.get_array().size()) && !Q_stricmp(command, "Down"))
    {
        std::swap(rows_.at(static_cast<size_t>(n)), rows_.at(static_cast<size_t>(n + 1)));
        Refresh(n + 1);
    }
    else
        BaseClass::OnCommand(command);
}
void CPluginsDialog::OnPermissionAccepted(KeyValues* data)
{
    if (data->GetInt("request") != consentRequest_)
        return;
    ++consentRequest_;
    if (AcceptPluginConsent(rows_, data->GetString("file"), data->GetString("hash")))
        Refresh(Selected());
}
void CPluginsDialog::PerformLayout()
{
    BaseClass::PerformLayout();
    int w = GetWide(), h = GetTall();
    const int listHeight = std::max(64, (h - 180) / 2 - (detailsTabs_ ? 32 : 0));
    list_->SetBounds(12, 32, w - 24, listHeight);
    int y = 32 + listHeight + 8;
    toggle_->SetBounds(12, y, 100, 26);
    up_->SetBounds(120, y, 80, 26);
    down_->SetBounds(208, y, 80, 26);
    recommended_->SetBounds(296, y, 210, 26);
    int changesHeight = changes_->IsVisible() ? 44 : 0;
    vgui2::Panel* detailPanel = detailsTabs_ ? static_cast<vgui2::Panel*>(detailsTabs_) : details_;
    detailPanel->SetBounds(12, y + 34, w - 24, h - y - 135 - changesHeight);
    changes_->SetBounds(12, h - 98 - changesHeight, w - 24, changesHeight);
    notice_->SetBounds(12, h - 94, w - 24, 48);
    ok_->SetBounds(w - 192, h - 36, 80, 24);
    cancel_->SetBounds(w - 104, h - 36, 92, 24);
}
