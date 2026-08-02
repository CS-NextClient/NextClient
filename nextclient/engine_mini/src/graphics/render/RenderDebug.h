#pragma once

namespace render
{
    // Installs or removes the driver debug callback according to gl_debug_output.
    // Called on every context creation and from the cvar hook.
    void Debug_Apply();
}
