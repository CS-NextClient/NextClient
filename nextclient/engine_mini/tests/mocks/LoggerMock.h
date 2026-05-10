#pragma once

#undef Assert

#include <gmock/gmock.h>
#include <next_engine_mini/CommandLoggerInterface.h>

class LoggerMock : public CommandLoggerInterface
{
public:
    MOCK_METHOD(void, LogCommand, (const char* command, const char* value, LogCommandType type), (override));
};
