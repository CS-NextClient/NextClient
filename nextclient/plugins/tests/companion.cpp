#include <windows.h>
extern "C" __declspec(dllexport) int nc_companion_value()
{
    return 42;
}
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        wchar_t path[32768]{};
        if (GetEnvironmentVariableW(L"NEXTCLIENT_PROBE_MARKER", path, 32768))
        {
            const auto file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
    }
    return TRUE;
}
