#include "../engine.h"

int* p_cmd_argc;
char* (*p_cmd_argv)[80];

int Cmd_Argc()
{
    p_g_engdstAddrs->Cmd_Argc();
    return *p_cmd_argc;
}

const char* Cmd_Argv(int arg)
{
    p_g_engdstAddrs->Cmd_Argv(&arg);

    if (arg < *p_cmd_argc)
        return (*p_cmd_argv)[arg];

    return "";
}
