#pragma once

namespace taskcoro
{
    enum class TaskType
    {
        Regular,
        NewThread,
        MainThread,
        IO,
    };
}
