#pragma once

#include <goldsrc/engine/user_msg.h>

namespace nitroapi
{
    goldsrc::PEngineMsg FindEngineMsgByName(goldsrc::PEngineMsg enigne_msg_start, const char* msg_name);
}