#include "multi_button_binds.h"
#include <array>
#include <string>
#include <goldsrc/client_api.h>
#include <key_binding/key_binding.h>

namespace
{
    // Key_Event drops key numbers above 255
    constexpr size_t kKeyCount = 256;

    // The release commands of each key whose press ran here, empty when it is up
    std::array<std::string, kKeyCount> g_PendingReleases;
}

int MultiButtonBinds_KeyEvent(int down, int keynum, const char* binding, HUD_Key_EventNext next)
{
    int result = next->Invoke(down, keynum, binding);
    std::string& pending_release = g_PendingReleases.at(keynum);

    if (!down) {
        if (pending_release.empty()) {
            return result;
        }

        gEngfuncs.pfnClientCmd(pending_release.c_str());
        pending_release.clear();
        return 0;
    }

    if (result == 0 || binding == nullptr || !key_binding::IsMultiButton(binding)) {
        return result;
    }

    // Key_Event asks the client about a key's release only after the client has eaten its
    // press, so the press is eaten even when it repeats.
    if (pending_release.empty()) {
        pending_release = key_binding::ReleaseCommands(binding, keynum);
        gEngfuncs.pfnClientCmd(key_binding::PressCommands(binding, keynum).c_str());
    }
    return 0;
}
