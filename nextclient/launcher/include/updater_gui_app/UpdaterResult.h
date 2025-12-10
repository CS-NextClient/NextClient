#pragma once
#include "UpdaterDoneStatus.h"
#include "json_data/BranchEntry.h"

struct UpdaterResult
{
    UpdaterDoneStatus done_status{};
    std::vector<BranchEntry> branches{};
};
