#include <string>
#include <vector>
#include <utility>
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
// Offsets into ScrollTextUp() of Steam's client.so; the instruction bytes are checked before use
const int kScrollTextUpSayTextLineRef = 0xA;     // 8B 3D <addr>: mov g_sayTextLine, %edi
const int kScrollTextUpConsolePrintCall = 0x1E6; // second byte of FF 15: call *gEngfuncs.pfnConsolePrint

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
static bool suppressLocalChatEcho = false;

int PrintLocalChat(int (*handler)(const char*, int, void*), const char* text)
{
    if (!handler || !text)
        return 0;
    std::vector<char> message{0}; // Local sender; no network chat.
    message.insert(message.end(), text, text + std::char_traits<char>::length(text));
    message.push_back('\n');
    message.push_back(0);
    struct RestoreEcho
    {
        bool previous = std::exchange(suppressLocalChatEcho, true);
        cl_enginefunc_t* engine = client()->gEngfuncs;
        pfnEngSrc_pfnConsolePrint_t print{};
        pfnEngSrc_Con_Printf_t printf{}, dprintf{};
        RestoreEcho()
        {
            // SayText has both plain and colored console paths. Suppress the
            // engine table calls as well as our colored-console patch.
            if (engine)
            {
                print = std::exchange(engine->pfnConsolePrint, +[](const char*) {});
                printf = std::exchange(engine->Con_Printf, IgnorePrintf);
                dprintf = std::exchange(engine->Con_DPrintf, IgnorePrintf);
            }
        }
        static void IgnorePrintf(const char*, ...) {}
        ~RestoreEcho()
        {
            if (engine)
            {
                engine->pfnConsolePrint = print;
                engine->Con_Printf = printf;
                engine->Con_DPrintf = dprintf;
            }
            suppressLocalChatEcho = previous;
        }
    } restore;
    return handler("SayText", static_cast<int>(message.size()), message.data());
}

void PrintPluginConsole(const char* text)
{
    unsigned char color = 1;
    const char* start = text;
    for (const char* p = text;; ++p)
    {
        const auto c = static_cast<unsigned char>(*p);
        if (c != 0 && c != 1 && c != 3 && c != 4)
            continue;
        if (p != start)
        {
            const std::string part(start, p);
            if (color == 4 && g_GameConsoleNext)
                g_GameConsoleNext->ColorPrintf(110, 220, 100, "%s", part.c_str());
            else if (color == 3 && g_GameConsoleNext)
                g_GameConsoleNext->ColorPrintf(150, 190, 255, "%s", part.c_str());
            else
                gEngfuncs.Con_Printf("%s", part.c_str());
        }
        if (!c)
            break;
        color = c;
        start = p + 1;
    }
}

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
    MemoryModule module("cstrike/cl_dlls/client.so");

    auto scrollTextUp = (uint8_t*)dlsym(module.Module(), "_Z12ScrollTextUpv");
    if (!scrollTextUp)
        return;

    uint8_t* sayTextLineRef = scrollTextUp + kScrollTextUpSayTextLineRef;
    uint8_t* printConsoleCall = scrollTextUp + kScrollTextUpConsolePrintCall;
    if (sayTextLineRef[0] != 0x8B || sayTextLineRef[1] != 0x3D || printConsoleCall[-1] != 0xFF || printConsoleCall[0] != 0x15)
    {
        gEngfuncs.Con_DPrintf("ColorChatInConsolePatch: unknown client.so build, colored chat in console disabled\n");
        return;
    }

    uint32_t printConsoleAddress = (uint32_t)printConsoleCall;
    uint32_t sayTextLineAddress = *(uint32_t*)(sayTextLineRef + 2);
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

    g_GameConsoleNext->ColorPrintfWide(r, g, b, L"%ls", print_text.c_str());
}

static void ColorChatConsolePrint(char string[512])
{
    if (suppressLocalChatEcho)
        return;
    if (g_sayTextLine[0].m_textRanges.Count() != 0)
    {
        for (int rangeIndex = 0; rangeIndex < g_sayTextLine[0].m_textRanges.Count(); rangeIndex++)
        {
            TextRange* range = &g_sayTextLine[0].m_textRanges[rangeIndex];
            std::wstring print_text = std::wstring(&g_sayTextLine[0].m_line[range->start], range->end - range->start);

            if (g_GameConsoleNext)
            {
                PrintWithConsoleNext(range, print_text);
            }
            else
            {
                PrintWithConsole(range, print_text);
            }
        }

        if (V_strEndsWith(string, "\n"))
        {
            g_GameConsole->Printf("\n");
        }
    }
}
