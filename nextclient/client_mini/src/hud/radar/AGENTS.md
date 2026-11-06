# Advanced radar (client_mini/src/hud/radar)

The advanced radar HUD: it replaces the stock square radar with a projected map, markers, labels and objectives. `HudRadarAdvanced` (one level up, in `src/hud`) owns the per-frame draw; everything under this directory is the machinery it drives.

`map/` owns map geometry and layer analysis, `render/` projection, GL drawing and text, `markers/` shared placement and feeds, `policy/` reveal decisions, and `integration/` engine hooks and diagnostics. The HUD layers own drawn state, with player and objective units beside their owning layer under `layers/`.

Cvar definitions do not live here: `include/next_client_mini/radar_cvars.h` is the single table the client registration and the GameUI dialog both read. Adding a cvar means editing that table, the dialog (`nextclient/gameui/src/GameUi/OptionsDialog/HudRadarDialog.cpp`, its `.res` and the localization files), not a new constant in this directory.

## How a frame works

`HudRadarAdvanced::Draw` computes, in this order: `RadarBox` (where the radar sits on screen), `RadarShape` (its rim), `RadarTransform` (world to screen), then fills one `RadarFrame` and hands it to the layers. Draw order is expressed literally by the order of those calls; there is no z sort.

Every draw belongs to a layer: a layer consumes the `RadarFrame` and owns the state behind what it draws. `HudRadarAdvanced` keeps only what produces that frame (the box, the transform, the frame context, the policy update, the flash alpha) plus the cvar and hook plumbing. Add new drawing to a new or existing layer, never to another `DrawX` method on the HUD class. Layers have no base class: direct calls keep draw order explicit.

## Invariants that break silently when violated

- **One shape per frame.** The clip passed to the shader, the clamp applied to markers and the
  offscreen test must all come from `frame.shape`. That is why `RadarShape` exists; re-deriving the
  center or the radius from `RadarBox` at a call site reintroduces the drift it was made to prevent.
- **`Contains` has no inset, `Clamp` takes the marker's own size.** Testing "is it off the radar"
  with an inset makes edge arrows appear a dot early.
- **Label passes run per pass, not per marker.** Cones, dots, health bars and names are separate
  sweeps over the job array so each is a single prim batch and z order comes from pass order.
  Folding them into a per-marker routine reorders the output and lets one player's cone wash over a
  neighbor's marker.
- **Restore GL state around the prim batches.** `RadarDraw` forces texture unit 0 and restores
  the binding, current color and blend state. Incorrect restoration can garble number sprites
  or make timer digits flash white.
- **Player and hostage tables are sized from `RadarLimits.h`.** The bounds are restated once there
  and tied to `goldsrc::cstrike::MAX_PLAYERS` and `MAX_HOSTAGES` by the static_asserts beside them.
  A local copy of either number is how a table silently ends up one size and its writer another.
- **Entity sweeps are bounded by the engine, not by a constant.** Start at `GetMaxClients() + 1` and
  stop when `GetEntityByIndex` returns null. The live edict ceiling comes from `-num_edicts` and
  exceeds the SDK's `MAX_EDICTS`.
- **Text goes through tier1's decoder.** `Q_UTF8ToUChar32` decodes and `Q_UnicodeTruncate` cuts, so
  one decoder decides where characters begin. `FilterCodepoint` is not a Unicode primitive but the
  engine-compatibility rule (control characters draw as a space). The flash cut for labels lives in
  `RadarLabel_FlashFadeFor`, shared by both label renderers because their fades differ.
- **Stock HUD text is buffered as fragments, then shifted as a block.** `HudTextBuffer` preserves
  the original draw order and return contracts; `HudTextZone` groups lines by their original
  alignment and computes one displacement for the group. Grouping is a geometric heuristic: the
  engine supplies no semantic block ID. Do not shift characters or adjacent lines independently.
  Measure through the corresponding original font path, invalidate metrics on video changes and
  preserve ambient color transitions, including the console-string reset to `con_color`.
- **A broadcast index is not an entity index.** `HostagePos` carries `CHostage::m_iHostageIndex`,
  a spawn counter the server hands out 1..N, while the snapshot carries edicts. `MarkerIdentity`
  pairs the two by position and is the only place that answers which entity a message describes;
  comparing the numbers directly leaves a second, lagging marker on every escorted hostage. The
  `Radar` message is the other case, where the index already is the entity and `index_is_entity`
  says so.
- **Hostage feeds go through `MarkerFeed`.** It pairs snapshot entities with broadcast ids and
  interpolates broadcast positions. Source transitions hold the snapshot position until a newer
  report arrives, then correct the position on snapshot return.
- **Players use confirmed positions and an opacity fade.** `PlayersLayer` chooses current entity
  states or permitted `Radar` messages. Its `PlayerMarkerFade` holds the last entity position until
  a newer report arrives, then immediately selects that report's position. A 120 ms crossfade draws
  only the old and new positions. Continuous entity states do not restart the fade. Trails sample
  confirmed positions and retain history across sparse reports and snapshot return; the fading
  copy never contributes trail points. Reveal decisions still belong to `RadarPolicy` and
  `RadarVisibility`.
- **Observer radar shows navigation only.** `PlayersLayer` draws one current observed player or
  the camera plus the crossed-signal status icon. Other players, memory marks, deaths and trails
  are neither collected nor drawn. Objectives retain static site labels; dynamic objectives and
  smoke overlays are hidden. `FrameContext::observed_idx` names the followed player independently
  of `highlight_idx`, which can be zero in chase views. Owned player and objective state is reset
  on observer transitions.
