#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace vgui2
{
    class Panel;
}

struct PropertyPageTabLayout
{
    std::vector<int> tab_widths;
    int more_width{};
};

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

    PropertyPageTabLayout CalculateLayout(
        const std::vector<int>& widths,
        vgui2::Panel* active_page,
        int available_width,
        int more_width,
        int spacing
    ) const
    {
        assert(widths.size() == pages_.size());
        PropertyPageTabLayout layout{widths};
        const int count = static_cast<int>(widths.size());
        int64_t total_width = static_cast<int64_t>(std::max(0, count - 1)) * spacing;
        for (int width : widths)
        {
            total_width += width;
        }
        if (total_width <= available_width)
        {
            return layout;
        }
        if (count == 1)
        {
            layout.tab_widths[0] = std::min(widths[0], std::max(0, available_width));
            return layout;
        }

        layout.tab_widths.assign(count, 0);
        layout.more_width = std::min(more_width, std::max(0, available_width));
        int remaining = std::max(0, available_width - layout.more_width - spacing);
        int active = -1;
        for (int index = 0; index < count; ++index)
        {
            if (pages_[index].panel == active_page)
            {
                active = index;
                layout.tab_widths[index] = std::min(widths[index], remaining);
                remaining -= layout.tab_widths[index];
                break;
            }
        }
        bool has_visible_tab = active >= 0 && layout.tab_widths[active] > 0;
        for (int index = 0; index < count; ++index)
        {
            if (index == active)
            {
                continue;
            }
            const int required = widths[index] + (has_visible_tab ? spacing : 0);
            if (required > remaining)
            {
                break;
            }
            layout.tab_widths[index] = widths[index];
            remaining -= required;
            has_visible_tab = true;
        }
        layout.more_width = has_visible_tab ? layout.more_width + remaining : std::max(0, available_width);
        return layout;
    }

private:
    struct Page
    {
        vgui2::Panel* panel;
        bool enabled;
    };
    std::vector<Page> pages_;
};
