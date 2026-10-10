#pragma once

#include <ncl_math/color.h>

#include "hud/HudBase.h"
#include "hud/HudBaseHelper.h"
#include "hud/radar/markers/RadarBroadcast.h"
#include "hud/radar/render/text/RadarText.h"
#include "hud/radar/policy/SmokeRegistry.h"
#include "hud/radar/policy/Visibility.h"
#include "hud/radar/policy/RadarPolicy.h"
#include "hud/radar/render/RadarView.h"
#include "hud/radar/RadarFrame.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/map/layers/RadarLayerAnalysisJob.h"
#include "hud/radar/map/MapBspGeometry.h"
#include "hud/radar/integration/RadarScoreboard.h"
#include "hud/radar/layers/BackdropLayer.h"
#include "hud/radar/layers/PlayersLayer.h"
#include "hud/radar/layers/ObjectivesLayer.h"

class HudRadarAdvanced : public HudBase, public HudBaseHelper
{
private:
    FrameContext ctx_{};
    RadarScoreboard scoreboard_;

    // Declared before the members that hold references to them (visibility_, backdrop_,
    // objectives_).
    SmokeRegistry smoke_;
    RadarBroadcast broadcast_;
    RadarMapLayers map_layers_;
    RadarLayerAnalysisJob layer_analysis_job_;
    RadarLayerContext layer_context_;
    RadarLayerLifecycle layer_lifecycle_;
    RadarVisibility visibility_{smoke_};
    RadarPolicy policy_;
    RadarText text_;
    BackdropLayer backdrop_{smoke_};
    PlayersLayer players_{visibility_, policy_, broadcast_, text_};
    ObjectivesLayer objectives_{visibility_, policy_, broadcast_, text_};

    // Serverinfo changes only on a server "serverinfo" command, so hints are re-read on
    // this cadence rather than every frame.
    float last_hint_read_{};

    int flash_alpha_{};
    char layer_map_[64]{};
    char layer_correction_path_[128]{};
    float layer_analysis_ms_{};
    RadarMapPreparationDiagnostic layer_preparation_{};
    RadarMapMode map_mode_ = RadarMapMode::None;
    uint64_t frame_count_{};
    float layer_frame_time_{};
    bool has_layer_frame_time_{};

    static constexpr float kHintInterval = 1.0f;

public:
    explicit HudRadarAdvanced(nitroapi::NitroApiInterface* nitro_api);
    ~HudRadarAdvanced() override;

    void VidInit() override;
    void InitHUDData() override;
    void Reset() override;
    void Draw(float flTime) override;
    void PrepareHudTextZone();
    void UpdateScoreboard(bool hud_visible);

private:
    RadarBox ComputeBox(bool spectating);
    RadarTransform ComputeTransform(const RadarBox& box, const RadarShape& shape, bool scoreboard);
    bool ShouldDraw();
    void UpdateFrameContext(bool spectating, float flTime);
    void UpdatePolicy(float flTime);
    void OnPlayerRespawn(PlayersLayer::ResetReason reason = PlayersLayer::ResetReason::Hud, float previous_time = 0.0f, float time = 0.0f);
    void ResetMapLayers();
    void ReportLayerFailure(RadarMapMode mode, int layer);
    RadarMapStatus EnsureMapLayers(RadarMapMode mode);
    RadarMapStatus EnsureMapLayout(RadarMapMode mode, bool prepare_only = false);
    void UpdateLayerContext(bool spectating, float time);

    int ComputeFlashAlpha(ncl_math::Color& color); // 0 when not blinding; writes the flash color

public:
    // True only when this frame had no map source at all (no overview sprite, no runtime
    // render). The advanced radar draws first, so the classic one reads this to take over.
    static bool is_map_unavailable();
};
