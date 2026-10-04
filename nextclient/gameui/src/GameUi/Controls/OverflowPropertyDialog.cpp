#include "OverflowPropertyDialog.h"
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui/ISurfaceNext.h>
#include <algorithm>

namespace
{
    class CPickerPropertySheet : public vgui2::PropertySheet
    {
        DECLARE_CLASS_SIMPLE(CPickerPropertySheet, vgui2::PropertySheet);

    public:
        CPickerPropertySheet(vgui2::Panel* parent, vgui2::ComboBox* picker) :
            BaseClass(parent, "PickerSheet", picker),
            picker_(picker)
        {
            SetKBNavigationEnabled(true);
            ShowContextButtons(false);
        }

    private:
        // Stock VGUI matches a truncated label; select by row so long or equal
        // translated titles still identify the correct page. Population and
        // programmatic selection synchronization remain owned by PropertySheet.
        MESSAGE_FUNC_PTR(OnPickerChanged, "TextChanged", panel)
        {
            if (panel == picker_)
            {
                const int row = picker_->GetActiveItem();
                if (row >= 0 && row < GetNumPages())
                    SetActivePage(GetPage(row));
            }
        }
        vgui2::ComboBox* picker_;
    };
} // namespace

COverflowPropertyDialog::COverflowPropertyDialog(vgui2::Panel* parent, const char* name) :
    BaseClass(parent, name)
{}

void COverflowPropertyDialog::AddPage(vgui2::Panel* page, const char* title)
{
    pages_.push_back({page, title});
    GetPropertySheet()->AddPage(page, title);
}
vgui2::PropertySheet* COverflowPropertyDialog::GetPropertySheet()
{
    return picker_sheet_ ? picker_sheet_ : BaseClass::GetPropertySheet();
}
vgui2::Panel* COverflowPropertyDialog::GetActivePage()
{
    return GetPropertySheet()->GetActivePage();
}
void COverflowPropertyDialog::ResetAllData()
{
    GetPropertySheet()->ResetAllData();
}
void COverflowPropertyDialog::RequestFocus(int direction)
{
    GetPropertySheet()->RequestFocus(direction);
}
void COverflowPropertyDialog::FitPageNavigation()
{
    int screenWide, screenTall;
    vgui2::surface()->GetScreenSize(screenWide, screenTall);
    const int available = std::max(1, screenWide - 40);
    if (!picker_sheet_ && !pages_.empty())
    {
        auto* sheet = GetPropertySheet();
        auto* active = sheet->GetActivePage();
        sheet->SetActivePage(pages_.back().panel);
        sheet->InvalidateLayout(true);
        auto* lastTab = sheet->GetActiveTab();
        const int required = lastTab->GetXPos() + lastTab->GetWide() + GetWide() - sheet->GetWide();
        sheet->SetActivePage(active);
        if (required <= available)
            SetWide(std::max(GetWide(), required));
        else
        {
            picker_ = new vgui2::ComboBox(this, "AllSettingsTabs", 12, false);
            picker_sheet_ = new CPickerPropertySheet(this, picker_);
            picker_sheet_->AddActionSignalTarget(this);
            picker_->SetTabPosition(1);
            picker_sheet_->SetTabPosition(2);
            sheet->RemoveAllPages();
            sheet->SetVisible(false);
            for (const auto& page : pages_)
                picker_sheet_->AddPage(page.panel, page.title.c_str());
            picker_sheet_->SetActivePage(active);
        }
    }
    SetWide(std::min(GetWide(), available));
    InvalidateLayout();
}
void COverflowPropertyDialog::PerformLayout()
{
    BaseClass::PerformLayout();
    if (picker_sheet_)
    {
        int x, y, wide, tall;
        BaseClass::GetPropertySheet()->GetBounds(x, y, wide, tall);
        picker_->SetBounds(x, y, wide, 24);
        picker_sheet_->SetBounds(x, y + 28, wide, std::max(0, tall - 28));
    }
}
bool COverflowPropertyDialog::OnOK(bool applyOnly)
{
    if (picker_sheet_)
        picker_sheet_->ApplyChanges();
    return BaseClass::OnOK(applyOnly);
}
