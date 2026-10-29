# Radar layers acceptance cases

[Russian version](radar-layer-cases.ru.md) | [Procedure](radar-layers.md) | [Reference](../radar-layers.md)

## Automated results

Windows and Linux x86 Release verification on 2026-10-10 passed 472 client tests on each platform. The external BSP case is skipped without a map path; its separate batch passed all 25 standard maps and checked repeated-build determinism. Both client modules linked successfully. Regression tests cover bounded raised-floor contact, narrow free height intervals, stacked supports, sky-capped support exclusion, common retained bounds after hides, renderer bounds, and unsupported detail assignment to a lower plane. The Linux software OpenGL pass rendered 47 production Flat layer images from the same 25 BSP hashes without GL errors. These checks do not establish live engine output, Generated Textured pixels, movement reachability, or hardware frame times.

| Area | Automated coverage |
|---|---|
| Geometry/corrections | Owned adapter data, clipped UVs, stacked/partial/narrow supports, ramps, deterministic grouping, finite/half-open corrections, region precedence, retained automatic areas, manual conflicts, render-only box subtraction with preserved support queries and interpolated UVs |
| Selection/contact | Confirmation, jump/fall, exact landing, crouch, observer sources, vertical discontinuities, snapshot timing, moving-platform contact, bounded raised-floor contact including narrow free intervals and stacked supports |
| Markers | XYZ history, opacity/vertical indicators, rotation/edge placement, objective volumes, active-layer trail segments |
| Resources | Cache/frame budget, failed-resource handling, retained common bounds after filtering/hides, reset and empty-mesh bounds, renderer projection/runtime bounds, GL state restoration, lifecycle reset policy |
| Diagnostics/BSP/queries | Read-only and unavailable-owner dumps, malformed-file rejection, real BSP, remote nearest support, preferred ties, tolerant tiny-footprint boundaries, multicell volume clipping |

Fake GL tests verify calls/resources, not pixels. Source-traced callbacks do not establish live engine ordering. Runtime cases remain `not_run` until observed by the developer.

## Runtime cases

Perform applicable cases in modes 5 and 6, including expanded radar. Save dumps and screenshots/video using the procedure. A mismatch with the pass condition is a failure; record its exact trigger and actual result.

