#include <Windows.h>

extern "C"
{
    // NVIDIA
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

    // AMD
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}