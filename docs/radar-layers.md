# Radar layers reference

[Russian version](radar-layers.ru.md) | [Verification procedure](testing/radar-layers.md) | [Acceptance cases](testing/radar-layer-cases.md)

## Getting started

With a map loaded, set `hud_radar_mode 1` and `hud_radar_map_mode 5`. The advanced radar shows a textured map and selects the player's layer when analysis is ready. Run `radar_layers_dump` to inspect analysis state and the selected layer. To adjust a particular map, see [Corrections](#corrections).

## Behavior

### Map modes and shared bounds

The HUD offers `Overview`, `Generated Flat`, and `Generated Textured`. `hud_radar_map_mode 1` selects the overview; `6` selects flat layers; `5` selects textured layers. Generated modes use the active gameplay layer. With `hud_radar_fullmap 1`, holding TAB fits the active layer image to the shared visible XY extent of all layers; releasing TAB restores the follow view. The layer follows the local player or the observed player, and free cameras select the support below them. Overview retains its authored image. The full-map radar uses 120% of the configured radar size. The map retains its proportions and fits with a fixed minimum inset of 3 HUD pixels. Rotation and follow zoom apply outside that full-map view.

Layer images share the XY bounds of retained nonvertical geometry with nonzero projected area across all layers, after filtering and manual hides. The center and scale remain constant when the selected layer changes. Raw world bounds remain available to analysis and support queries. Completely hidden geometry uses raw world bounds and keeps image readiness waiting. Pixels without retained geometry on any layer remain transparent. Generated Textured lifts dark pixels toward white by 17% of the remaining range while preserving transparency. This adjustment also applies to its whole-map view. Height tint uses each rendered layer's own support-height range. Walls and decorative surfaces do not expand that range; the projection still includes the entire layer mesh.

### Layer composition

The active layer has priority at every covered pixel. Empty regions show the nearest other layer by representative support height; equal distances favor the lower layer. Lower context layers use a brightness multiplier of 0.6 before the textured shadow lift; upper context layers keep their ordinary color. Each layer retains its own top-down surface ordering. The composite uses the existing image cache and frame generation budget, including TAB. Manual hides still remove geometry before composition.

### Automatic grouping

Automatic grouping separates substantial connected regions where support surfaces overlap in XY and both player volumes have clearance within the overlap. The significance threshold is 131072 square units of projected union area. Terrain height changes, small bridges, boxes, and isolated platforms join surrounding groups; height range alone does not create a layer. Group-wide separation constraints keep significant stacked floors independently selectable across stairs. Geometry uses 32-unit XY cells and support normal Z >= 0.7. Clearance confirms player fit. Sky-capped surfaces excluded from the image do not contribute automatic supports or separation conflicts. Map-specific corrections can resolve geometric ambiguity.

### Collision filtering

