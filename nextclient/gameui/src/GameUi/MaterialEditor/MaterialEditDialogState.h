#pragma once

#include <string>

namespace material_editor
{
class MaterialEditDialogState
{
private:
    std::string saved_material_;
    bool active_{};
    bool dirty_{};
    bool comparing_{};

public:
    void Begin(const char* material)
    {
        saved_material_ = material;
        active_ = true;
        dirty_ = false;
    }

    void MarkMaterialChanged(const char* material)
    {
        if (active_)
        {
            dirty_ = saved_material_ != material;
        }
    }

    void MarkMaterialSaved(const char* material)
    {
        if (active_)
        {
            saved_material_ = material;
            dirty_ = false;
        }
    }

    bool is_material_dirty() const
    {
        return dirty_;
    }

    void SetComparison(bool comparing)
    {
        comparing_ = comparing;
    }

    bool is_comparing() const
    {
        return comparing_;
    }

    void End()
    {
        saved_material_.clear();
        active_ = false;
        dirty_ = false;
        comparing_ = false;
    }
};

} // namespace material_editor
