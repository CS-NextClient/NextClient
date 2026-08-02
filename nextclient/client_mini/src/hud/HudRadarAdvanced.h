#pragma once

#include <ncl_math/color.h>

#include "HudBase.h"
#include "HudBaseHelper.h"
#include "radar/RadarBroadcast.h"
#include "radar/RadarText.h"
#include "radar/SmokeRegistry.h"
#include "radar/Visibility.h"
#include "radar/RadarPolicy.h"
#include "radar/RadarView.h"
#include "radar/RadarFrame.h"
#include "radar/layers/BackdropLayer.h"
#include "radar/layers/PlayersLayer.h"
#include "radar/layers/ObjectivesLayer.h"

class HudRadarAdvanced : public HudBase, public HudBaseHelper
{
private:
    FrameContext ctx_{};

    // Declared before the members that hold references to them (visibility_, backdrop_,
    // objectives_).
    SmokeRegistry smoke_;
    RadarBroadcast broadcast_;
    RadarVisibility visibility_{smoke_};
    RadarPolicy policy_;
    RadarText text_;
    BackdropLayer backdrop_{smoke_};
    PlayersLayer players_{visibility_, policy_, text_};
    ObjectivesLayer objectives_{visibility_, policy_, broadcast_, text_};

    // Serverinfo changes only on a server "serverinfo" command, so hints are re-read on
    // this cadence rather than every frame.
    float last_hint_read_{};
    static constexpr float kHintInterval = 1.0f;

    int flash_alpha_{};

public:
    explicit HudRadarAdvanced(nitroapi::NitroApiInterface* nitro_api);
    ~HudRadarAdvanced() override;

    void VidInit() override;
    void InitHUDData() override;
    void Reset() override;
    void Draw(float flTime) override;

    // True only when this frame had no map source at all (no overview sprite, no runtime
    // render). The advanced radar draws first, so the classic one reads this to take over.
    static bool is_map_unavailable();

private:
    RadarBox ComputeBox(bool spectating);
    RadarTransform ComputeTransform(const RadarBox& box, const RadarShape& shape);
    bool ShouldDraw();
    void UpdateFrameContext(bool spectating, float flTime);
    void UpdatePolicy(float flTime);
    void OnPlayerRespawn();

    int ComputeFlashAlpha(ncl_math::Color& color); // 0 when not blinding; writes the flash colour
};
