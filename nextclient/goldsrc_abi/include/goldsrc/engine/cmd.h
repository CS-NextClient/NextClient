#pragma once

#include <goldsrc/fwd.h>

namespace goldsrc
{
    using xcommand_t = void (*)();

    inline constexpr int COM_MAX_CMD_LINE = 256;
    inline constexpr int NUM_SAFE_ARGVS = 7;

    enum cmd_source_t
    {
        src_client = 0,
        src_command = 1
    };
} // namespace goldsrc
