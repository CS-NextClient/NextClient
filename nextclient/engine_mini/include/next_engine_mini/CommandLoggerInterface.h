#pragma once

#include <string>

enum class LogCommandType
{
    AllowServerCommand = 0,
    BlockedAllCommand = 1,
    BlockedDirectorCommand = 2,
    BlockedStufftextComamnd = 3,
    BlockedBanner = 4
};

class CommandLoggerInterface
{
public:
    virtual ~CommandLoggerInterface() = default;

    virtual void LogCommand(const std::string& command, const std::string& value, LogCommandType type) = 0;
};