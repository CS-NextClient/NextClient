#pragma once
#include <string>
#include <cstdint>

void CL_StringRegistryInit();
void CL_StringRegistryShutdown();
void CL_StringRegistryClear();
const char* CL_StringRegistryGetString(uint16_t string_id);
