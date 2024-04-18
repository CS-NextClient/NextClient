#pragma once

#include <next_engine_mini/CommandLoggerInterface.h>

class ConsoleCmdLogger : public CommandLoggerInterface
{
public:
    void LogCommand(const std::string& command, const std::string& value, LogCommandType type) override;

    static bool ReplaceInconvenientSymbols(std::string& command);
};
