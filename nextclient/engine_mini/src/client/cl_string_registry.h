#pragma once

#include <cstdint>

class ClientStringRegistry;

ClientStringRegistry& CL_GetStringRegistry();
void CL_StringRegistryInit();
void CL_StringRegistryShutdown();
void CL_StringRegistryClear();
const char* CL_StringRegistryGetString(uint16_t string_id);
