#include "GameConsoleNext.h"
#include <strtools.h>
#include <cstdarg>

static CGameConsoleNext g_GameConsoleNext;

CGameConsoleNext &GameConsoleNext()
{
    return g_GameConsoleNext;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameConsole, IGameConsoleNext, GAMECONSOLE_NEXT_INTERFACE_VERSION, g_GameConsoleNext);

void CGameConsoleNext::Initialize(CGameConsoleDialog *console_dialog)
{
    if (initialized_)
        return;

    console_dialog_ = console_dialog;

    initialized_ = true;
}

void CGameConsoleNext::ColorPrintf(uint8_t r, uint8_t g, uint8_t b, const char *format, ...)
{
    if (!initialized_)
        return;

    char msg[4096];

    va_list params;
    va_start(params, format);
    Q_vsnprintf(msg, sizeof(msg), format, params);
    msg[sizeof(msg) - 1] = '\0';
    va_end(params);

    console_dialog_->ColorPrint(Color(r, g, b, 255), msg);
}

void CGameConsoleNext::ColorPrintfWide(uint8_t r, uint8_t g, uint8_t b, const wchar_t *format, ...)
{
    if (!initialized_)
        return;

    wchar_t msg[4096];

    va_list params;
    va_start(params, format);
    int symbols_written = V_vsnwprintf(msg, sizeof(msg), format, params);
    msg[sizeof(msg) / sizeof(wchar_t) - 1] = '\0';
    va_end(params);

    console_dialog_->ColorPrint(Color(r, g, b, 255), msg, msg + symbols_written);
}

void CGameConsoleNext::PrintfEx(const char *format, ...)
{
    if (!initialized_)
        return;

    char msg[4096];

    va_list params;
    va_start(params, format);
    V_vsnprintf(msg, sizeof(msg), format, params);
    msg[sizeof(msg) - 1] = '\0';
    va_end(params);

    Printf(msg);
}

void CGameConsoleNext::PrintfExWide(const wchar_t *format, ...)
{
    if (!initialized_)
        return;

    wchar_t msg[4096];

    va_list params;
    va_start(params, format);
    V_vsnwprintf(msg, sizeof(msg) / sizeof(wchar_t), format, params);
    msg[sizeof(msg) / sizeof(wchar_t) - 1] = '\0';
    va_end(params);

    Printf(msg);
}