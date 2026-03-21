#include "../engine.h"

int* p_cmd_argc;
char* (*p_cmd_argv)[80];

void Cbuf_InsertText(const char* text)
{
    eng()->Cbuf_InsertText.InvokeChained(text);
}

void Cmd_AddCommand(const char* cmd_name, xcommand_t function)
{
    eng()->Cmd_AddCommand.InvokeChained(cmd_name, function);
}

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
