#include "ObjectivesLayer.h"

#include <algorithm>
#include <cmath>

#include "main.h"

#include "hud/radar/RadarConfig.h"
#include "hud/radar/RadarDraw.h"
#include "hud/radar/RadarLabel.h"
#include "hud/radar/RadarPalette.h"
#include "hud/radar/RadarText.h"

namespace
{
    // Fraction of box.marker_px. Sites get no size cvar: two gold glyphs, not ten nicknames.
    constexpr float kSiteHeightScale = 0.093f;
    constexpr int kSiteMinPx = 9;
    constexpr int kSiteMaxPx = 28;

    // Layout cache slots after the player range (see RadarText).
    constexpr int kSiteSlotBase = 70;

    // Marker sizes as a fraction of box.marker_px.
    constexpr float kHostageDotScale = 0.021f;
    constexpr float kBombDotScale = 0.0273f;
} // namespace

ObjectivesLayer::ObjectivesLayer(
    RadarVisibility& visibility,
    const RadarPolicy& policy,
    const RadarBroadcast& broadcast,
    RadarText& text
) :
    text_(text),
    bomb_(visibility, policy, broadcast),
    hostages_(visibility, policy, broadcast)
{
}

void ObjectivesLayer::OnPlayerRespawn()
{
    bomb_.Reset();
    hostages_.Reset();
}

void ObjectivesLayer::OnVidInit()
{
    objective_entities_.OnVidInit();
    hostages_.OnLevelChange();
}

const RadarMapObjectives& ObjectivesLayer::get_map_objectives()
{
    objectives_.EnsureLoaded();
    return objectives_;
}

void ObjectivesLayer::CollectEntities(const RadarFrame& frame)
{
    objective_entities_.CollectEntities(frame);
}

void ObjectivesLayer::Draw(const RadarFrame& frame)
{
    DrawBomb(frame);
    DrawSites(frame);
    DrawHostages(frame);    
}

void ObjectivesLayer::DrawBomb(const RadarFrame& frame)
{
    if (!frame.ctx.local)
    {
        return;
    }

    bomb_.Draw(frame, objective_entities_, std::max(3, (int)(frame.box.marker_px * kBombDotScale)));
}

void ObjectivesLayer::DrawHostages(const RadarFrame& frame)
{
    if (!frame.ctx.local)
    {
        return;
    }

    hostages_.Draw(frame, objective_entities_, std::max(2, (int)(frame.box.marker_px * kHostageDotScale)));
}

void ObjectivesLayer::DrawSites(const RadarFrame& frame)
{
    objectives_.EnsureLoaded();

    int site_px = ComputeSiteHeightPx(frame);
    bool batched = text_.CheckAvailable(site_px);

    float flash_fade = RadarLabel_ComputeFlashFadeFor(batched, frame.flash_alpha);

    if (flash_fade <= 0.0f)
    {
        return;
    }

    if (batched)
    {
        RadarDraw_BeginPrims();
    }

    // Fall back to the engine font rather than dropping the letters; a single glyph costs nothing.
    int fallback_h = 0;

    if (!batched)
    {
        int probe_w = 0;
        gEngfuncs.pfnDrawConsoleStringLen("x", &probe_w, &fallback_h);
    }

    for (int i = 0; i < objectives_.get_site_count(); i++)
    {
        const BombSite& site = objectives_.get_site(i);

        ncl_math::Vector2 s = frame.shape.Clamp(6, frame.t.WorldToScreen({site.pos.x, site.pos.y}));

        if (batched)
        {
            ncl_math::Color gold = RadarPalette::kGold.WithAlpha((int)(255 * flash_fade));
            text_.DrawCentered(kSiteSlotBase + i, site.label, {s.x, s.y - site_px * 0.5f}, site_px, gold);
        }
        else
        {
            int tw = 0, th = 0;
            gEngfuncs.pfnDrawConsoleStringLen(site.label, &tw, &th);
            RadarLabel_Draw(site.label, (int)s.x - tw / 2, (int)s.y - fallback_h / 2, RadarPalette::kGold, frame.flash_alpha);
        }
    }

    if (batched)
    {
        RadarDraw_EndPrims();
    }
}

int ObjectivesLayer::ComputeSiteHeightPx(const RadarFrame& frame)
{
    // Original site letters are the engine's standard scheme font; the ladder is a stopgap until it resolves.
    int standard = text_.ResolveStandardPx();

    if (standard > 0)
    {
        return standard;
    }

    return std::clamp((int)std::lround(frame.box.marker_px * kSiteHeightScale), kSiteMinPx, kSiteMaxPx);
}
