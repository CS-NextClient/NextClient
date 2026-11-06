#pragma once
namespace vgui2
{
    class ISystem
    {
    public:
        double GetCurrentTime()
        {
            return 0;
        }
    };
    ISystem* system();
} // namespace vgui2
