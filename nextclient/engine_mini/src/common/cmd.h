#pragma once

#include <goldsrc/engine/cmd.h>

void Cbuf_InsertText(const char* text);
void Cmd_AddCommand(const char* cmd_name, goldsrc::xcommand_t function);
