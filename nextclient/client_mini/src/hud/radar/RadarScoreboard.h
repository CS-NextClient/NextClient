#pragma once

#include <cstdint>
#include <cstring>

#include <ncl_math/aabb.h>

class RadarScoreboard
{
public:
    ncl_math::Vector2 Update(
        std::uint32_t panel,
        const ncl_math::Aabb2& bounds,
        ncl_math::Vector2 screen_size,
        const ncl_math::Aabb2* radar
    );
    void Reset();
    bool is_shifted() const { return shifted_; }
    std::uint32_t get_panel() const
    {
        return panel_;
    }

private:
    bool shifted_{};
    std::uint32_t panel_{};
    ncl_math::Vector2 original_{};
    ncl_math::Vector2 placed_{};
    ncl_math::Vector2 size_{};
    ncl_math::Vector2 screen_size_{};
};

// PanelTree follows IPanel's read-only tree interface.
template <class PanelTree>
std::uint32_t RadarScoreboard_FindVisiblePanel(PanelTree& panels, std::uint32_t panel)
{
    if (!panel || !panels.IsVisible(panel))
    {
        return 0;
    }
    if (std::strcmp(panels.GetName(panel), "ClientScoreBoard") == 0)
    {
        return panel;
    }
    int child_count = panels.GetChildCount(panel);
    for (int i = 0; i < child_count; i++)
    {
        if (std::uint32_t found = RadarScoreboard_FindVisiblePanel(panels, panels.GetChild(panel, i)))
        {
            return found;
        }
    }
    return 0;
}
