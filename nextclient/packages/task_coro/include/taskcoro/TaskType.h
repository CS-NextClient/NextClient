#pragma once

namespace taskcoro
{
    enum class TaskType
    {
        ThreadPool,
        NewThread,
        MainThread,
        IO,
    };
}
