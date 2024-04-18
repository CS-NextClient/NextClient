#pragma once

#include "../hlsdk.h"

//
// cmd.cpp
//
extern int* p_cmd_argc;
extern char* (*p_cmd_argv)[80];

int Cmd_Argc();
const char* Cmd_Argv(int arg);

//
// custom.cpp
//
qboolean COM_CreateCustomization(customization_t* pListHead, resource_t* pResource, int playernumber, int flags, customization_t** pCustomization, int* nLumps);
