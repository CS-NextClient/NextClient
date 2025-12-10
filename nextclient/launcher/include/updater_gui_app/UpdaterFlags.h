#pragma once
#include <utils/bitmask.h>

enum class UpdaterFlags
{
    None         = 0,
    Updater      = 1 << 0,
};
BITMASK_OPS(UpdaterFlags)
