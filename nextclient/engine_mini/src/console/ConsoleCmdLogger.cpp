#include "../engine.h"

#include <nitro_utils/string_utils.h>

#include "ConsoleCmdLogger.h"

const int kLogR = 237;
const int kLogG = 63;
const int kLogB = 40;

void ConsoleCmdLogger::LogCommand(const std::string& command, const std::string& value, LogCommandType type)
{
    if (type == LogCommandType::AllowServerCommand)
        return;

    std::string cmd_copy = command;
    bool cmd_modified = ReplaceInconvenientSymbols(cmd_copy);

    switch (type)
    {
        case LogCommandType::BlockedAllCommand:
            g_GameConsoleNext->ColorPrintf(kLogR, kLogG, kLogB, "Blocked server cmd: ");
            break;

        case LogCommandType::BlockedDirectorCommand:
            g_GameConsoleNext->ColorPrintf(kLogR, kLogG, kLogB, "Blocked director cmd: ");
            break;

        case LogCommandType::BlockedStufftextComamnd:
            g_GameConsoleNext->ColorPrintf(kLogR, kLogG, kLogB, "Blocked stufftext cmd: ");
            break;

        case LogCommandType::BlockedBanner:
            g_GameConsoleNext->ColorPrintf(kLogR, kLogG, kLogB, "Blocked director banner");
            break;
    }

    if (type != LogCommandType::BlockedBanner)
        g_GameConsoleNext->PrintfEx("%s", cmd_copy.c_str());

    if (!value.empty())
    {
        g_GameConsoleNext->ColorPrintf(kLogR, kLogG, kLogB, " value: ");
        g_GameConsoleNext->PrintfEx("%s", value.c_str());
    }

    g_GameConsoleNext->PrintfEx("\n");

    if (cmd_modified)
        g_GameConsoleNext->ColorPrintf(250, 163, 50, "(line breaks and tabulation are displayed as \\n and \\t, respectively)\n");
}

bool ConsoleCmdLogger::ReplaceInconvenientSymbols(std::string& command)
{
    bool modified;

    modified = nitro_utils::replace_all(command, "\n", "\\n");
    modified = nitro_utils::replace_all(command, "\t", "\\t") || modified;

    return modified;
}
