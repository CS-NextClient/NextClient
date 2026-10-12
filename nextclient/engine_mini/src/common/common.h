#pragma once

#include <goldsrc/engine/resource.h>
#include <goldsrc/types.h>

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
goldsrc::qboolean COM_CreateCustomization(goldsrc::customization_t* pListHead,
                                          goldsrc::resource_t* pResource,
                                          int playernumber,
                                          int flags,
                                          goldsrc::customization_t** pCustomization,
                                          int* nLumps);
