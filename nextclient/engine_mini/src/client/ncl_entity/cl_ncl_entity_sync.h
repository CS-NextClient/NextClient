#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "INclEntityHandler.h"

void CL_NclEntitySyncInit();
void CL_NclEntitySyncShutdown();
void CL_NclEntitySyncClear();
void CL_NclEntitySyncRegisterType(uint8_t type_id, std::unique_ptr<INclEntityHandler> handler);
INclEntityHandler* CL_NclEntitySyncGetHandler(uint8_t type_id);
void CL_NclEntitySyncCollectDebugInfo(std::vector<std::string>& out_lines);
