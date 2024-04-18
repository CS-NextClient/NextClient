#include "CmdLoggerAggregator.h"

void CmdLoggerAggregator::LogCommand(const std::string& command, const std::string& value, LogCommandType type)
{
    for (auto& logger : loggers_)
        logger->LogCommand(command, value, type);
}

bool CmdLoggerAggregator::AddLogger(CommandLoggerInterface* logger)
{
    if (std::find(loggers_.cbegin(), loggers_.cend(), logger) != loggers_.cend())
        return false;

    loggers_.push_back(logger);
    return true;
}

bool CmdLoggerAggregator::RemoveLogger(CommandLoggerInterface* logger)
{
    auto logger_it = std::find(loggers_.cbegin(), loggers_.cend(), logger);
    if (logger_it == loggers_.cend())
        return false;

    loggers_.erase(logger_it);
    return true;
}

void CmdLoggerAggregator::RemoveAllLoggers()
{
    loggers_.clear();
}
