#include <cstdint>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace nitro_utils
{
    enum class State
    {
        Initialized,
        NotInitialized,
        HighPerformanceCounterNotAvailable
    };

    void InitializeIfNeeded();

    State g_State = State::NotInitialized;

#ifdef _WIN32
    LARGE_INTEGER g_Frequency;

    uint64_t HRCount()
    {
        InitializeIfNeeded();

        if (g_State == State::HighPerformanceCounterNotAvailable)
        {
            return GetTickCount64();
        }

        LARGE_INTEGER time;
        QueryPerformanceCounter(&time);

        return time.QuadPart;
    }

    uint32_t GetElapsedMs(uint64_t end, uint64_t start)
    {
        InitializeIfNeeded();

        if (g_State == State::HighPerformanceCounterNotAvailable)
        {
            return (uint32_t) (end - start);
        }

        return (uint32_t) (((end - start) * 1000) / g_Frequency.QuadPart);
    }

    void InitializeIfNeeded()
    {
        if (g_State != State::NotInitialized)
            return;

        if (QueryPerformanceFrequency(&g_Frequency))
            g_State = State::Initialized;
        else
            g_State = State::HighPerformanceCounterNotAvailable;
    }
#endif
}