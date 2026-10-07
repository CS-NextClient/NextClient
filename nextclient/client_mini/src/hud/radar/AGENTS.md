# Advanced radar (client_mini/src/hud/radar)

The advanced radar HUD: it replaces the stock square radar with a projected map, markers, labels
and objectives. `HudRadarAdvanced` (one level up, in `src/hud`) owns the per-frame draw; everything
under this directory is the machinery it drives.

## Map of the directory

| Area | Files |
|---|---|
| Frame model and projection | `RadarView.*` (box, shape, transform), `RadarFrame.h`, `RadarTeam.h` |
| Drawing | `RadarDraw.*` (GL prims, shaders, clip) with `RadarDrawEffects.cpp` (blur capture, mask) behind `RadarDrawInternal.h`, `RadarText.*` and `RadarLabel.*` (labels), `RadarPalette.h`, `ServerMarker.*`, `RadarFade.*`, `HudTextZone.*`, `HudTextBuffer.*` |
| Map sources | `Overview.*` (BSP overview sprite), `MapRender.*` (runtime generated map), `MapObjectives.*` (bomb sites, A/B letters) |
| Per-frame contents | `layers/BackdropLayer.*` (box, map, smoke, rim), `layers/PlayersLayer.*` over `layers/players/*`, `layers/ObjectivesLayer.*` (sites plus the dispatch) over `layers/objectives/*` (`ObjectiveEntities.*`, `BombMarker.*`, `HostageMarkers.*`, each marker picking its own feed through `ObjectiveSource.*`) |
| State that outlives a frame | `layers/players/PlayerTracks.*` (trails), `layers/players/DeathMarks.*`, the remembered spots inside the two marker units, `MarkerFeed.*` over `MarkerIdentity.*` (the snapshot and broadcast feeds of one feature, resolved into one marker set), `SmokeRegistry.*`, `RadarBroadcast.*` (server-sent markers), `RadarText.*` (layout cache, font memo) |
| Rules and plumbing | `RadarPolicy.*` (what may be revealed), `ObjectiveSource.*` (which feed serves whom), `Visibility.*` (line of sight), `RadarConfig.*` (cvars), `RadarHooks.*` (engine hooks), `RadarDebug.*`, `RadarUtil.h`, `RadarLimits.h` |

Cvar definitions do not live here: `include/next_client_mini/radar_cvars.h` is the single table the
client registration and the GameUI dialog both read. Adding a cvar means editing that table, the
dialog (`nextclient/gameui/src/GameUi/OptionsDialog/HudRadarDialog.cpp`, its `.res` and the
localisation files), not a new constant in this directory.

## How a frame works

`HudRadarAdvanced::Draw` computes, in this order: `RadarBox` (where the radar sits on screen),
`RadarShape` (its rim), `RadarTransform` (world to screen), then fills one `RadarFrame` and hands it
to the layers. Draw order is expressed literally by the order of those calls; there is no z sort.

Every draw belongs to a layer: a layer consumes the `RadarFrame` and owns the state behind what it
draws. `HudRadarAdvanced` keeps only what produces that frame (the box, the transform, the frame
context, the policy update, the flash alpha) plus the cvar and hook plumbing. A new drawn thing is a
new layer or an existing one, never another `DrawX` on the HUD class; the layers hold no base class,
because a virtual list would bury the order that this file calls load-bearing.

## Invariants that break silently when violated

- **One shape per frame.** The clip passed to the shader, the clamp applied to markers and the
  offscreen test must all come from `frame.shape`. That is why `RadarShape` exists; re-deriving the
  centre or the radius from `RadarBox` at a call site reintroduces the drift it was made to prevent.
- **`Contains` has no inset, `Clamp` takes the marker's own size.** Testing "is it off the radar"
  with an inset makes edge arrows appear a dot early.
- **Label passes run per pass, not per marker.** Cones, dots, health bars and names are separate
  sweeps over the job array so each is a single prim batch and z order comes from pass order.
  Folding them into a per-marker routine reorders the output and lets one player's cone wash over a
  neighbour's marker.
- **GL state around the prim batches is load-bearing.** `RadarDraw` forces texture unit 0 and
  restores the binding, the current colour and the blend state. Both were once real HUD defects
  (garbled number sprites, a white flashing timer digit), not caution.
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
- **Two feeds for one thing go through `MarkerFeed`.** A feature the snapshot and a broadcast both
  describe (hostages today, players and anything custom later) hands both spans to `MarkerFeed` and
  draws what it returns: it pairs the ids, reports a thing the snapshot carries once, and plays the
  broadcast positions back over the interval they arrive at, which is what keeps a marker outside
  the PVS from stepping once a second. A thing that leaves the PVS holds where the snapshot last
  showed it until a newer message arrives: the message at hand is up to a second older and would
  roll the marker back. Merging the two feeds inside a marker unit is how the duplicate, the
  stutter and the rollback come back.
- **Which feed serves whom is decided in `ObjectiveSource`.** Who receives a message is the
  server's rule: a dropped bomb and its pickup reach living Terrorists only, while the plant and
  its clears reach everyone. A spectator therefore takes the bomb from the snapshot first, and a
  drop message that stopped repeating is stale rather than a bomb still lying there.
