# Radar layers reference

[Russian version](radar-layers.ru.md) | [Verification procedure](testing/radar-layers.md) | [Acceptance cases](testing/radar-layer-cases.md)

## Getting started

With a map loaded, set `hud_radar_mode 1` and `hud_radar_map_mode 5`. The advanced radar shows a textured map and selects the player's layer when analysis is ready. Run `radar_layers_dump` to inspect analysis state and the selected layer. To adjust a particular map, see [Corrections](#corrections).

## Behavior

### Map modes and shared bounds

`hud_radar_map_mode 1` selects the authored overview, `6` generated flat layers, and `5` generated textured layers. With `hud_radar_fullmap 1`, holding TAB fits the active layer to the full-map view. Generated layers keep a common map position and scale when the active floor changes.

### Layer composition

The radar prioritizes the player’s floor. Openings and areas without floor geometry reveal other floors. Unwanted areas can be hidden in the correction file. Compare the result from the same position and in the same radar mode.

### Automatic grouping

The radar separates substantial stacked floors while keeping ordinary terrain changes and small platforms with their surrounding area. A map with unusual geometry may need a correction. Use the dump and the acceptance cases to assess the result on an actual route.

### Active layer selection

The layer follows the local or observed player. A jump normally retains the current floor; movement onto another floor changes the selection. A free camera uses the floor below or the nearest available floor. Check stairs, falls, moving platforms, and observer changes on the target map.

### Player and other-layer markers

Markers distinguish players on the active floor from players above or below it. A temporarily lost known position can become a fading question mark; the radar does not reveal an unknown position. Death, disconnect, round changes, and observer changes reset the relevant marker history.

### Map smoothing

`hud_radar_map_smoothing 1` smooths generated-map edges, including the full-map view. Its default is `0`. Compare edges and rendering cost at the same zoom and rotation. Switching it preserves the prepared map and analysis.

### Background analysis

While layer analysis or imagery is pending, the radar uses the corresponding whole-map view when available. Markers retain ordinary styling until the layer is ready. The dump reports readiness and waiting reasons.

Map/video changes and leaving layered modes cancel pending preparation. Switching between flat and textured layer modes retains analysis. Results from a previous map must not replace the current map.

## Corrections

The game filesystem reads `overviews/<map>_layers.txt` during analysis. Ship corrections under `assets/cstrike/overviews/`. If the file is missing or invalid, the radar assigns layers automatically. An invalid file also produces a message with its path and the error. The dump command does not reload corrections.

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

Each block requires all shown fields, in any order: three for `layer`/`region`, two for `hide`. Unknown/repeated fields are errors. Numbers must be finite; vectors contain exactly three numbers. Names are nonempty and unique. Every minimum is strictly below its maximum. Height ranges are half-open `[min_z, max_z)` and cannot overlap. Regions are half-open on XYZ, reference a declared layer, and have no interior overlap with a region of another layer. Touching boundaries are allowed. Regions take precedence over height ranges; uncovered geometry retains automatic assignment. If a correction combines vertically overlapping floors into one layer, the client reports a conflict. Check that the image shows the intended rooms and passages.

`hide` removes an XYZ box from every layer image. Coordinates use the same lower-inclusive, upper-exclusive convention as layer-assignment regions. Hide regions can overlap. They affect only the image: floor selection, marker positions, and marker brightness are preserved. Existing overview sprites and whole-map top images do not use `hide`.

### Shipped corrections

The shipped `cs_assault_layers.txt` combines the map into one layer and hides the warehouse roof and overhead details within `(-688, 1616, 384)` to `(688, 2752, 1024)`. The warehouse interior and exterior structures remain visible. Players on the roof belong to the same layer as those inside. The manual overlap conflicts reported for this deliberate merge are expected.

The shipped `awp_assault_layers.txt` combines the map into one `Main` layer. This file contains no `hide` blocks. The manual overlap conflicts reported for this deliberate merge are expected.

The shipped `cs_747_layers.txt` assigns the terminal roof in the height range `[383, 385)` to a separate layer. The radar shows the roof when the player is on it, and shows the lower terminal floors in their own layer. Other heights use automatic layer assignment.

## `radar_layers_dump`

The command reports radar status. It does not analyze the map or generate images. Numbers use a decimal point and three fractional digits. Unavailable data is reported as `unavailable`.

| Fields | Meaning |
|---|---|
| `map`, `correction_path`, `analysis_ms`, `map_state` | Map, attempted correction path, build/correction duration in milliseconds; state `waiting`, `ready`, `failed`, or `unavailable` |
| `geometry_state`, `image_state`, `fallback`, `failure_reason` | Geometry and image status, use of the fallback map image, and the failure reason. Unavailable data is reported as `unavailable` |
| `bounds`, `layers` | Common retained geometry XY bounds after hides and layer count |
| `overlap_area_threshold`, `largest_overlap_area`, `significant_overlap_regions` | Projected area threshold, largest connected clear overlap area, and number of regions that justify layer separation |
| `reachability`, `collision_regions`, `connected_regions`, `valid_entry_points`, `excluded_surfaces`, `reachability_reason` | Collision filter state, free/connected region counts, usable entry count, excluded source surface count, and unavailable reason |
| `layer`, `name`, `height`, `area`, `mesh_surfaces` | Identifier, label, representative height, area of surfaces the player can stand on, mesh surface count |
| `heights`, `bounds`, `assignment_reason` on a layer line | Height range where the player can stand, layer XY bounds, and reasons for assigning surfaces to the layer |
| `transitions`, `transition`, `height`, `surfaces` | Count, lower/upper identifiers, boundary height, source surface identifiers |
| `manual_conflicts`, `manual_conflict` | Count and conflict messages |
| `active_layer`, `candidate_layer`, `candidate_since` | Selected/pending layer and confirmation start in client time; layer `-1` means absent |
| `source` | Last accepted entity index, `0` for camera, `-1` before a valid observation |
| `selection_reason` | `initial`, `unchanged`, `pending`, `confirmed`, `grounded`, `airborne`, `discontinuity`, `invalid`, `nearest`, or `unavailable` |
| `cache`, `cache_slot`, `layer`, `valid`, `last_frame` | Entry count, slot, layer, image validity, last-use HUD frame |

### Waiting reasons

During `waiting`, `analysis_ms=0.000` means no completed analysis is available. `capture_ms` measures data preparation, while `analysis_ms` measures the completed background calculation without waiting time. `waiting_reason="stage=analysis_worker state=scheduled"` means analysis is scheduled or running.

`preparation_attempts` counts readiness checks, `preparation_mode` records the last checked map mode, and `waiting_reason` explains what prevents preparation. `image_waiting_reason` explains what prevents image creation, such as unavailable world data, materials, or textures. `unchecked` means the relevant check has not been performed. A successful operation or reset clears the reason.

### Image geometry

Layer images preserve surface contours and texture alignment. They show accessible floors and nearby details, including boundaries set by manual corrections. Sky surfaces and roof undersides are hidden.

Use the [acceptance cases](testing/radar-layer-cases.md) to assess the image. A successful diagnostic status alone does not establish visual correctness.
