#include "taskbar_icon.h"
#include <shellapi.h>

namespace
{
    struct EnumWindowCtx
    {
        DWORD target_pid;
        HWND hwnd;
    };

    BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM param)
    {
        EnumWindowCtx* ctx = (EnumWindowCtx*)param;
        DWORD wpid = 0;

        GetWindowThreadProcessId(hwnd, &wpid);

        if (wpid == ctx->target_pid && IsWindowVisible(hwnd))
        {
            char cls[256];
            if (GetClassNameA(hwnd, cls, 256))
            {
                if (strcmp(cls, "SDL_app") == 0)
                {
                    ctx->hwnd = hwnd;
                    return FALSE;
                }
            }
        }
        return TRUE;
    }
} // namespace

HWND FindCurrentProcessSDLWindow()
{
    EnumWindowCtx ctx;
    ctx.target_pid = GetCurrentProcessId();
    ctx.hwnd = nullptr;

    EnumWindows(EnumWindowsCallback, (LPARAM)&ctx);

    return ctx.hwnd;
}

void SetTaskbarIcon(HWND hwnd)
{
    if (!hwnd)
    {
        return;
    }

    wchar_t exe_path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH))
    {
        return;
    }

    HICON icon_big = nullptr;
    HICON icon_small = nullptr;
    UINT extracted = ExtractIconExW(exe_path, 0, &icon_big, &icon_small, 1);
    if (extracted == 0)
    {
        return;
    }

    if (icon_big)
    {
        HICON old_big = (HICON)SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon_big);
        HICON old_class_big = (HICON)SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR)icon_big);

        if (old_big)
        {
            DestroyIcon(old_big);
        }

        if (old_class_big)
        {
            DestroyIcon(old_class_big);
        }
    }

    if (icon_small)
    {
        HICON old_small = (HICON)SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon_small);
        HICON old_class_small = (HICON)SetClassLongPtr(hwnd, GCLP_HICONSM, (LONG_PTR)icon_small);

        if (old_small)
        {
            DestroyIcon(old_small);
        }

        if (old_class_small)
        {
            DestroyIcon(old_class_small);
        }
    }

    if (icon_big || icon_small)
    {
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}