- **`RadarPolicy` owns reveal decisions.** Game modes re-purpose the stock messages (free-for-all
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
- **Hostages.** Movement cannot tell a re-purposed HostagePos apart, because hostages walk. It is
  judged instead by what the map holds, by the index range the server can legitimately use, and by
  an update rate `CHostage` cannot produce.
- An explicit `serverinfo` hint outranks every heuristic and is the only evidence available before
  the first round produces any.

## The shape of an API in this directory

Pick by what the thing owns, not by taste; the rule itself is in the repository's root `AGENTS.md`,
and here it lands as:

- **Classes** for state with an owner: `SmokeRegistry`, `RadarBroadcast`, `RadarText`, `RadarPolicy`,
  `RadarVisibility`, the three layers and the units they own (`PlayerTracks` and `DeathMarks` under
  `layers/players`, `ObjectiveEntities`, `BombMarker` and `HostageMarkers` under
  `layers/objectives`), and `MarkerFeed` with the `MarkerIdentity` it owns, one instance per feature
  that has two feeds.
  `HudRadarAdvanced` owns the instances and resets them; the engine callbacks reach the two the hooks
  feed through the `RadarSinks` registration in `RadarHooks.h`, which is live for exactly the owner's
  lifetime. A unit keeps its own remembered state: the hostage slots and the bomb's last sighting sit
  in the markers that draw them, one class per file.
- **`Module_Function` free functions** for the pure helpers (`RadarView`, `RadarUtil`, `RadarLabel`,
  `RadarFade`, `ObjectiveSource`) and for the singletons a C callback or an engine table has to
  reach: `RadarDraw` (GL programs), `RadarHooks` (the patched client.dll table), `RadarConfig` (cvar
  pointers), `RadarDebug`, `HudTextZone`. `namespace RadarConfig` is closed anyway: `struct RadarConfig` holds the name.
- **A namespace** for the marker palette (`RadarPalette`); the bounds in `RadarLimits.h` read
  unambiguously on their own and stay at file scope.
- **Types at file scope**: `RadarBox`, `RadarShape`, `RadarTransform`, `RadarFrame`, `FrameContext`,
  `RadarTeam`, `RuntimeMap`, `RadarSinks`.

Before renaming any symbol declared here, run `git grep -n <old_name> feature/radar-harness`. That
branch adds whole files, so a rename it depends on rebases without a conflict marker and then fails
to compile.

## Conventions

Includes, member defaults, naming forms, comments, casts and braces are project-wide and live in the
root `AGENTS.md`. What this directory adds:

- Project include paths are written from `src` (`#include "hud/radar/RadarDraw.h"`), and the module
  umbrella `main.h` stays ahead of the legacy HL SDK headers, which are not self-contained. A layer
  reaches the units it owns through its own sub-directory (`#include "players/PlayerTracks.h"`). The
  headers sitting directly in this directory include their neighbours by bare name (`#include
  "RadarLimits.h"`); the unit test target adds both `src` and this directory as include roots, so
  either form resolves there.
- Prefer `ncl_math` over hand-rolled maths: `Clamp`, `Lerp`, `Remap`, `TryNormalize`, `Aabb2`,
  `Vector2::Perp`. If an expression is written twice, it belongs in the package.
- tier1 string helpers are `V_`-prefixed (`V_strcpy_safe`, `V_sprintf_safe`); the `Q_` names are
  aliases.
- The `Check` half of the predicate rule lands here as `RadarDraw_CheckPrimsAvailable` (one-time
  shader init), `RadarText::CheckAvailable` (creates the font) and
  `RadarVisibility::CheckPointVisible` (runs engine traces); `get_map_objectives` is the accessor
  that is not plain, since it parses the entity lump.

## Building and checking

```bash
cmake --build build/ninja-local --target client_mini --config Debug
cmake --build build/ninja-local-tests --target client_mini-tests --config Debug
build/ninja-local-tests/out/bin/Debug/client_mini-tests.exe
```

The unit tests cover the parts that can run without the engine: `RadarViewTests` (projection and
clamping, including the rotated basis), `RadarPolicyTests`, `RadarFadeTests`, `PlayerTracksTests`,
`MarkerIdentityTests`, `MarkerFeedTests`, `ObjectiveSourceTests`, `RadarUtilTests`, `HudTextZoneTests`
and `HudTextBufferTests`. Changes to projection, reveal rules or stock-text placement must keep the
corresponding tests green.

Do not launch the game to check a change. A build plus the unit tests is the loop; the developer runs
the game. Deploying (`--target BUILD_ALL`) installs into the user's game directory, so do it only when
asked.

## How the harness reaches the radar's state

`RadarBroadcast` and `SmokeRegistry` belong to `HudRadarAdvanced` and have no global reader. The test
harness on `feature/radar-harness` reads them through `RadarHooks_Sinks()`, whose pointers are live for
exactly the radar's lifetime and null outside it. A reader that has an owner to ask should take a
reference instead, the way `RadarVisibility` and the layers do.

## The test harness lives on another branch

`RadarDump`, `RadarShot`, `RadarTestPlan` and `tools/radar_tests` are not part of this directory: they
are one commit on `feature/radar-harness`, rebased onto this branch when the harness is needed. When
you add a marker kind or change a draw path, that commit carries the matching dump call and will
conflict on the next rebase.
Build it too when you touch these files: a local that is dead here can be live there.
