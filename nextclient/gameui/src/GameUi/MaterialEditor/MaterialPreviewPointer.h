#pragma once

#include <cstdint>

namespace material_editor
{
class MaterialPreviewPointer
{
private:
    int x_{};
    int y_{};
    bool active_{};
    bool moved_{};

public:
    void Begin(int x, int y)
    {
        x_ = x;
        y_ = y;
        active_ = true;
        moved_ = false;
    }

    bool Move(int x, int y)
    {
        int64_t dx = static_cast<int64_t>(x) - x_;
        int64_t dy = static_cast<int64_t>(y) - y_;
        moved_ = moved_ || dx <= -4 || dx >= 4 || dy <= -4 || dy >= 4 || dx * dx + dy * dy >= 16;
        return active_ && moved_;
    }

    bool End(int x, int y)
    {
        Move(x, y);
        bool click = active_ && !moved_;
        Cancel();
        return click;
    }

    void Cancel()
    {
        active_ = false;
        moved_ = false;
    }
};

} // namespace material_editor
