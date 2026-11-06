#pragma once

#include <string>
#include <vector>

#include <vgui_controls/Panel.h>

#include "GameUi/MaterialEditor/MaterialCatalogState.h"
#include "GameUi/MaterialEditor/MaterialModelThumbnailCache.h"

namespace vgui2
{
    class ScrollBar;
}

namespace material_editor
{
class CMaterialModelList : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CMaterialModelList, vgui2::Panel);

private:
    class Row;

    MaterialCatalogState state_;
    MaterialModelThumbnailCache thumbnails_;
    std::vector<Row*> rows_;
    vgui2::ScrollBar* scroll_{};
    vgui2::Panel* receiver_{};

public:
    CMaterialModelList(vgui2::Panel* parent, vgui2::Panel* receiver, IFileSystem& filesystem, vgui2::ISurface& surface);
    ~CMaterialModelList() override;
    void SetModels(const std::vector<std::string>& models);
    void SetFilter(const std::string& filter);
    int get_match_count() const;
    void ProcessOneThumbnail();
    void ClearThumbnails();
    void PerformLayout() override;
    void OnMouseWheeled(int delta) override;

private:
    MESSAGE_FUNC_INT(OnScrollMoved, "ScrollBarSliderMoved", position);
};

} // namespace material_editor
