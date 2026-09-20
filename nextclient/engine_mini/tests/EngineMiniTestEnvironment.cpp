#include <gtest/gtest.h>

#include <easylogging++.h>

namespace
{
    // engine_mini logs through a logger the engine registers at start-up, which a test run never reaches
    class EngineMiniLoggerEnvironment : public testing::Environment
    {
    public:
        void SetUp() override
        {
            el::Loggers::getLogger(ELPP_DEFAULT_LOGGER);
        }
    };

    const testing::Environment* const g_LoggerEnvironment = testing::AddGlobalTestEnvironment(new EngineMiniLoggerEnvironment);
} // namespace
