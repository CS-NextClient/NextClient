#include "GameConsoleNext.h"
#include <strtools.h>
#include <cstdarg>
#include <nitro_utils/string_utils.h>
#include <nitro_utils/config/FileConfigProvider.h>

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

    // This provides a 1 frame delay to display the text after the temporary buffer from the engine
    TaskCoro::RunInMainThread([this]
    {
        ExecuteTempConsoleBuffer();
    });
}

void CGameConsoleNext::ColorPrintf(uint8_t r, uint8_t g, uint8_t b, const char *format, ...)
{
    char msg[4096];

    va_list params;
    va_start(params, format);
    Q_vsnprintf(msg, sizeof(msg), format, params);
    msg[sizeof(msg) - 1] = '\0';
    va_end(params);

    Color color(r, g, b, 255);

    if (initialized_)
    {
        console_dialog_->ColorPrint(color, msg);
    }
    else
    {
        temp_console_buffer_.emplace_back(color, Utf8ToWstring(msg));
    }
}

void CGameConsoleNext::ColorPrintfWide(uint8_t r, uint8_t g, uint8_t b, const wchar_t *format, ...)
{
    wchar_t msg[4096];

    va_list params;
    va_start(params, format);
    int symbols_written = V_vsnwprintf(msg, sizeof(msg), format, params);
    msg[sizeof(msg) / sizeof(wchar_t) - 1] = '\0';
    va_end(params);

    Color color(r, g, b, 255);

    if (initialized_)
    {
        console_dialog_->ColorPrint(color, msg, msg + symbols_written);
    }
    else
    {
        temp_console_buffer_.emplace_back(color, std::wstring(msg, msg + symbols_written));
    }
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

void CGameConsoleNext::ExecuteTempConsoleBuffer()
{
    for (const auto& [color, text] : temp_console_buffer_)
    {
        console_dialog_->ColorPrint(color, text.data(), text.data() + text.size());
    }

    temp_console_buffer_.clear();
    temp_console_buffer_.shrink_to_fit();
}

std::wstring CGameConsoleNext::Utf8ToWstring(const char* str)
{
    int cch = Q_strlen(str);
    int cubDest = (cch + 1) * sizeof(wchar_t);
    wchar_t* pwch = (wchar_t*) stackalloc(cubDest);
    int cwch = Q_UTF8ToWString(str, pwch, cubDest) / sizeof(wchar_t);

    return std::wstring(pwch, pwch + cwch);
}