- **Which feed serves whom is decided in `ObjectiveSource`.** Who receives a message is the
  server's rule: a dropped bomb and its pickup reach living Terrorists only, while the plant and
  its clears reach everyone. A spectator therefore takes the bomb from the snapshot first, and a
  drop message that stopped repeating is stale rather than a bomb still lying there.
- **`RadarPolicy` owns reveal decisions.** Game modes repurpose the stock messages (free-for-all
  zeroes the Radar origin rather than withholding it; a plant arrives as a broadcast BombDrop), and
  the rules are unit-tested. Do not spread mode sniffing into the layers.

## What the policy decides, and on what evidence

- **Allies.** The server hands out the right to reveal an ally. An ally is drawn ungated only while
  its broadcast keeps tracking it; once the broadcast freezes, the ally goes through the enemy
  line-of-sight path. This is what makes free-for-all work: ReGameDLL keeps sending the Radar
  message and zeroes the origin, so filling the gap from the PVS would reveal what the server
  withheld.
- **The bomb.** A bomb that keeps moving, or that moves while flagged as planted, is not a bomb, and
  neither is any bomb message on a map with no bomb targets. Zombie modes mark a surviving player
  with BombDrop.
- **Hostages.** Movement cannot tell a repurposed HostagePos apart, because hostages walk. It is
  judged instead by what the map holds, by the index range the server can legitimately use, and by
  an update rate `CHostage` cannot produce.
- An explicit `serverinfo` hint outranks every heuristic and is the only evidence available before
  the first round produces any.

## Ownership and API

The root `AGENTS.md` defines API shape. Apply it here as follows:

- Keep owned state in classes: `HudRadarAdvanced` owns the layers and shared services; each layer owns its player or objective units. `RadarSinks` registration in `RadarHooks.h` is live for exactly the HUD owner's lifetime. Keep each unit's remembered state in its owning class and group related classes by owner.
- `HudRadarAdvanced` owns `radar::integration::LayerAnalysis`. Its native `RadarLayerAnalysisJob` owns worker input, cancellation, one running task and the latest pending request. The job emits native request/status/duration events only from owner-thread dispatch, reset and polling. The integration adapter captures monitor map generations at `Start`, translates events to worker reports and erases attribution on terminal events. Shut down and join analysis before destroying the injected performance owner or scheduler; the adapter cannot be copied or moved because its job sink captures the owner.
- `HudRadarAdvanced` owns the concrete `RadarDrawContext` and supplies direct references to its layers and text. Small marker units receive the same context through `RadarFrame`. Prepare native identity at HUD frame/lifecycle boundaries, never inside primitive or glyph loops. Stop retirement callbacks before destroying the HUD. Explicit reset releases GL names only for the tracked current lifetime; otherwise it abandons names and pending work. Overview/map target owners follow the same decision, and their destructors perform no GL work. `RadarHooks` and `RadarDebug` remain engine callback adapters.
- Subsystem APIs may group types and functions in namespaces under the root rules. `RadarPalette`, `RadarDrawUniform` and `RadarPlayerMarkerGeometry` qualify groups of constants; `RadarLimits.h` defines the shared table bounds.

## Conventions

Includes, member defaults, naming forms, comments, casts and braces are project-wide and live in the root `AGENTS.md`. What this directory adds:

- Project include paths are written from `src` (`#include "hud/radar/render/RadarDraw.h"`),
  and the module umbrella `main.h` stays ahead of the legacy HL SDK headers, which are not
  self-contained. A translation unit includes its own header first; dependencies in other files
  use their full project path. Keep each owner's header and implementation in the same directory.
- Prefer `ncl_math` over hand-rolled math: `Clamp`, `Lerp`, `Remap`, `TryNormalize`, `Aabb2`,
  `Vector2::Perp`. If an expression is written twice, it belongs in the package.
- tier1 string helpers are `V_`-prefixed (`V_strcpy_safe`, `V_sprintf_safe`); the `Q_` names are
  aliases.
- The `Check` half of the predicate rule lands here as `RadarDrawContext::CheckPrimsAvailable` (one-time
  shader init), `RadarText::CheckAvailable` (creates the font) and
  `RadarVisibility::CheckPointVisible` (runs engine traces); `get_map_objectives` is the accessor
  that is not plain, since it parses the entity lump.

## Building and checking

Use the configured build directories and toolchain described in the root `AGENTS.md`. Replace `<build-dir>`, `<test-build-dir>` and `<config>` with the selected directories and configuration; the test build must have `NEXTCLIENT_BUILD_TESTS=ON`.

```text
cmake --build <build-dir> --target client_mini --config <config>
cmake --build <test-build-dir> --target client_mini-tests --config <config>
```

Run `client_mini-tests` from `<test-build-dir>/out/bin/<config>/`, with the `.exe` suffix on Windows.

The unit tests cover the parts that can run without the engine: `RadarViewTests` (projection and clamping, including the rotated basis), `RadarPolicyTests`, `RadarFadeTests`, `PlayerTracksTests`, `MarkerIdentityTests`, `MarkerFeedTests`, `ObjectiveSourceTests`, `RadarUtilTests`, `HudTextZoneTests` and `HudTextBufferTests`, with `PlayerMarkerFadeTests` covering immediate positions, source transitions and opacity fades. Changes to projection, reveal rules or stock-text placement must keep the corresponding tests green.

Do not launch the game to check a change. A build plus the unit tests is the loop; the developer runs the game. `BUILD_ALL` is available only in non-test configurations and installs into the user's game directory when `NEXTCLIENT_INSTALL_DIR` is set, so run it only when deployment is requested.
