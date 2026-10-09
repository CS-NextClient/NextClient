#pragma once

#include <algorithm>
#include <vector>

namespace vgui2
{
    class Panel;
}

class PropertyPageNavigation
{
public:
    void AddPage(vgui2::Panel* page)
    {
        pages_.push_back({page, true});
    }

    void RemovePage(vgui2::Panel* page)
    {
        std::erase_if(pages_, [page](const Page& entry) { return entry.panel == page; });
    }

    void SetPageEnabled(vgui2::Panel* page, bool enabled)
    {
        for (Page& entry : pages_)
        {
            if (entry.panel == page)
            {
                entry.enabled = enabled;
                return;
            }
        }
    }

    bool is_page_enabled(vgui2::Panel* page) const
    {
        return std::any_of(pages_.begin(), pages_.end(), [page](const Page& entry) { return entry.panel == page && entry.enabled; });
    }

    int FindEnabled(int start, int direction = 1) const
    {
        const int count = static_cast<int>(pages_.size());
        for (int offset = 0; offset < count; ++offset)
        {
            const int index = ((start + offset * direction) % count + count) % count;
            if (pages_[index].enabled)
            {
                return index;
            }
        }
        return -1;
    }

private:
    struct Page
    {
        vgui2::Panel* panel;
        bool enabled;
    };
    std::vector<Page> pages_;
};
