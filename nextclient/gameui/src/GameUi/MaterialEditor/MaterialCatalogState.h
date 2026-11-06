#pragma once

#include <span>
#include <string>
#include <vector>

namespace material_editor
{
struct MaterialCatalogVisibleRows
{
    int first_row{};
    int first_y{};
    std::span<const int> indices;
};

class MaterialCatalogState
{
private:
    std::vector<std::string> models_;
    std::vector<int> matches_;
    std::string filter_;

public:
    void SetModels(const std::vector<std::string>& models);
    bool SetFilter(const std::string& filter);
    int get_match_count() const;
    const std::string& get_model(int index) const;
    const std::vector<int>& get_indices() const;
    MaterialCatalogVisibleRows GetVisibleRows(int top, int height, int row_height) const;

private:
    void Filter();
};

} // namespace material_editor