Before grouping, the generated modes filter upward supports against free regions of the world crouch collision hull. Player starts (`info_player_start`, `info_player_deathmatch`, `info_player_coop`) and teleport destinations (`info_teleport_destination` and named targets of `trigger_teleport`) seed those regions. Destination origins represent feet and are shifted to the crouching player center, following the [Valve HL SDK teleport contract](https://github.com/ValveSoftware/halflife/blob/master/dlls/triggers.cpp). With entry points, a surface is excluded only when its player-facing contact has no connection to a seeded region. Without entry points, contact is tested against every free collision region, including disconnected rooms and open areas. Any eligible contact retains the entire source polygon, including ledges and contours. The same exclusion list applies to layer images and the loading top image. Walls and other non-support details retain their rendering rules.

Render floors below collision floors use a one-sided upward contact envelope bounded by the crouch hull's upper extent (18 units for the standard hull). The contact test preserves ceiling constraints in both seeded and all-free-region modes. Layer support and overlap collection first test exact contact, then use the same bounded envelope when exact clearance is empty. Diagonal constraints conservatively retain uncertain contact.

This filter checks collision contact and, with known entry points, connectivity, without simulating jumps, gravity, or movement rules. Roofs with eligible collision contact may remain. Missing or invalid entry metadata selects the all-free-region contact test. Absent collision data or entirely blocked known entry points preserve geometry and report an unavailable filter. A ready filter with zero valid entry points uses all free regions; its connected-region count remains zero because no seeded connectivity is computed. While analysis is pending, the loading top image uses the available world geometry; it is regenerated with the exclusions when analysis is ready. Overview sprites retain their authored pixels.

### Active layer selection

An ordinary switch confirms for 0.15 seconds beyond the midpoint margin `min(16, height_gap / 4)`. Source changes, position discontinuities, and time rollback select immediately. Jumping retains the layer; falling can confirm a lower layer. Exact traced landing selects immediately. Crouching preserves feet-based selection. Free cameras use the support below, then nearest-support fallback.

### Player and other-layer markers

A known player whose authorized position becomes unavailable retains a frozen marker for 0.15 seconds, then displays a question mark at the last position. Each player has at most one mark. It holds full opacity for 3 seconds from the loss, then fades until removal at 4.5 seconds. Repeated missing updates do not extend the lifetime, and another player nearby does not clear the mark. A new authorized position for that player restores the live marker. Death, disconnect, round reset, and perspective reset clear its history. During play, missing data never extrapolates movement or reveals an unseen position. Observation displays the current followed player or camera and a crossed-signal status icon; other player markers and trails are hidden.

Player positions update immediately from confirmed entity states or permitted Radar messages. On loss of the current entity, the last confirmed position holds until a newer Radar message arrives. Cached stock-client positions supply initial placement. Broadcast movement and source corrections larger than one projected marker diameter use a 120 ms opacity crossfade between confirmed positions. Smaller corrections keep the current fade timing. The diameter tolerance follows map zoom, radar size, and TAB marker scaling. Rapid updates preserve the combined opacity of the fading pair. The old and new marker images are combined before blending into the HUD, preserving brightness where their shapes overlap. Trails record confirmed positions.

Other-layer markers multiply opacity by 0.4 and retain screen-vertical above/below indicators under radar rotation. Unknown height retains ordinary style. Height arrows sit next to the marker image with a nominal 1-pixel gap. Player heading and edge-arrow geometry determine the offset on the indicated side; site labels use half their font height. Bomb height arrows use the base marker size and its TAB scaling, independently of the larger icon clamp extent. Objective volumes intersecting the active layer retain full opacity; a volume with no intersection uses its center as a point fallback. Trails display only segments with both endpoints on the active layer.

### Shifting the map at its edges

The HUD Shift map at edges checkbox controls `hud_radar_map_shift` (default `0`) in all map modes. Follow view moves inward when its visible footprint reaches the generated image bounds or the world model bounds for Overview. The footprint is inset by 30% of the radius for a circular radar and 20% of the half-width for a square radar, leaving a gap between the map edge and the radar rim. The circular footprint is independent of view yaw; the square footprint includes its rotated corners. An axis narrower than the inset footprint is centered. The reference position stays within 90% of the radar radius (or square half-width), retaining room for its marker. Map tiles and all markers share the shift, with the selected map scale preserved. The full-map TAB view uses its fitted projection.

### Map smoothing

The HUD Map smoothing checkbox controls `hud_radar_map_smoothing` (default `0`) in generated modes, including the whole-map view. Enabled smoothing takes four additional subpixel samples of alpha when displaying the map. UV derivatives keep those samples inside the current framebuffer pixel under radar scaling and rotation. Coverage is averaged, while opaque interior colors retain the central sample. A transparent central sample takes its edge color from the most opaque subpixel sample. Textures use linear filtering.

Toggling reuses the generated pixels and ready analysis; it requires no additional texture storage or render targets. The enabled path uses five texture reads per map fragment, compared with one when disabled. Premultiplied alpha preserves transparent edges during opacity changes, mask feathering, and flash fading. Overview sprites use the engine's texture filtering.

### Resources, failures, and resets

Images are 1024 x 1024 RGBA8, with at most three cached color textures, one reusable framebuffer/depth buffer, and one generation attempt per HUD frame.

Analysis/render failure uses the corresponding whole-map fallback. Its first latched failure prints the map, mode, layer when known, and exact operation/error reason once; subsequent frames and respawns do not repeat it. Map load, video initialization, and mode changes permit retry.

Respawn, round reset, and backwards HUD time beyond 10 ms clear selection/history while retaining geometry, images, and failure state. Smaller clock corrections hold the preceding radar frame time and preserve marker fades, reports and trails. Mode changes retain ready geometry and invalidate images.

### Background analysis

Analysis runs as a scheduled `task_coro` pool task over an owned geometry snapshot. Engine data capture and GL image generation run on the main thread.

While analysis or a layer image is pending, the advanced radar displays the corresponding whole-map top view: textured in mode 5, flat in mode 6. Markers use ordinary styling until the layer image is ready. If the top image is also pending, the advanced frame and markers remain visible. The top image has independent storage and does not invalidate cached layers.

Map/video reset and leaving layered modes cancel pending work; canceled results cannot replace the current map. Switching between modes 5 and 6 retains pending analysis. Shutdown waits for the worker before releasing the client runtime.

## Corrections

The game filesystem reads `overviews/<map>_layers.txt` during analysis. Ship corrections under `assets/cstrike/overviews/`. Missing files retain automatic assignments. Invalid files print their path/error and retain automatic assignments. The dump command does not reload corrections.

```text
radar_layers
{
    layer { name "Lower" min_z "-1" max_z "100" }
    layer { name "Upper" min_z "200" max_z "300" }
    region { layer "Upper" mins "0 0 0" maxs "128 128 128" }
    hide { mins "32 32 250" maxs "96 96 300" }
}
```

### Format rules

Each block requires all shown fields, in any order: three for `layer`/`region`, two for `hide`. Unknown/repeated fields are errors. Numbers must be finite; vectors contain exactly three numbers. Names are nonempty and unique. Every minimum is strictly below its maximum. Height ranges are half-open `[min_z, max_z)` and cannot overlap. Regions are half-open on XYZ, reference a declared layer, and have no interior overlap with a region of another layer. Touching boundaries are allowed. Regions take precedence over height ranges; uncovered geometry retains automatic assignment. Valid manual merges of vertically overlapping supports report conflicts requiring map-specific assessment.

`hide` subtracts a half-open XYZ box from every layer image. Intersecting polygons are clipped at the box boundary with interpolated texture and lightmap coordinates. Hide boxes may overlap and work without layer assignments. Supports, layer selection, marker classification, and support-height tint ranges remain intact; overview sprites and whole-map top images do not use manual hide boxes.

### Shipped corrections

The shipped `cs_assault_layers.txt` combines the map into one layer and hides the warehouse roof and overhead details within `(-688, 1616, 384)` to `(688, 2752, 1024)`. The warehouse interior and exterior structures remain visible. Players on the roof belong to the same layer as those inside. The manual overlap conflicts reported for this deliberate merge are expected.

The shipped `awp_assault_layers.txt` combines all retained supports into one `Main` layer without hide volumes. The manual overlap conflicts reported for this deliberate merge are expected.

The shipped `cs_747_layers.txt` assigns terminal roof supports in the height range `[383, 385)` to a separate layer. Roof geometry and support selection remain available, while lower terminal floors retain their own image. Other heights retain automatic assignment.

## `radar_layers_dump`

The command reads current owners without rebuilding geometry or generating images. Numbers use a decimal point and three fractional digits. Absent owners report `unavailable`.

| Fields | Meaning |
|---|---|
| `map`, `correction_path`, `analysis_ms`, `map_state` | Map, attempted correction path, build/correction duration in milliseconds; state `waiting`, `ready`, `failed`, or `unavailable` |
| `geometry_state`, `image_state`, `fallback`, `failure_reason` | Owned lifecycle states, fallback flag, and retained failure reason; reading them performs no GL queries. Missing lifecycle reports `unavailable` |
| `bounds`, `layers` | Common retained geometry XY bounds after hides and layer count |
| `overlap_area_threshold`, `largest_overlap_area`, `significant_overlap_regions` | Projected area threshold, largest connected clear overlap area, and number of regions that justify layer separation |
| `reachability`, `collision_regions`, `connected_regions`, `valid_entry_points`, `excluded_surfaces`, `reachability_reason` | Collision filter state, free/connected region counts, usable entry count, excluded source surface count, and unavailable reason |
| `layer`, `name`, `height`, `area`, `mesh_surfaces` | Identifier, label, representative height, support area, mesh surface count |
| `heights`, `bounds`, `assignment_reason` on a layer line | Support height range, group XY bounds, assignment reasons |
| `transitions`, `transition`, `height`, `surfaces` | Count, lower/upper identifiers, boundary height, source surface identifiers |
| `manual_conflicts`, `manual_conflict` | Count and conflict messages |
| `active_layer`, `candidate_layer`, `candidate_since` | Selected/pending layer and confirmation start in client time; layer `-1` means absent |
| `source` | Last accepted entity index, `0` for camera, `-1` before a valid observation |
| `selection_reason` | `initial`, `unchanged`, `pending`, `confirmed`, `grounded`, `airborne`, `discontinuity`, `invalid`, `nearest`, or `unavailable` |
| `cache`, `cache_slot`, `layer`, `valid`, `last_frame` | Entry count, slot, layer, image validity, last-use HUD frame |

### Waiting reasons

During `waiting`, `analysis_ms=0.000` means no completed analysis has been published. `capture_ms` measures main-thread geometry capture and correction parsing; `analysis_ms` measures the completed worker calculation, excluding queue time. `waiting_reason="stage=analysis_worker state=scheduled"` identifies scheduled or running analysis. `preparation_attempts` counts prerequisite checks while geometry needs building, `preparation_mode` records the last checked map mode, and `waiting_reason` identifies the blocking stage with surface/polygon details or callback/collision data where available. These fields are owned state read without additional engine or GL queries; success/reset clears the reason.

`image_waiting_reason` identifies the current image prerequisite: world data, an empty layer mesh, the first invalid material/texture, or the frame render budget. Material details include source surface/material identifiers, sky/tiled flags, base/lightmap texture identifiers, whether a lightmap is required, and results of the texture checks already performed. `unchecked` means the lightmap query was skipped by the readiness check. Success/reset clears the reason; reading it performs no GL queries.

### Image geometry

Layer images preserve original surface contours and UVs independently of player-clearance samples. Floor ownership still uses accessible supports; rendering retains visible edges and nearby details, with clipping at layer transitions and manual correction boundaries. Meshes exclude sky, downward-facing surfaces including sloped roof undersides, and sky-capped roofs; tiled supports remain renderable.

Cache state does not establish rendered-pixel correctness. Visual acceptance follows the linked cases.
