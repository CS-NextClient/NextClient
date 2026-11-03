#pragma once

namespace nitroapi
{
    struct StudioAnimationData;

    void StudioAnimation_InitializeAccess(StudioAnimationData& data);
    bool StudioAnimation_IsResolved(const StudioAnimationData& data);
} // namespace nitroapi
