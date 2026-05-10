#pragma once

enum class LogCommandType
{
    AllowServerCommand = 0,
    BlockedAllCommand = 1,
    BlockedDirectorCommand = 2,
    BlockedStufftextCommand = 3,
    BlockedBanner = 4,
    BlockedDirectorCommandByEngine = 5,
    BlockedStufftextCommandByEngine = 6,
};

class CommandLoggerInterface
{
public:
    virtual ~CommandLoggerInterface() = default;

    virtual void LogCommand(const char* command, const char* value, LogCommandType type) = 0;
};