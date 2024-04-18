#include <string>
#include <nitro_utils/platform.h>
#include <nitro_utils/MemoryTools.h>
#include <tier1/utlvector.h>
#include <Color.h>
#include "main.h"
#include "IGameConsole.h"
#include <next_gameui/IGameConsoleNext.h>

#ifdef _WIN32
const char* kChatPrintConsolePatternWin32 = "FF 15 ? ? ? ? 83 C4 ? 66 89 1D +1";
const char* kSayTextLinePatternWin32 = "[66 39 1D ? ? ? ? 55 +3]&";

const int kCGameConsoleDialogCompColorPad = 292;
#else
const int kChatPrintConsoleAddressLinux = 0x117145;
const int kSayTextLineAddressLinux = 0x2393A0;

const int kCGameConsoleDialogCompColorPad = 292;
#endif

#define MAX_LINES 5
#define MAX_CHARS_PER_LINE 256

#define F2B(f) ((f) >= 1.0f ? 255 : (int)((f)*256.f))

struct CGameConsoleDialogComp
{
    char pad[kCGameConsoleDialogCompColorPad];
    Color color;
};

struct CGameConsoleComp
{
    void* base;
    bool m_bInitialized;
    CGameConsoleDialogComp* m_pConsole;
};

struct TextRange
{
    int start;
    int end;
    float *color;
};

struct SayTextLine
{
    wchar_t m_line[MAX_CHARS_PER_LINE];
    CUtlVector<TextRange> m_textRanges;
    int m_clientIndex;
    float *m_teamColor;
};

static void ColorChatConsolePrint(char string[512]);

#define g_sayTextLine (*pg_sayTextLine)
static SayTextLine g_sayTextLine[MAX_LINES + 1];

void ColorChatInConsolePatch()
{
#ifdef _WIN32
    MemoryModule module("client.dll");
    MemScanner scanner(module);

    uint32_t printConsoleAddress = scanner.FindPattern2(kChatPrintConsolePatternWin32);
    uint32_t sayTextLineAddress = scanner.FindPattern2(kSayTextLinePatternWin32);
#else
    MemoryModule module("client.so");

    uint32_t printConsoleAddress = module.Start() + kChatPrintConsoleAddressLinux;
    uint32_t sayTextLineAddress = module.Start() + kSayTextLineAddressLinux;
#endif

    pg_sayTextLine = reinterpret_cast<decltype(pg_sayTextLine)>(sayTextLineAddress);

    nitro_utils::SetProtect((void*)(printConsoleAddress - 1), 6, nitro_utils::ProtectMode::PROTECT_RWE);
    *(uint8_t*)(printConsoleAddress - 1) = 0x90; // NOP
    *(uint8_t*)printConsoleAddress = 0xe8; // CALL
    *(intptr_t*)(printConsoleAddress + 1) = (uintptr_t)ColorChatConsolePrint - (printConsoleAddress + 5); // ADDRESS
    nitro_utils::SetProtect((void*)(printConsoleAddress - 1), 6, nitro_utils::ProtectMode::PROTECT_RE);
}

static void PrintWithConsole(TextRange* range, const std::wstring& print_text)
{
    int size_needed = Q_UTF32ToUTF8((const uchar32*)print_text.c_str(), nullptr, 0);
    std::string print_text_utf8(size_needed, 0);
    Q_UTF32ToUTF8((const uchar32*)print_text.c_str(), print_text_utf8.data(), size_needed);

    auto game_console_comp = (CGameConsoleComp*)g_GameConsole;

    uint32_t r, g, b;

    if (range->color)
    {
        r = F2B(range->color[0]);
        g = F2B(range->color[1]);
        b = F2B(range->color[2]);
    }
    else
    {
        const char* con_color = gEngfuncs.pfnGetCvarString("con_color");
        if (sscanf(con_color, "%u %u %u", &r, &g, &b) != 3)
        {
            r = 255;
            g = 180;
            b = 30;
        }
    }

    Color old_color = game_console_comp->m_pConsole->color;
    game_console_comp->m_pConsole->color.SetColor(r, g, b, 255);
    g_GameConsole->Printf("%s", print_text_utf8.c_str());
    game_console_comp->m_pConsole->color = old_color;
}

static void PrintWithConsoleNext(TextRange* range, const std::wstring& print_text)
{
    uint32_t r, g, b;

    if (range->color)
    {
        r = F2B(range->color[0]);
        g = F2B(range->color[1]);
        b = F2B(range->color[2]);
    }
    else
    {
        const char* con_color = gEngfuncs.pfnGetCvarString("con_color");
        if (sscanf(con_color, "%u %u %u", &r, &g, &b) != 3)
        {
            r = 255;
            g = 180;
            b = 30;
        }
    }

    g_GameConsoleNext->ColorPrintfWide(r, g, b, L"%s", print_text.c_str());
}

static void ColorChatConsolePrint(char string[512])
{
    int rangeIndex;
    TextRange *range;
    int total_length = 0;


    if (g_sayTextLine[0].m_textRanges.Count() != 0)
    {
        for (rangeIndex = 0; rangeIndex < g_sayTextLine[0].m_textRanges.Count(); rangeIndex++)
        {
            range = &g_sayTextLine[0].m_textRanges[rangeIndex];

            std::wstring print_text = std::wstring(&g_sayTextLine[0].m_line[range->start], range->end - range->start);

            if (g_GameConsoleNext)
                PrintWithConsoleNext(range, print_text);
            else
                PrintWithConsole(range, print_text);
        }

        // Print newline char if needed
        std::string check_string(&string[0], total_length);
        if (check_string.find('\n') == std::string::npos)
        {
            g_GameConsole->Printf("%s", "\n");
        }
    }
}
