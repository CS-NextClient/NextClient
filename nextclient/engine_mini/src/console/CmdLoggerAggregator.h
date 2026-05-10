#pragma once
#include <vector>

#include <next_engine_mini/CommandLoggerInterface.h>

class CmdLoggerAggregator : public CommandLoggerInterface
{
    std::vector<CommandLoggerInterface*> loggers_;

public:
    CmdLoggerAggregator() = default;
    CmdLoggerAggregator(CmdLoggerAggregator& other) = delete;
    CmdLoggerAggregator& operator=(CmdLoggerAggregator& other) = delete;

    void LogCommand(const char* command, const char* value, LogCommandType type) override;

    bool AddLogger(CommandLoggerInterface* logger);
    bool RemoveLogger(CommandLoggerInterface* logger);
    void RemoveAllLoggers();
};
