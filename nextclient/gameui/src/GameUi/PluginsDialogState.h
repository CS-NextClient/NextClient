#pragma once
#include <string>
#include <vector>

struct PluginSelection
{
    std::string file;
    bool enabled;
    bool operator==(const PluginSelection&) const = default;
};

inline bool PluginCanConfirmSelection(
    const std::vector<PluginSelection>& initial,
    const std::vector<PluginSelection>& current,
    bool recoveryPending
)
{
    return recoveryPending || initial != current;
}

inline const char* PluginStatusToken(
    const std::vector<PluginSelection>& initial,
    const std::vector<PluginSelection>& current,
    size_t index,
    bool running,
    bool blocked,
    bool safeMode
)
{
    if (blocked && !running)
        return "#NextPlugins_Blocked";

    const auto& selection = current[index];
    size_t previous = 0;
    while (previous < initial.size() && initial[previous].file != selection.file)
        ++previous;
    bool changed = previous == initial.size() || initial[previous].enabled != selection.enabled;
    bool reordered = previous != index && (selection.enabled || running);
    if (changed || reordered)
        return "#NextPlugins_PendingRestart";
    if (running)
        return "#NextPlugins_Active";
    if (selection.enabled && !safeMode)
        return "#NextPlugins_Blocked";
    return "#NextPlugins_Disabled";
}
