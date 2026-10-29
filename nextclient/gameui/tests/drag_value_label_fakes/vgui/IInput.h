#pragma once

#include <vgui_controls/Label.h>

namespace vgui2
{
    enum KeyCode
    {
        KEY_LSHIFT,
        KEY_RSHIFT,
        KEY_LCONTROL,
        KEY_RCONTROL
    };

    class IInput
    {
    public:
        virtual ~IInput() = default;
        virtual VPANEL GetAppModalSurface()
        {
            return 0;
        }
        virtual void SetMouseCapture(VPANEL panel) = 0;
        virtual void GetCursorPos(int& x, int& y) = 0;
        virtual void SetCursorPos(int x, int y) = 0;
        virtual bool IsKeyDown(KeyCode code) = 0;
    };
} // namespace vgui2
