#pragma once

#include <goldsrc/fwd.h>

namespace goldsrc
{
    using PEngineMsg = EngineMsg*;
    using pfnEngineMessage = void (*)();

    inline constexpr char OFF_SVC_MESSAGES_P[] = "End of List";

    struct EngineMsg
    {
        EngineMsg() = delete;
        EngineMsg(const EngineMsg&) = delete;
        EngineMsg& operator=(const EngineMsg&) = delete;

        int number;
        char* name;
        pfnEngineMessage pfn;
    };
} // namespace goldsrc
