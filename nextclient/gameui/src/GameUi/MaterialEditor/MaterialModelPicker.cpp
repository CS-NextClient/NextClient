#include "GameUi/MaterialEditor/MaterialModelPicker.h"

#include <algorithm>

#include <tier1/strtools.h>
#include <tier1/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>

namespace material_editor
{

using namespace vgui2;

CMaterialModelPicker::CMaterialModelPicker(Panel* parent, const char* name) :
    BaseClass(parent, name)
{
    model_ = new ComboBox(this, "ModelChoices", 8, false);
    model_->AddActionSignalTarget(this);
    browse_ = new Button(this, "BrowseModels", "...", this, "browse");
}

void CMaterialModelPicker::SetModels(const std::string& selected, const std::vector<std::string>& choices)
{
    choices_.clear();
    choices_.push_back("");
    choices_.insert(choices_.end(), choices.begin(), choices.end());
    model_->RemoveAll();
    model_->AddItem("#GameUI_MatEdit_StandInNone", nullptr);
    int active = 0;

    for (size_t index = 1; index < choices_.size(); index++)
    {
        model_->AddItem(choices_[index].c_str(), nullptr);

        if (V_stricmp(choices_[index].c_str(), selected.c_str()) == 0)
        {
            active = static_cast<int>(index);
        }
    }

    model_->SilentActivateItemByRow(active);
}

void CMaterialModelPicker::PerformLayout()
{
    BaseClass::PerformLayout();
    model_->SetBounds(0, 0, std::max(GetWide() - 34, 1), GetTall());
    browse_->SetBounds(GetWide() - 30, 0, 30, GetTall());
}

void CMaterialModelPicker::OnCommand(const char* command)
{
    if (V_stricmp(command, "browse") == 0)
    {
        PostActionSignal(new KeyValues("Command", "command", "catalog"));
        return;
    }

    BaseClass::OnCommand(command);
}

void CMaterialModelPicker::OnTextChanged(Panel* panel)
{
    int index = model_->GetActiveItem();

    if (panel == model_ && index >= 0 && index < static_cast<int>(choices_.size()))
    {
        PostActionSignal(new KeyValues("ModelPicked", "model", choices_[index].c_str()));
    }
}

} // namespace material_editor
