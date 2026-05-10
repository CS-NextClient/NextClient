#include "../engine.h"

#include <nitro_utils/string_utils.h>

#include "ConsoleCmdLogger.h"

constexpr int kLogR = 237;
constexpr int kLogG = 63;
constexpr int kLogB = 40;

void ConsoleCmdLogger::LogCommand(const char* command, const char* value, LogCommandType type)
{
    if (type == LogCommandType::AllowServerCommand)
        return;

    std::string cmd_copy = command;
    bool cmd_modified = ReplaceInconvenientSymbols(cmd_copy);

    switch (type)
    {
        case LogCommandType::BlockedAllCommand:
            g_GameConsoleNext->ColorPrintfWide(kLogR, kLogG, kLogB, L"Blocked server cmd: ");
            break;

        case LogCommandType::BlockedDirectorCommand:
            g_GameConsoleNext->ColorPrintfWide(kLogR, kLogG, kLogB, L"Blocked director cmd: ");
            break;
  
        case LogCommandType::BlockedStufftextCommand:
            g_GameConsoleNext->ColorPrintfWide(kLogR, kLogG, kLogB, L"Blocked stufftext cmd: ");
            break;

        case LogCommandType::BlockedBanner:
            g_GameConsoleNext->ColorPrintfWide(kLogR, kLogG, kLogB, L"Blocked director banner");
            break;

        case LogCommandType::BlockedDirectorCommandByEngine:
            g_GameConsoleNext->ColorPrintfWide(kLogR, kLogG, kLogB, L"Blocked director cmd (by engine): ");
            break;

        case LogCommandType::BlockedStufftextCommandByEngine:
            g_GameConsoleNext->ColorPrintfWide(kLogR, kLogG, kLogB, L"Blocked stufftext cmd (by engine): ");
            break;
    }

    if (type != LogCommandType::BlockedBanner)
        g_GameConsoleNext->PrintfEx("%s", cmd_copy.c_str());

    if (value && value[0] != '\0')
    {
        g_GameConsoleNext->ColorPrintfWide(kLogR, kLogG, kLogB, L" value: ");
        g_GameConsoleNext->PrintfEx("%s", value);
    }

    g_GameConsoleNext->PrintfExWide(L"\n");

    if (cmd_modified)
        g_GameConsoleNext->ColorPrintfWide(250, 163, 50, L"(line breaks and tabulation are displayed as \\n and \\t, respectively)\n");
}

bool ConsoleCmdLogger::ReplaceInconvenientSymbols(std::string& command)
{
    bool modified;

    modified = nitro_utils::replace_all(command, "\n", "\\n");
    modified = nitro_utils::replace_all(command, "\t", "\\t") || modified;

    return modified;
}
