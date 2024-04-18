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

    void LogCommand(const std::string& command, const std::string& value, LogCommandType type) override;

    bool AddLogger(CommandLoggerInterface* logger);
    bool RemoveLogger(CommandLoggerInterface* logger);
    void RemoveAllLoggers();
};
