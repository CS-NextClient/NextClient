#pragma once

#include <nitro_utils/config/ConfigProviderInterface.h>
#include <next_engine_mini/CommandLoggerInterface.h>

void PROTECTOR_Init(std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider);
void PROTECTOR_Shutdown();
bool PROTECTOR_AddCmdLogger(CommandLoggerInterface* logger);
bool PROTECTOR_RemoveCmdLogger(CommandLoggerInterface* logger);
