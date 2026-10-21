#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace key_binding
{
    // Split the way Cbuf_Execute does: at ';' outside quotes and at every newline. Blanks
    // around each command are trimmed and empty commands dropped.
    std::vector<std::string_view> SplitCommands(std::string_view binding);

    // On key up the engine turns only the binding's first '+' into '-' and runs the rest as
    // written, so "+a;+b" releases as "-a;+b". Only a binding that starts with '+' gets a
    // key-up command at all; scripts rely on that to hold a button down.
    bool IsMultiButton(std::string_view binding);

    // Text for Cbuf_AddText. Each +command gets the key number the engine appends to a single
    // one, so a button knows which key holds it.
    std::string PressCommands(std::string_view binding, int key);
    std::string ReleaseCommands(std::string_view binding, int key);
}
