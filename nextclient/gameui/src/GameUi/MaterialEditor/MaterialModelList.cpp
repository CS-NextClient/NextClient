#include "GameUi/MaterialEditor/MaterialModelList.h"

#include <algorithm>

#include <tier1/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ScrollBar.h>

namespace material_editor
{

using namespace vgui2;

namespace
{
    constexpr int kRowHeight = 76;
}

class CMaterialModelList::Row : public Button
{
    DECLARE_CLASS_SIMPLE(Row, Button);

private:
    MaterialModelThumbnailCache& thumbnails_;
    std::string path_;

public:
    Row(Panel* parent, Panel* receiver, MaterialModelThumbnailCache& thumbnails) :
        BaseClass(parent, "CatalogRow", ""),
        thumbnails_(thumbnails)
    {
        SetContentAlignment(a_west);
        SetTextInset(76, 0);
        AddActionSignalTarget(receiver);
    }

    void SetModel(const std::string& path)
    {
        if (path_ == path)
        {
            return;
        }

        SetSelected(false);
        path_ = path;
        SetText(path.c_str() + 7);
        SetCommand(new KeyValues("ModelPicked", "model", path.c_str()));
    }

    void Paint() override
    {
        BaseClass::Paint();
        thumbnails_.Draw(path_, 4, 4, 68);
    }
};

CMaterialModelList::CMaterialModelList(Panel* parent, Panel* receiver, IFileSystem& filesystem, ISurface& surface) :
    BaseClass(parent, "CatalogList"),
    thumbnails_(filesystem, surface),
    receiver_(receiver)
{
    scroll_ = new ScrollBar(this, "CatalogScroll", true);
    scroll_->SetButtonPressedScrollValue(kRowHeight);
    scroll_->AddActionSignalTarget(this);
}

CMaterialModelList::~CMaterialModelList()
{
    ClearThumbnails();
}

void CMaterialModelList::SetModels(const std::vector<std::string>& models)
{
    state_.SetModels(models);
    scroll_->SetValue(0);
    InvalidateLayout();
}

void CMaterialModelList::SetFilter(const std::string& filter)
{
    if (state_.SetFilter(filter))
    {
        scroll_->SetValue(0);
        InvalidateLayout();
    }
}

int CMaterialModelList::get_match_count() const
{
    return state_.get_match_count();
}

void CMaterialModelList::ProcessOneThumbnail()
{
    thumbnails_.ProcessOne();
}

void CMaterialModelList::ClearThumbnails()
{
    thumbnails_.Clear();
    InvalidateLayout();
}

void CMaterialModelList::PerformLayout()
{
    BaseClass::PerformLayout();

    int height = GetTall();
    int content_height = state_.get_match_count() * kRowHeight;

    scroll_->SetBounds(GetWide() - 20, 0, 18, height);
    scroll_->SetRangeWindow(height);
    scroll_->SetRange(0, std::max(content_height, height));
    scroll_->SetEnabled(content_height > height);

    MaterialCatalogVisibleRows visible = state_.GetVisibleRows(scroll_->GetValue(), height, kRowHeight);

    while (rows_.size() < visible.indices.size())
    {
        rows_.push_back(new Row(this, receiver_, thumbnails_));
    }

    std::vector<std::string> paths;
    paths.reserve(visible.indices.size());

    for (size_t index = 0; index < rows_.size(); index++)
    {
        Row* row = rows_[index];
        row->SetVisible(index < visible.indices.size());

        if (index < visible.indices.size())
        {
            const std::string& path = state_.get_model(visible.indices[index]);
            row->SetModel(path);
            row->SetBounds(4, visible.first_y + static_cast<int>(index) * kRowHeight, GetWide() - 28, kRowHeight);
            paths.push_back(path);
        }
        else
        {
            row->SetSelected(false);
        }
    }

    thumbnails_.SetVisibleModels(paths);
}

void CMaterialModelList::OnMouseWheeled(int delta)
{
    scroll_->SetValue(scroll_->GetValue() - delta * kRowHeight);
}

void CMaterialModelList::OnScrollMoved(int position)
{
    InvalidateLayout();
    Repaint();
}

} // namespace material_editor
