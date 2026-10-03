#ifdef _WIN32

#include <Windows.h>

extern "C"
{
    // NVIDIA
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

    // AMD
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// No equivalent outside Windows - hybrid GPU selection on Linux is a runtime
// environment variable (DRI_PRIME, __NV_PRIME_RENDER_OFFLOAD, ...) set by the
// user/desktop environment, not a hint embedded in the binary.
#endif