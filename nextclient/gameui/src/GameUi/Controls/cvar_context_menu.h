#pragma once

namespace vgui2
{
    class Menu;
    class Panel;
}

// Builds the menu into menu on first use, parented to host, and opens it at the cursor: the
// cvar name, which copies itself to the clipboard when picked, then an item posting
// "ResetToDefault" to host. Returns false, and opens nothing, when cvar_name is empty or
// with_reset is false.
bool CvarContextMenu_Show(vgui2::Panel* host, vgui2::Menu*& menu, const char* cvar_name, bool with_reset);
