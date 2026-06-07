#include "engine.h"
#include "font_render.h"

#include <funchook.h>
#include <nitro_utils/MemoryTools.h>
#include <nitro_utils/platform.h>

#include <Windows.h>

// Two fixes for hw.dll's CWin32Font, both affecting non-latin glyphs (cyrillic, greek, etc.).
//
// 1) AddGlyphSetToFont registers the requested scheme font only for 0x0-0xFF and
//    a Tahoma fallback for 0x100-0xFFFF, so characters above 0xFF render from Tahoma instead
//    of the requested face. GetFontForChar (win32) returns the first range that
//    contains the char without a glyph-presence check, so widening the primary
//    range to 0x0-0xFFFF makes the requested font cover everything (GDI still
//    substitutes truly-missing glyphs at ExtTextOut). The patch flips the 0xFF
//    high-range immediate of the primary AddFont call to 0xFFFF.
//
// 2) GetCharABCWidths bakes the dropshadow offset into b only for the cached
//    range (ch < 0x80); for ch >= 0x80 it returns the raw abc, while GetCharRGBA
//    always copies (b - dropshadow) columns, so those characters lose their
//    rightmost column. The hook widens b / narrows c by the dropshadow offset for
//    ch >= 0x80 (mirrors the cache; the advance a + b + c is unchanged).

namespace
{
    // ---- fix 1: requested font covers the full unicode range --------------------

    // push 0xFFFF; push 0x100 -> the asianFont AddFont(0x100, 0xFFFF) call
    constexpr const char* kRangeAnchor = "68 FF FF 00 00 68 00 01 00 00";

    uint8_t* g_RangePatchByte;
    uint8_t g_RangeOriginalByte;

    bool PatchFullRange()
    {
        MemoryModule module("hw.dll");
        if (module.Module() == nullptr)
        {
            return false;
        }

        MemScanner scanner(module);
        uint32_t anchor = scanner.FindPattern2(kRangeAnchor);
        if (anchor == 0)
        {
            return false;
        }

        // the primary AddFont(winFont, 0x0, 0xFF) sits just before the anchor;
        // find its "push 0xFF" (68 FF 00 00 00) and flip the high byte to 0xFFFF
        uint8_t* anchor_ptr = reinterpret_cast<uint8_t*>(anchor);
        uint8_t* push_ptr = nullptr;
        for (uint8_t* cursor = anchor_ptr - 1; cursor >= anchor_ptr - 0x20; --cursor)
        {
            if (cursor[0] == 0x68 && cursor[1] == 0xFF && cursor[2] == 0x00 && cursor[3] == 0x00 && cursor[4] == 0x00)
            {
                push_ptr = cursor;
                break;
            }
        }
        if (push_ptr == nullptr)
        {
            return false;
        }

        g_RangePatchByte = push_ptr + 2; // 0x00 -> 0xFF turns "push 0xFF" into "push 0xFFFF"
        g_RangeOriginalByte = *g_RangePatchByte;

        nitro_utils::SetProtect(g_RangePatchByte, 1, nitro_utils::ProtectMode::PROTECT_RWE);
        *g_RangePatchByte = 0xFF;
        nitro_utils::SetProtect(g_RangePatchByte, 1, nitro_utils::ProtectMode::PROTECT_RE);
        FlushInstructionCache(GetCurrentProcess(), g_RangePatchByte, 1);
        return true;
    }

    void UnpatchFullRange()
    {
        if (g_RangePatchByte == nullptr)
        {
            return;
        }

        nitro_utils::SetProtect(g_RangePatchByte, 1, nitro_utils::ProtectMode::PROTECT_RWE);
        *g_RangePatchByte = g_RangeOriginalByte;
        nitro_utils::SetProtect(g_RangePatchByte, 1, nitro_utils::ProtectMode::PROTECT_RE);
        FlushInstructionCache(GetCurrentProcess(), g_RangePatchByte, 1);
        g_RangePatchByte = nullptr;
    }

    // ---- fix 2: keep the dropshadow but stop clipping non-latin glyphs ----------

    // CWin32Font::GetCharABCWidths prologue
    constexpr const char* kAbcSignature = "83 EC 24 53 8B 5C 24 2C 55 8B E9 56 81 FB 80 00 00 00";
    constexpr int kDropShadowFieldOffset = 0x30; // CWin32Font::m_iDropShadowOffset
    constexpr int kExtendedRangeStart = 0x80; // ABCWIDTHS_CACHE_SIZE

    using get_char_abc_widths_t = int(__thiscall*)(void* self, int ch, int* a, int* b, int* c);

    funchook_t* g_GetCharAbcWidthsHook;
    get_char_abc_widths_t g_OriginalGetCharAbcWidths;

    int __fastcall GetCharAbcWidthsHook(void* self, void* /*edx*/, int ch, int* a, int* b, int* c)
    {
        int result = g_OriginalGetCharAbcWidths(self, ch, a, b, c);

        if (ch >= kExtendedRangeStart && b != nullptr && c != nullptr)
        {
            int drop_shadow = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(self) + kDropShadowFieldOffset);
            if (drop_shadow != 0)
            {
                *b += drop_shadow;
                *c -= drop_shadow;
            }
        }

        return result;
    }

    bool InstallAbcWidthFix()
    {
        MemoryModule module("hw.dll");
        if (module.Module() == nullptr)
        {
            return false;
        }

        MemScanner scanner(module);
        uint32_t func = scanner.FindPattern2(kAbcSignature);
        if (func == 0)
        {
            return false;
        }

        g_GetCharAbcWidthsHook = funchook_create();
        if (g_GetCharAbcWidthsHook == nullptr)
        {
            return false;
        }

        void* target = reinterpret_cast<void*>(func);

        // bit-copy the detour address into void* to avoid a function-pointer cast
        auto detour = &GetCharAbcWidthsHook;
        void* detour_ptr = nullptr;
        memcpy(&detour_ptr, &detour, sizeof(detour_ptr));

        if (funchook_prepare(g_GetCharAbcWidthsHook, &target, detour_ptr) != FUNCHOOK_ERROR_SUCCESS)
        {
            funchook_destroy(g_GetCharAbcWidthsHook);
            g_GetCharAbcWidthsHook = nullptr;
            return false;
        }

        // funchook replaced target with the trampoline that calls the original
        memcpy(&g_OriginalGetCharAbcWidths, &target, sizeof(g_OriginalGetCharAbcWidths));

        if (funchook_install(g_GetCharAbcWidthsHook, 0) != FUNCHOOK_ERROR_SUCCESS)
        {
            funchook_destroy(g_GetCharAbcWidthsHook);
            g_GetCharAbcWidthsHook = nullptr;
            g_OriginalGetCharAbcWidths = nullptr;
            return false;
        }

        return true;
    }

    void UninstallAbcWidthFix()
    {
        if (g_GetCharAbcWidthsHook == nullptr)
        {
            return;
        }

        funchook_uninstall(g_GetCharAbcWidthsHook, 0);
        funchook_destroy(g_GetCharAbcWidthsHook);
        g_GetCharAbcWidthsHook = nullptr;
        g_OriginalGetCharAbcWidths = nullptr;
    }
} // namespace

void FontRender_InstallHook()
{
    PatchFullRange();
    InstallAbcWidthFix();
}

void FontRender_UninstallHook()
{
    UninstallAbcWidthFix();
    UnpatchFullRange();
}
