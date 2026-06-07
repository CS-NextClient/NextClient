#pragma once

// Patches hw.dll's CWin32Font so the requested scheme font covers the full
// unicode range (non-latin glyphs render from the requested face instead of the
// Tahoma fallback) and so the dropshadow no longer clips their right column.

// Applies the patches. Must run before the engine creates any font.
void FontRender_InstallHook();

// Reverts the patches.
void FontRender_UninstallHook();
