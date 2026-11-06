#pragma once

#include <vgui/IInput.h>

namespace vgui2
{
    class IInputInternal : public IInput
    {
    public:
        virtual VPANEL GetMouseCapture() = 0;
    };

    IInputInternal* input();
}
