# Advanced radar feature review (feature/advanced-radar)

Date: 2026-07-22. Scope: the whole `feature/advanced-radar` branch versus `main`
(118 commits, ~110 files, ~11.6k insertions) spanning two subsystems: the advanced
radar HUD (`client_mini`) and the engine-owned text/glyph service (`engine_mini`,
touching vgui2, the nitro_api ABI and GL).

The review was framed around a catalogue of AI-review pitfalls (project-style drift,
functional duplication, version/ABI incompatibility, incomplete call-graph safety,
unreliable model self-assessment, masked flaky tests, implicit business rules, public
API contracts). It ran in two layers because reading alone cannot cover all of them.

## Method

- **Layer 1: build + tests.** `BUILD_ALL` (whole project, Debug) plus the
  `client_mini-tests` unit suite were built and run. `engine_mini` builds under `/WX`,
  so a clean build already rules out warnings and cross-module link/ABI breakage. An
  in-game capture harness (`radar_dump` / `radar_shot`, driven unattended by
  `tools/radar_tests/radar_harness.py`) is also available on this branch for visual and
  marker-model regression capture; see "Regression harness" below.
- **Layer 2: multi-agent static review.** Seven dimension finders
  (`lifetime-ordering`, `abi-nitro-api`, `duplication-architecture`, `gl-resource-state`,
  `business-logic-correctness`, `style-contract`, `test-coverage`) read the changed
  code. The three highest-risk finders were seeded with the project's hard-won
  invariants (single-linker `vgui2_surfacelib`; append-after-`CL_HookEvent` for
  `EngineData`; `cl.entities` freed on disconnect/hot-switch; static destruction
  order; blur capture from `GL_DRAW_FRAMEBUFFER_BINDING`) and with the user's
  code-quality contract. **Every finding was then independently adversarially
  verified** (a second agent tasked with refuting it, defaulting to REFUTED when the
  code could not decide it), which is the layer that addresses unreliable
  self-assessment: only CONFIRMED findings survive.

## Results

Build layer: `BUILD_ALL` green; unit suite green.
Static layer: 14 raw findings -> **8 confirmed, 6 refuted** (0 uncertain). After
verification recalibrated severities: 0 high, 4 medium, 4 low. No live crash, no ABI
break, no lifetime defect survived verification; the `abi-nitro-api` dimension found
nothing.

### Confirmed findings and resolution

| # | File | Sev | Summary | Status |
|---|------|-----|---------|--------|
| 5 | `Visibility.cpp:272` | low | Teammate LOS viewer eye used the standing offset (`PM_VEC_VIEW=17`) for crouching teammates, over-revealing enemies over low cover | Fixed: pick `PM_VEC_DUCK_VIEW` when `curstate.usehull == 1` |
| 6 | `text_diag.cpp:96` | low | Atlas page size / texel stride duplicated as private literals, decoupled from `glyph_atlas`; latent readback overrun if `kPageSize` changes | Fixed: exposed `GlyphAtlas::kPageSize` / `kBytesPerTexel`; both sides read the single source |
| 7 | `ObjectivesLayer.h:48` | low | `HostageEnt::origin` was `float[3]`, Ref-wrapped on every use; sibling `BombEnt` already used a vector type | Fixed: `ncl_math::Vector3 origin` |
| 8 | `PlayersLayer.h:63` | low | `prev_pos_[N][2]` hand-packed/unpacked; sibling `prev_alive_` already `std::array` | Fixed: `std::array<ncl_math::Vector2, N>` |
| 1 | `clip.h:20` | medium | `ncl_math::ClipHalfPlane` (fallback map clip) had zero tests | Fixed: 5 tests (both sides, both axes, fully in/out, UV interpolation) |
| 2 | `RadarFade.cpp:14` | medium | `RadarFade::Alpha` (flashbang blind gating) untested | Fixed: 5 tests; `RadarFade.cpp` made engine-free so it links into the harness |
| 3 | `RadarMemory.cpp:62` | medium | `PlayerTracks` ring buffer wrap/ordering/jump-break untested | Fixed: 4 tests |
| 4 | `RadarView.cpp:75` | medium | `ComputeTransform` rotated-overview and multi-tile branches never executed under test | Fixed: 2 tests (tile grid; rotated-overview projection invariant) |

Note on #4: the rotated-overview branch was found, on inspection, to leave the marker
projection unchanged for the square 4:3 tile grid (`x_tiles = aspect * y_tiles`, so
`|x_step| = |y_step|`); the flag only reorients the map image. The test pins that
invariant, which still catches a sign error in the rotated basis assignment.

### Refuted findings (rejected by the adversarial layer)

These were surfaced by finders and killed on verification against the real code; they
are recorded so the reasoning is not re-derived later:

- `SmokeRegistry` createsmoke hook "survives Uninitialize and calls a zeroed
  `gEngfuncs`" -> unreachable: `Uninitialize` runs only after `engine->Run()` returns,
  when no events dispatch.
- `TextGlyphFormat` single-value-enum "dead generality" -> no second format exists;
  not a defect.
- `RadarText` "HFont != TextFont_t" invariant -> both index the same
  `g_TextRender.fonts_` singleton; the cast is a no-op.
- Glyph atlas "not released on GL context recreation" -> `Draw_Shutdown` brackets
  `Draw_Init` (proven via the `menu_wad` free), releasing the atlas and bumping the
  generation.
- `Overview.cpp` `Q_snprintf` -> the contract does not ban `Q_` for formatting; the
  module's convention is explicit-size.
- `scalar.h` `HoldLinear`/`LerpAngle` untested -> `LerpAngle` is dead code (zero
  callers); the finding misrepresented it. `HoldLinear` residue is a low nit.

## Changes landed on this branch

Fixes: `Visibility.cpp`, `glyph_atlas.h/.cpp`, `text_diag.cpp`, `ObjectivesLayer.h/.cpp`,
`PlayersLayer.h/.cpp`, `RadarFade.cpp`.
Tests: `NclMathTests.cpp` (+5), `RadarViewTests.cpp` (+2), new `RadarMemoryTests.cpp`
(+4), new `RadarFadeTests.cpp` (+5), `tests/CMakeLists.txt`. Suite: 21 -> 37 tests,
all passing.

The test harness stays engine-free (links `ncl_math` only). `RadarFade` needs the
SDK's `screenfade.h` POD header only, pulled in via the `hl1_source_sdk` interface
include paths (headers only, no engine/vgui/steam link).

## Regression harness

The `radar_dump` / `radar_shot` capture harness was merged onto this branch from
`radar-autotests-local2` (commit `14927e8`, fast-forward). It adds an in-game
regression layer on top of the engine-free unit suite:

- `radar_dump [file.json]` serialises the computed marker model (kind, world/screen
  xy, colour, flags) - GPU-independent, so its goldens are portable.
- `radar_shot [file.tga]` reads back the full composited HUD frame (blur backdrop,
  flash wash, smoke, text shift) - machine/resolution-bound, compared with a tolerant
  image diff.
- `tools/radar_tests/radar_harness.py` drives a demo end to end (a plan file the
  client auto-loads, with event/interval/time triggers) and diffs everything in one
  command; `imgdiff.py` does the tolerant image comparison. Full usage:
  `tools/radar_tests/README.md`.

The review's internal type changes (`HostageEnt::origin`, `prev_pos_`) do not alter any
computed marker coordinate, so existing `radar_dump` goldens stay valid. The merged
tree (harness + review fixes) builds green and the 37-test unit suite still passes.
