#pragma once

#include <cstdint>
#include "interface.h"
#include "IGameConsole.h"

class IGameConsoleNext : public IBaseInterface
{
public:
    /*
     * format and string parameters must be encoded as UTF8.
     * To convert ANSI codepage you may to use nitro_utils::ConvertCurrentCodepageToUtf8.
     */
    virtual void ColorPrintf(uint8_t r, uint8_t g, uint8_t b, const char *format, ...) = 0;

    virtual void ColorPrintfWide(uint8_t r, uint8_t g, uint8_t b, const wchar_t *format, ...) = 0;

    /*
     * format and string parameters must be encoded as UTF8.
     * To convert ANSI codepage you may to use nitro_utils::ConvertCurrentCodepageToUtf8.
     *
     * format may contain special tags:
     *  [color=r,g,b][/] - text between tags will be colored in r, g, b which must be between 0 and 255
     */
    virtual void PrintfEx(const char *format, ...) = 0;

    /*
     * format may contain special tags:
     *  [color=r,g,b][/] - text between tags will be colored in r, g, b which must be between 0 and 255
     */
    virtual void PrintfExWide(const wchar_t *format, ...) = 0;
};

#define GAMECONSOLE_NEXT_INTERFACE_VERSION "GameConsoleNext002"