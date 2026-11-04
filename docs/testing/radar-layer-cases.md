# Radar layers acceptance cases

[Russian version](radar-layer-cases.ru.md) | [Procedure](radar-layers.md) | [Reference](../radar-layers.md)

## Scope and preparation

These cases define observable acceptance criteria for generated radar layers. Prepare the client, maps, captures, and automated checks using the [verification procedure](radar-layers.md). Record each execution separately with its source/build identity and result; the cases remain applicable across runs.

Automated geometry and GL checks complement these observations. They do not establish live engine ordering, movement reachability, or hardware frame times.

## Runtime cases

Perform applicable cases in modes 5 and 6, including expanded radar. Save dumps and screenshots/video using the procedure. A mismatch with the pass condition is a failure; record its exact trigger and actual result.

| Action | Expected result | What to save on failure |
|---|---|---|
| de_dust2 routes across terrain rises and small platforms | One automatic layer remains selected across the route | Extra layers or local image changes; map/layers/assignment reasons |
| de_dust2 layer image compared with top | Continuous original contours and floor coverage in modes 5 and 6 | Stepped cutouts, missing edge strips, or interior gaps; paired images |
| cs_assault with the shipped correction, inside the warehouse and on its roof | One layer; warehouse interior visible; exterior coverage retained; roof markers stay on the main layer | Roof obscures interior, missing exterior geometry, or extra layer; correction path and paired images |
| cs_747 terminal lower floors and roof with the shipped correction | Lower floors retain visible paths; terminal roof at Z 384 is independently selectable and remains rendered | Roof covers the selected lower floor, lost roof geometry, or the wrong floor is selected; coordinates, layer dump, paired images |
| Small bridge or raised platform above a passage | Surrounding layer remains selected | Additional layer or marker dimming caused only by the small overlap; positions/dump |
| SCP routes on stacked floors with overlapping XY | The radar shows the floor on which the player stands | The radar shows the wrong floor or splits the map into incorrect layers; save the layer dump and player coordinates |
| SCP without a layer correction or entry-point metadata | Layer and whole-map images omit roofs blocked from player access by collision. Open areas and separate rooms remain visible. As the player moves, the radar selects the corresponding floor | A roof covers a floor, open areas or separate rooms disappear, or the wrong floor is selected; save the layer dump and paired images |
| adv_university staircase between adjacent floors | All 16 treads retain visible contours; the radar shows the floor on which the player stands | Missing stair strip or wrong layer; route, paired images, transitions |
| SCP analysis while entering or switching maps | Frames continue during background analysis. The radar shows only the map to which the client is currently connected | Freeze, stale layer, repeated preparation; capture_ms, analysis_ms, waiting_reason |
| Reset/video change, mode 5 to 6, or shutdown during analysis | The radar does not show stale data after a map or video reset. Switching between modes 5 and 6 preserves the analysis. The client exits without hanging | Hang, obsolete result, duplicate analysis; exact sequence and dump |
| Stairs and oscillation near a boundary | Confirmation after 0.15 s beyond margin; no boundary image oscillation | Early/repeated switch; active/candidate/since/reason |
| Jump, crouch, fall, exact landing | Jump/crouch retain layer; fall confirms lower; exact landing selects immediately | Wrong layer/delay; transition video/reason |
| Lift or moving platform through heights | While the elevator moves, layer changes use the configured confirmation delay. Elevator movement is not treated as teleportation | Wrong floor or repeated detection of movement as teleportation; route/source/reason |
| Vertical teleport | Destination selects immediately | Old image/pending timer; before/after dump |
| TAB full map in modes 5/6 during play and observation | Current player layer fits within the common retained bounds of all layers at 120% size with at least 3 HUD pixels of inset; switching floors preserves center/scale; observed player determines the layer; other-floor indicators remain screen-vertical | Top image replaces the selected floor, wrong source, incorrect fit, or layer cache regenerated on TAB |
| Player data suppressed briefly, then longer than 150 ms | Frozen last authorized marker during the grace period, then one question mark per player; fade expires 4.5 seconds after loss; same-player return restores live marker | Flicker on a brief gap, duplicate marks, lifetime extended by missing frames, or another player clears the mark |
| Known player dies or disconnects while data is missing | Lost-data mark removed; death retains the normal death-mark rules | Stale question mark or disconnected-player death mark |
| Switch the observed player or observer mode | When observing a player, the radar shows that player’s floor. Switching players selects the new floor immediately | Local/stale target layer; source/active/reason |
| Free camera above geometry and outside bounds | The radar promptly shows the floor below the camera. If there is no floor below it, the nearest floor is selected | Stale/unassigned layer/pause; camera position/reason |
| View an area with a hole in the floor or a partial floor above it | Where the selected floor has a surface, that floor is shown. Openings reveal the nearest other floor: a lower floor is darkened, while an upper floor keeps its color; pixels with no geometry remain transparent; common scale and XY alignment | Missing context, covered active floor, wrong shading, stretch or misalignment; images/bounds |
| Other-floor marker at rotating radar edge | Opacity multiplier 0.4; screen-vertical indicator close to the marker; composite fits shape; A/B arrows follow letter height; bomb arrows follow marker size in TAB | Wrong opacity, distant/rotated/clipped arrow, or oversized TAB bomb arrow; screenshot/height |
| Player/death/sound/history/trail floor changes | Recorded XYZ retained; only active-layer endpoint pairs form trail segments | Lost height/bridge between floors; history sequence |
| Objective volumes spanning floors | If any target volume intersects the selected floor, its marker is fully opaque. Otherwise, the marker uses the appearance for another floor | Center-only result despite intersection; volume bounds/active |
| Other-floor smoke | Recorded XYZ determines relative-floor style | Wrong opacity/arrow; smoke origin/active |
| Scoreboard and radar text, scoped/unscoped | All required labels remain visible, are not duplicated, and are not covered by the map or scope image | Missing/duplicate/covered text; screenshots |
| Map smoothing on/off in generated modes, at rotating/fitted/follow scales | Map-image edges become smoother while opaque details inside the map remain sharp; toggle preserves cached images, analysis, and GL state | Interior blur, dark edge halo, missing contour pixels, regenerated images, or mask/flash artifacts |
| Map shifting on/off at edges and corners, both shapes and rotated views | Follow view retains a 30% radius inset for round and 20% half-width inset for square map edges without changing zoom; the marker of the player followed by the radar remains inside the rim; tiles and markers stay aligned; Apply/discard/defaults include the toggle | Missing marker of the player followed by the radar, scale change, marker/image drift, jump at the clamping threshold, or incorrect toggle restoration |
| Video reset | After changing the video mode, the map is visible again and the stock HUD has no rendering artifacts. Use diagnostics to verify the per-frame generation limit | Stale texture/broken HUD/resource failure; cache/settings |
| Same-name reconnect, then another map | Geometry/images/selection reset; current map in dump | Old map/image/IDs; map/bounds/cache before/after |
| Client clock correction up to 10 ms | Marker fades, reports, trails and selection retained | Group fade-in, Radar reports cleared or history restarted |
| Respawn, round reset, client time rewind beyond 10 ms | Selection/history reset; geometry/images/failure state retained until resource reset | Respawn-only reanalysis/retry or stale history; analysis/cache/source |
| Reproducible generation failure, then map/video/mode reset | Usable whole-map fallback, no per-frame retry; reset permits retry | Blank radar/repeated attempts/no retry; state/cache/trigger |
| Active layer with floor height changes and tall walls | In both modes, floor brightness varies with height within the selected layer. Walls remain fully visible. The height of an adjacent floor does not affect brightness | Compressed floor contrast, clipped details, or brightness affected by another floor |
| Pending analysis or layer material in modes 5/6 | Advanced radar shows the matching top image and ordinary markers until the layer is ready; top waiting keeps the advanced frame | Stock radar, missing markers, repeated top generation, or discarded layer cache |
| Pending GL error before map generation | The map is created if its preparation and rendering succeed. A GL error during these operations produces a diagnostic message, and the faulty image is not used | False texture-storage failure or a hidden allocation/render error |
| Repeated `radar_layers_dump` | State reported without analysis/image generation | Command-only analysis/cache changes; consecutive output |

Automatic structure and determinism are verified on the measured fixture. Correspondence to intended game floors and visual quality require the developer's map-specific runtime assessment.
