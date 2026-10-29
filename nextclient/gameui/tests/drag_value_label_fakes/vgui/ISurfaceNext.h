#pragma once

namespace vgui2
{
    class ISurfaceNext
    {
    public:
        virtual ~ISurfaceNext() = default;
        virtual void GetScreenSize(int& width, int& height) = 0;
    };

    ISurfaceNext* surface();
}
