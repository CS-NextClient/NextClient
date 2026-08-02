#pragma once

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

#include "HudBase.h"
#include "HudBaseHelper.h"
#include "radar/Overview.h"
#include "radar/Visibility.h"
#include "radar/RadarPolicy.h"
#include "radar/RadarView.h"
#include "radar/RadarFrame.h"
#include "radar/layers/PlayersLayer.h"
#include "radar/layers/ObjectivesLayer.h"

class HudRadarAdvanced : public HudBase, public HudBaseHelper
{
private:
    FrameContext ctx_ = {};

    RadarOverview overview_;
    RadarVisibility visibility_;
    RadarPolicy policy_;
    PlayersLayer players_{visibility_, policy_};
    ObjectivesLayer objectives_{visibility_, policy_};

    // Serverinfo only changes when the server issues a "serverinfo" command, so the
    // mode hints are re-read on this cadence rather than every frame.
    float last_hint_read_ = 0.0f;
    static constexpr float kHintInterval = 1.0f;

    // Overview sprite whose frame textures already have edge clamp set (wrap state
    // persists per texture object, so it is applied once per load, not per tile).
    model_t* clamped_sprite_ = nullptr;

    int flash_alpha_ = 0; // computed each frame; gates the name labels too

public:
    explicit HudRadarAdvanced(nitroapi::NitroApiInterface* nitro_api);

    void VidInit() override;
    void InitHUDData() override;
    void Reset() override;
    void Draw(float flTime) override;

private:
    RadarBox ComputeBox();
    RadarTransform ComputeTransform(const RadarBox& box);
    bool ShouldDraw(bool spectating);
    void UpdateFrameContext(bool spectating, float flTime);
    // Feeds RadarPolicy the observations behind its decisions: which teammates the
    // server still broadcasts, and how the bomb message behaves. Runs on the context
    // UpdateFrameContext just built, before the visibility pass consumes the answer.
    void UpdatePolicy(float flTime);
    void ResetRound();

    void DrawBackground(const RadarBox& box);
    void DrawRuntimeMap(const RadarBox& box, const RadarTransform& t);
    void DrawOverviewTiles(const RadarBox& box, const RadarTransform& t);
    void DrawBorder(const RadarBox& box);
    int FlashAlpha(ncl_math::Color& color); // current blinding-flash alpha (0 when not blinding) and its colour
    void DrawSmoke(const RadarBox& box, const RadarTransform& t, float time);
};
