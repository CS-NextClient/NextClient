#pragma once

#include <vgui_controls/PropertyDialog.h>

class COverflowPropertyDialog : public vgui2::PropertyDialog
{
    DECLARE_CLASS_SIMPLE(COverflowPropertyDialog, vgui2::PropertyDialog);

public:
    COverflowPropertyDialog(vgui2::Panel* parent, const char* name);
    void AddPage(vgui2::Panel* page, const char* title) override;
    void SetPageEnabled(vgui2::Panel* page, bool enabled);
    vgui2::PropertySheet* GetPropertySheet() override;
    vgui2::Panel* GetActivePage() override;
    void ResetAllData() override;

protected:
    void PerformLayout() override;
    void RequestFocus(int direction = 0) override;
    bool OnOK(bool apply_only) override;
    void OnCommand(const char* command) override;
    void OnClose() override;

private:
    class CEnabledPropertySheet* sheet_;
};