| Trigger | Observable pass condition | Failure record |
|---|---|---|
| de_dust2 routes across terrain rises and small platforms | One automatic layer remains selected across the route | Extra layers or local image changes; map/layers/assignment reasons |
| de_dust2 layer image compared with top | Continuous original contours and floor coverage in modes 5 and 6 | Stepped cutouts, missing edge strips, or interior gaps; paired images |
| cs_assault with the shipped correction, inside the warehouse and on its roof | One layer; warehouse interior visible; exterior coverage retained; roof markers stay on the main layer | Roof obscures interior, missing exterior geometry, or extra layer; correction path and paired images |
| cs_747 terminal lower floors and roof with the shipped correction | Lower floors retain visible paths; terminal roof at Z 384 is independently selectable and remains rendered | Roof covers the selected lower floor, lost roof geometry, or incorrect support selection; coordinates, layer dump, paired images |
| Small bridge or raised platform above a passage | Surrounding layer remains selected | Additional layer or marker dimming caused only by the small overlap; positions/dump |
| SCP routes on stacked floors with overlapping XY | Active group/image follows the intended supporting floor | Wrong support/image or map-specific grouping; heights/bounds/reasons |
| SCP without a layer correction or entry-point metadata | All free collision regions remain eligible; collision-blocked roof surfaces are absent from generated layer and ready top images; legitimate floor groups remain selectable | Covered ground, missing open/disconnected floors, changed group areas, or unavailable contact filter; exclusions, group dump, and paired images |
| adv_university staircase between adjacent floors | All 16 treads retain visible contours; layer follows the supporting floor | Missing stair strip or wrong layer; route, paired images, transitions |
| SCP analysis while entering or switching maps | Frames continue during worker analysis; only the current map is published | Freeze, stale layer, repeated preparation; capture_ms, analysis_ms, waiting_reason |
| Reset/video change, mode 5 to 6, or shutdown during analysis | Reset cancels stale work; 5/6 retains the request; shutdown completes without a frame pump | Hang, obsolete result, duplicate analysis; exact sequence and dump |
| Stairs and oscillation near a boundary | Confirmation after 0.15 s beyond margin; no boundary image oscillation | Early/repeated switch; active/candidate/since/reason |
| Jump, crouch, fall, exact landing | Jump/crouch retain layer; fall confirms lower; exact landing selects immediately | Wrong layer/delay; transition video/reason |
| Lift or moving platform through heights | Continuous support movement uses ordinary confirmation | Wrong support/repeated discontinuity; route/source/reason |
| Vertical teleport | Destination selects immediately | Old image/pending timer; before/after dump |
| TAB full map in modes 5/6 during play and observation | Current player layer fits within the common retained bounds of all layers at 120% size with at least 3 HUD pixels of inset; switching floors preserves center/scale; observed player determines the layer; other-floor indicators remain screen-vertical | Top image replaces the selected floor, wrong source, incorrect fit, or layer cache regenerated on TAB |
| Player data suppressed briefly, then longer than 150 ms | Frozen last authorized marker during the grace period, then one question mark per player; fade expires 4.5 seconds after loss; same-player return restores live marker | Flicker on a brief gap, duplicate marks, lifetime extended by missing frames, or another player clears the mark |
| Known player dies or disconnects while data is missing | Lost-data mark removed; death retains the normal death-mark rules | Stale question mark or disconnected-player death mark |
| Observer entity/mode switch | Entity modes use observed player; source change selects immediately | Local/stale target layer; source/active/reason |
| Free camera above geometry and outside bounds | Below then nearest fallback selects responsively | Stale/unassigned layer/pause; camera position/reason |
| Hole/partial upper footprint | Active coverage has priority; empty active regions show the nearest other layer, with lower layers dimmed and upper layers unchanged; pixels with no geometry remain transparent; common scale and XY alignment | Missing context, covered active floor, wrong shading, stretch or misalignment; images/bounds |
| Other-floor marker at rotating radar edge | Opacity multiplier 0.4; screen-vertical indicator close to the marker; composite fits shape; A/B arrows follow letter height; bomb arrows follow marker size in TAB | Wrong opacity, distant/rotated/clipped arrow, or oversized TAB bomb arrow; screenshot/height |
| Player/death/sound/history/trail floor changes | Recorded XYZ retained; only active-layer endpoint pairs form trail segments | Lost height/bridge between floors; history sequence |
| Objective volumes spanning floors | Full opacity when any volume intersects active support; correct style otherwise | Center-only result despite intersection; volume bounds/active |
| Other-floor smoke | Recorded XYZ determines relative-floor style | Wrong opacity/arrow; smoke origin/active |
| Scoreboard and radar text, scoped/unscoped | Visibility and text order remain correct | Missing/duplicate/covered text; screenshots |
| Map smoothing on/off in generated modes, at rotating/fitted/follow scales | Alpha contours gain subpixel coverage; opaque interior details retain sharpness; toggle preserves cached images, analysis, and GL state | Interior blur, dark edge halo, missing contour pixels, regenerated images, or mask/flash artifacts |
| Map shifting on/off at edges and corners, both shapes and rotated views | Follow view retains a 30% radius inset for round and 20% half-width inset for square map edges without changing zoom; reference marker remains inside the rim; tiles and markers stay aligned; Apply/discard/defaults include the toggle | Missing reference marker, scale change, marker/image drift, jump at the clamping threshold, or incorrect toggle restoration |
| Video reset | Valid new images, frame budget respected, stock HUD GL state usable | Stale texture/broken HUD/resource failure; cache/settings |
| Same-name reconnect, then another map | Geometry/images/selection reset; current map in dump | Old map/image/IDs; map/bounds/cache before/after |
| Client clock correction up to 10 ms | Marker fades, reports, trails and selection retained | Group fade-in, Radar reports cleared or history restarted |
| Respawn, round reset, client time rewind beyond 10 ms | Selection/history reset; geometry/images/failure state retained until resource reset | Respawn-only reanalysis/retry or stale history; analysis/cache/source |
| Reproducible generation failure, then map/video/mode reset | Usable whole-map fallback, no per-frame retry; reset permits retry | Blank radar/repeated attempts/no retry; state/cache/trigger |
| Active layer with floor height changes and tall walls | Height tint spans the active support range in both modes; full walls remain visible; adjacent-floor height does not affect tint | Compressed floor contrast, clipped details, or brightness affected by another floor |
| Pending analysis or layer material in modes 5/6 | Advanced radar shows the matching top image and ordinary markers until the layer is ready; top waiting keeps the advanced frame | Stock radar, missing markers, repeated top generation, or discarded layer cache |
| Pending GL error before map generation | Successful allocation still produces the map; errors generated by the pass still cause a diagnosed failure | False texture-storage failure or a hidden allocation/render error |
| Repeated `radar_layers_dump` | State reported without analysis/image generation | Command-only analysis/cache changes; consecutive output |

Automatic structure and determinism are verified on the measured fixture. Correspondence to intended game floors and visual quality require the developer's map-specific runtime assessment.
