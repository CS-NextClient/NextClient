#include <easylogging++.h>
#include "console/console.h"

INITIALIZE_EASYLOGGINGPP

class EngineLogAppender : public el::LogDispatchCallback
{
protected:
    void handle(const el::LogDispatchData* data) noexcept override
    {
        using namespace el;
        using namespace el::base::type;

        if (!con_initialized)
        {
            return;
        }

        std::string log_message = data->logMessage()->message();
        Level message_level = data->logMessage()->level();

        if (message_level == Level::Trace || message_level == Level::Debug || message_level == Level::Verbose)
        {
            Con_DPrintf(ConLogType::Info, "%s\n", log_message.c_str());
        }
        else if (message_level == Level::Error)
        {
            Con_Printf("[ERROR] %s\n", log_message.c_str());
        }
        else
        {
            Con_Printf("%s\n", log_message.c_str());
        }
    }
};

void ConfigureEngineElppLogger()
{
    el::Loggers::reconfigureAllLoggers(el::ConfigurationType::ToStandardOutput, "false");
    el::Loggers::reconfigureAllLoggers(el::ConfigurationType::ToFile, "false");

    el::Configurations config;
    config.setToDefault();

    config.set(el::Level::Global, el::ConfigurationType::Enabled, "true");
    config.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
    config.set(el::Level::Global, el::ConfigurationType::ToFile, "false");

    el::Loggers::reconfigureLogger(ELPP_DEFAULT_LOGGER, config);

    el::Helpers::installLogDispatchCallback<EngineLogAppender>("EngineLogAppender");
}
