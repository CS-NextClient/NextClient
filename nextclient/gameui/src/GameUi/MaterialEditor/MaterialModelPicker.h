#pragma once

#include <string>
#include <vector>

#include <vgui_controls/Panel.h>

namespace vgui2
{
    class Button;
    class ComboBox;
} // namespace vgui2

namespace material_editor
{
class CMaterialModelPicker : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CMaterialModelPicker, vgui2::Panel);

private:
    vgui2::ComboBox* model_{};
    vgui2::Button* browse_{};
    std::vector<std::string> choices_;

public:
    CMaterialModelPicker(vgui2::Panel* parent, const char* name);
    void SetModels(const std::string& selected, const std::vector<std::string>& choices);
    void PerformLayout() override;
    void OnCommand(const char* command) override;

private:
    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
};

} // namespace material_editor
