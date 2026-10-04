#pragma once
#include <vgui_controls/PropertyDialog.h>
#include <string>
#include <vector>

// Uses the standard tab strip when it fits and PropertySheet's native combo
// mode otherwise. Navigation is configured at reset/apply boundaries so moving
// pages (which sends ResetData) cannot discard an in-progress edit.
class COverflowPropertyDialog : public vgui2::PropertyDialog
{
    DECLARE_CLASS_SIMPLE(COverflowPropertyDialog, vgui2::PropertyDialog);

public:
    COverflowPropertyDialog(vgui2::Panel* parent, const char* name);
    void AddPage(vgui2::Panel* page, const char* title) override;
    vgui2::PropertySheet* GetPropertySheet() override;
    vgui2::Panel* GetActivePage() override;
    void ResetAllData() override;

protected:
    void FitPageNavigation();
    void PerformLayout() override;
    void RequestFocus(int direction = 0) override;
    bool OnOK(bool applyOnly) override;

private:
    struct Page
    {
        vgui2::Panel* panel;
        std::string title;
    };
    std::vector<Page> pages_;
    vgui2::PropertySheet* picker_sheet_{};
    vgui2::ComboBox* picker_{};
};
