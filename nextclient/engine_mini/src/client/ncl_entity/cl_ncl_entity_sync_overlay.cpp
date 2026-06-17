#include "engine.h"
#include "cl_ncl_entity_sync_overlay.h"

#include <vector>
#include <string>

#include "cl_ncl_entity_sync.h"

namespace
{
    cvar_t* g_cvar_debug_entity_sync = nullptr;
    std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_unsubs;

    constexpr int OVERLAY_X = 120;
    constexpr int OVERLAY_Y = 10;
    constexpr int LINE_HEIGHT = 17;

    void DrawOverlay()
    {
        if (!g_cvar_debug_entity_sync || g_cvar_debug_entity_sync->value == 0.0f)
        {
            return;
        }

        if (cls == nullptr || cls->state != ca_active)
        {
            return;
        }

        static std::vector<std::string> lines;
        lines.clear();
        lines.push_back("=== Entity Sync ===");
        CL_NclEntitySyncCollectDebugInfo(lines);

        if (lines.size() <= 1)
        {
            lines.push_back("  (no entities)");
        }

        int y = OVERLAY_Y;

        gEngfuncs.pfnFillRGBA(OVERLAY_X - 2, OVERLAY_Y - 2, 300, (int)lines.size() * LINE_HEIGHT + 4, 0, 0, 0, 120);

        gEngfuncs.pfnDrawSetTextColor(1.0f, 1.0f, 0.0f);
        for (const auto& line : lines)
        {
            gEngfuncs.pfnDrawConsoleString(OVERLAY_X, y, const_cast<char*>(line.c_str()));
            y += LINE_HEIGHT;
        }
    }

    int HUD_RedrawHook(float flTime, int iIntermission, nitroapi::NextHandlerInterface<int, float, int>* next)
    {
        int result = next->Invoke(flTime, iIntermission);
        DrawOverlay();
        return result;
    }
} // namespace

void CL_NclEntitySyncOverlayInit()
{
#ifdef NDEBUG
    return;
#endif

    g_cvar_debug_entity_sync = gEngfuncs.pfnRegisterVariable("ncl_debug_entity_sync", "0", FCVAR_ARCHIVE);
    g_unsubs.emplace_back(client()->HUD_Redraw |= HUD_RedrawHook);
}

void CL_NclEntitySyncOverlayShutdown()
{
#ifdef NDEBUG
    return;
#endif

    for (auto& unsubscriber : g_unsubs)
    {
        unsubscriber->Unsubscribe();
    }
    g_unsubs.clear();
    g_cvar_debug_entity_sync = nullptr;
}
