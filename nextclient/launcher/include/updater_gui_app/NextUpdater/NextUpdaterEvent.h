#pragma once

#include <string>
#include <utils/bitmask.h>

enum class NextUpdaterState
{
    Idle,
    RestoringFromBackup,
    ClearingBackupFolder,
    RequestingFileList,
    GatheringFilesToUpdate,
    Backuping,
    OpeningFilesToInstall,
    Downloading,
    Installing,
    CanceledByUser,
    Done
};

namespace NextUpdaterEventFlags
{
    enum Value : uint8_t
    {
        StateChanged    = 1 << 0,
        ProgressChanged = 1 << 1,
        ErrorChanged    = 1 << 2
    };

    BITMASK_OPS(Value)
}

struct NextUpdaterEvent
{
    NextUpdaterState state;
    float state_progress;
    std::string error_description;
    NextUpdaterEventFlags::Value flags;

    explicit NextUpdaterEvent(NextUpdaterState state, float state_progress, std::string error_description, NextUpdaterEventFlags::Value flags) :
            state(state),
            state_progress(state_progress),
            error_description(std::move(error_description)),
            flags(flags)
    { }

    NextUpdaterEvent() = delete;
};