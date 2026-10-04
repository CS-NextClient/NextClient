#include <nextclient/plugin.hpp>
#include <windows.h>
#include <stdexcept>

#ifdef NC_PROBE_DEPENDENT
NC_MANIFEST(
    R"({"schema":1,"id":"test.dependent","name":"Dependent","author":"Tests","description":"Runtime fixture","version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,"permissions":["ui.settings","player.write"],"compatibility_revision":1,"requires":[{"id":"test.probe","version":"*","reason":"Test failure propagation"}]})"
)
#else
NC_MANIFEST(
    R"({"schema":1,"id":"test.probe","name":"Probe","author":"Tests","description":"Runtime fixture","version":"1.0.0","sdk":"1.1.0","abi":1,"api":1,"permissions":["ui.settings","player.write"],"compatibility_revision":1})"
)
#endif

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        wchar_t path[32768]{};
        if (GetEnvironmentVariableW(L"NEXTCLIENT_PROBE_MARKER", path, 32768) > 0)
        {
            HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
    }
    return TRUE;
}
class Probe : public nextclient::Plugin
{
public:
    void load() override
    {
        checkbox("enabled", "mouse", "Example mouse option", "", false);
        control({sizeof(NcControl), "speed", "mouse", NC_SLIDER, "Speed", "", 5, 0, 10, "", ""});
        control({sizeof(NcControl), "mode", "mouse", NC_CHOICE, "Mode", "", 0, 0, 1, "One\nTwo", ""});
        control({sizeof(NcControl), "action", "mouse", NC_BUTTON, "Action", "", 0, 0, 0, "", ""});
        if (GetEnvironmentVariableW(L"NEXTCLIENT_PROBE_FAIL_LOAD", nullptr, 0))
            throw std::runtime_error("test initialization failure");
    }
    void command(NcCommand& command, const NcPlayer&) override
    {
        command.buttons |= 8;
        if (GetEnvironmentVariableW(L"NEXTCLIENT_PROBE_FAIL_COMMAND", nullptr, 0))
            throw std::runtime_error("test callback failure");
    }
};
NC_PLUGIN(Probe)
