#include "GameUi/MaterialEditor/MaterialCatalogState.h"

#include <algorithm>
#include <cassert>
#include <cctype>

namespace material_editor
{

namespace
{
    bool Contains(const std::string& value, const std::string& needle)
    {
        return std::search(value.begin(), value.end(), needle.begin(), needle.end(), [](unsigned char a, unsigned char b) {
                   return std::tolower(a) == std::tolower(b);
               }) != value.end();
    }
} // namespace

void MaterialCatalogState::SetModels(const std::vector<std::string>& models)
{
    models_ = models;
    Filter();
}

bool MaterialCatalogState::SetFilter(const std::string& filter)
{
    if (filter_ == filter)
    {
        return false;
    }

    filter_ = filter;
    Filter();
    return true;
}

void MaterialCatalogState::Filter()
{
    matches_.clear();

    for (size_t index = 0; index < models_.size(); index++)
    {
        if (filter_.empty() || Contains(models_[index], filter_))
        {
            matches_.push_back(static_cast<int>(index));
        }
    }
}

int MaterialCatalogState::get_match_count() const
{
    return static_cast<int>(matches_.size());
}

const std::vector<int>& MaterialCatalogState::get_indices() const
{
    return matches_;
}

const std::string& MaterialCatalogState::get_model(int index) const
{
    return models_[index];
}

MaterialCatalogVisibleRows MaterialCatalogState::GetVisibleRows(int top, int height, int row_height) const
{
    assert(row_height > 0);

    if (height <= 0 || matches_.empty())
    {
        return {};
    }

    int count = get_match_count();
    top = std::clamp(top, 0, std::max(count * row_height - height, 0));
    int first = top / row_height;
    int end = std::min((top + height + row_height - 1) / row_height, count);
    return {first, first * row_height - top, std::span<const int>(matches_).subspan(first, end - first)};
}

} // namespace material_editor
