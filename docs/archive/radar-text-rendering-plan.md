# Radar Text Rendering Plan

Status: steps 1 to 6 and 8 to 10 shipped and verified in game; step 7 cancelled by design (see
section 5). Line anchors are against the working tree at the time of writing.

**Superseded (later work).** The label-rendering design below was reworked afterwards and much of it no
longer matches the code. Nicknames and bomb-site letters now use the current vgui scheme's standard
text font ("EngineFont", read via `TextRenderInterface::GetFontDesc`) with its baked drop shadow, not
the custom Tahoma face with a dilated keyline this plan describes; the keyline glyph path has been
removed from the text service entirely. Nicknames render slightly smaller than the standard text and
are truncated to 16 codepoints; bomb sites match the standard size. Players whose projected position
leaves the radar are drawn as outward direction arrows with name and HP hidden. Client-side marker
easing was dropped in favour of the engine's own entity interpolation (smooth out-of-PVS teammates
require the server-side `nclapi_force_team_visibility` cvar added in NextClientServerApi). Read the
sections below as the history of the keyline-era design, not the current one.

**This document was rewritten after the rasterizer moved into the engine.** Its first version
designed a radar-private glyph atlas (`RadarFont`) living in `client_mini`. That module no longer
exists: rasterization, atlas packing and the glyph cache are now owned by `next_engine_mini` and
reached through `TextRenderInterface`. Everything about how glyphs are produced therefore lives in
`docs/archive/engine-text-service-plan.md`, and is not repeated here. What remains here is the radar side:
how a label is laid out, sized, coloured and drawn.

---

## 1. Why this was done

### 1.1 Cost

Labels went through `pfnDrawConsoleString`, which reaches the vgui surface. Before the atlas work,
each glyph there was its own GL texture with its own bind and its own immediate-mode quad
(`FontReplace.cpp`, since rewritten), so 32 players plus 2 bomb sites cost roughly 380 binds and
draws per frame. Note this was NextClient's own code, not the stock engine: stock GoldSrc packs
glyphs into 256x256 atlas pages and batches them, and our vtable replacement had given both up in
exchange for per-glyph font fallback.

That cost is now addressed twice over, independently: `FontReplace` batches through shared pages,
and radar labels no longer enter the vgui path at all.

### 1.2 Size

`pfnDrawConsoleString` is hard-wired to `EngineFont` and takes no font parameter. `EngineFont` is
Verdana weight 600 with `dropshadow 1` on a five-step `yres` ladder
(`assets/cstrike/resource/TrackerScheme.res:397-437`), and the alternative engine faces are larger.
Against the radar dimensions from `RadarView_ComputeBox`:

| Resolution | `box.elem` | dot diameter | old label height | ratio |
|---|---|---|---|---|
| 1024x768 | 153.6 | 6 px | 15 px | 2.50x |
| 1920x1080 | 216.0 | 8 px | 21 px | 2.63x |
| 2560x1440 | 244.8 | 10 px | 25 px | 2.50x |

A 12-character nickname at 1080p spanned roughly 60-67% of a 216 px radar, so several labels could
not coexist. This is the half the engine API could not solve, and it is why the radar needed its own
text path even after the cost problem was fixed.

---

## 2. What shipped

| Step | Commit | Notes |
|---|---|---|
| 1. Cvar `hud_radar_name_size` | `d84f439` | hidden tuning cvar, default 1.0, range 0.6-2.0 |
| 2. Glyph rasterizer and atlas | `d84f439`, superseded by `a943e5d` | began as radar-private `RadarFont`; moved to `next_engine_mini` |
| 3. Glyph quad in the prim pipeline | `d84f439` | `kShapeGlyph = 6`, atlas sampling in `radar_prim.frag` |
| 4. `RadarText`: decode, layout, draw | `d84f439` | UTF-8 decode, per-slot layout cache, centred draw |
| 5. Switch nicknames to it | `d84f439` | verified in game |
| 6. `text_ms` metric | `188fa5d`, `22751a5`, `6ac97bf` | diagnostics overlay; harness re-baseline still outstanding |

Migration of the radar onto the engine service is `a943e5d`; `RadarFont.{h,cpp}` were deleted there.

---

## 3. Architecture as built

Only the radar side is described. For fonts, atlas pages, cell packing and the fallback chain see
`docs/archive/engine-text-service-plan.md`.

**`src/hud/radar/RadarText.{h,cpp}`** owns everything between a nickname and a quad:

- A file-local UTF-8 decoder that always advances at least one byte, rejects overlong forms,
  surrogates and out-of-range scalars, and emits `U+FFFD` on error.
- A control-character filter mapping `< 0x20`, `0x7F` and `0x80..0x9F` to a space, reproducing the
  engine's `iswprint` behaviour so those positions do not become missing-glyph boxes.
- A per-slot layout cache keyed on (slot, text, device size, atlas generation). The slot is the
  player index, so a steady-state frame does one `strcmp` per label and no decode, no measurement
  and no allocation. The generation term is what makes a repack in the service safe.
- One memoized font handle per pixel size. The service never releases fonts, so a naive create per
  frame would grow its table without bound; the size clamp below caps the memoized set at about
  twenty handles.
- Device-pixel snapping of the run origin. The cells are rasterized hinted at their size, and a
  fractional origin would resample that away.

**`RadarDraw_DrawGlyph`** appends one glyph cell to the radar's own prim batch as shape id 6. The
per-vertex UV rides in `vTri.xy`, which the fragment shader reads only inside the triangle branch,
so no new vertex attribute was needed. Fill colour travels in `aColor` and the keyline colour in
`aCenter.xyz`; the fragment composites fill over keyline and applies the flashbang wash to the
result, which is algebraically the same as washing each layer.

Consequence worth stating: radar glyphs are inside the radar's batch, so the radar clip and the
flashbang wash apply to them per fragment. That is why the engine interface has a query half at all,
and why the radar does not use the draw half.

**`PlayersLayer::DrawNames`** draws health bars first and names second, because health bars are
engine immediate-mode fills (`pfnFillRGBABlend` is one `glBegin`/`glEnd` per rectangle, three per
bar) and queued glyph quads would otherwise composite underneath them. Step 10 removes that
constraint. It falls back to `RadarLabel_Draw` when `RadarText_Available()` is false.

---

## 4. Sizing and contrast

`name_px = clamp(lround(box.elem * kNameHeightScale * clamp(name_size, 0.6, 2.0)), 9, 28)` with
`kNameHeightScale = 0.055f`, about 2.4 times the player dot.

| Resolution | new | old | reduction |
|---|---|---|---|
| 1024x768 | 9 px (floor binds) | 15 px | 40% |
| 1920x1080 | 12 px | 21 px | 43% |
| 2560x1440 | 13 px | 25 px | 48% |

Derived from `box.elem` rather than literal pixels because every other radar dimension already is
(`kPlayerDotScale = 0.023f`, `kDeathCrossScale = 0.0162f`), because `box.elem` carries the
high-resolution damping from `RadarView`, and because `hud_radar_scale` is live-editable from the
settings dialog, so a literal size would strand names when the radar is resized.

Contrast is a 1 px symmetric dark keyline baked into the atlas, 2 px at 18 px and above, not a drop
shadow: the map rotates under a static label, so a directional shadow has no stable background
direction to exploit, and at 9-13 px a shadow asymmetrically thickens 1 px stems and reads as blur.
The keyline is restricted to texels reachable from the cell border, so a dilation cannot close the
counter of a round glyph.

---

## 5. Remaining steps

### Step 7 - Migrate the RadarDebug overlay. **Cancelled.**

The original plan routed the overlay's own text through `RadarText`. That is now a bad idea and the
recommendation is reversed.

The overlay exists to look at the text pipeline, among other things. If it draws through that
pipeline, a fault makes the instrument unreadable exactly when it is needed. It currently draws
through `pfnDrawConsoleString`, an independent path, and that independence is worth more than the
consistency. The `RadarFont_LineHeight` call the original step prescribed no longer exists either.

Left on the engine text path. If the duplication ever becomes a real cost, revisit.

### Step 8 - Migrate bomb-site letters, keeping `RadarLabel` (`d1b11bb`)

`ObjectivesLayer::DrawSites` still calls `pfnDrawConsoleStringLen` twice (a per-frame `"x"` height
probe and a per-site measure) and draws through `RadarLabel_Draw`. Route it through `RadarText` at a
site tier from `kSiteHeightScale = 0.093f`, which reproduces the current ladder within a pixel at
768 and 1080 and one pixel low at 1440. Wrap the site loop in one `BeginPrims`/`EndPrims`.

The original step then deleted `RadarLabel.{h,cpp}`. **Do not.** `RadarLabel_Draw` is the fallback
at `PlayersLayer.cpp:570` when `RadarText_Available()` is false. That mattered less when the
rasterizer was in the same module; now it lives behind a DLL boundary and an interface factory, so
it has more ways to be unavailable, and the fallback is the difference between a radar with
engine-font names and a radar with no names. Keep it, and keep `kLabelFlashHide` with it.

Bomb sites are deferred behind the nicknames deliberately: they are 2 labels of 34 and carry none of
the performance argument, so the risky swap ships against nicknames alone.

**Built.** Sites route through `RadarText` at slot range 70+ (their own, not the player indices), with
`RadarLabel_Draw` kept as the fallback. Verified in game.

### Step 9 - Soften the flashbang cut (`db111ec`)

Replace the hard `flash_alpha >= 120` skip with a smoothstep ramp over `[90, 200]`, reaching
approximately zero well before 255. The label alpha multiplies `uColor.a`, so fill and keyline ramp
together.

The hard cut existed only because the console font's drop shadow is baked black and cannot be
recoloured. With an atlas-owned, independently fadeable keyline that constraint is gone.
`RadarFade::NearWhite` gating is untouched, so tinted death and round-transition fades still do not
wash the radar. Note the fallback path keeps the hard cut, since it still draws the console font.

**Built** as `RadarLabel_FlashFade`, a smoothstep over `[90, 200]` shared by names and sites; the
batched path multiplies label alpha by it, the fallback keeps the hard `>= 120` cut. Verified in game.

### Step 10 - Batch the health bars (`27725d1`)

Not text, but it belongs here: it is in the same function, it is the larger cost, and step 5 already
had to work around it.

`PlayersLayer::DrawHealthBar` draws three `pfnFillRGBABlend` rectangles per player (border,
background, filled portion). In the reference engine `Draw_FillRGBABlend` is a bare
`qglBegin(GL_QUADS)` / four vertices / `qglEnd()` wrapped in six state changes: disable texture,
enable blend, texenv, blendfunc, colour, then restore. There is no batching and nowhere to add it,
because the engine's 2D fill API has no flush entry point for a deferred batch to drain into. The
engine half is **INFERRED** from the reverse-engineered reference, but the absence of a flush point
is structural and holds independently of it.

At 32 players that is 96 immediate-mode quads and roughly 1250 GL calls per frame. For scale, the
nickname path cost about 380 binds and draws before any of this work and now costs one additional
`glDrawArrays`. **Health bars are therefore very likely the most expensive part of the radar in
GL-call terms, more so than names ever were.**

**Does:** routes the three rectangles through `RadarDraw` as `kShapeSolid` quads, which the prim
pipeline already supports and which the polyline path already uses. Removes the CPU-side
`RadarDraw_FadeColor` calls in `DrawHealthBar`, since the shader applies the wash.

**Files:** `src/hud/radar/layers/PlayersLayer.cpp`, possibly one small helper in `RadarDraw`.

**Unlocks:** the pass split in `DrawPlayerLayers` exists only because the bars are engine immediate
draws that would otherwise composite over queued glyph quads. Once batched, bars and names can share
one `BeginPrims` region and the split can go.

**Behaviour changes, both wanted but both worth eyeballing:** the radar clip and the flashbang wash
start applying to the bars per fragment, where today they do not. The clip is the one to watch: a
marker at the rim is clamped by `RadarView_ClampToRadar` and its bar sits above the dot, so a bar
that currently overhangs the rim will start being cut. Check a full server with `hud_radar_health 1`
and players pinned at the edge.

**Verify:** `batches` in `hud_radar_debug` should not rise, since the bars join existing batches.
The measurable win is in GL calls rather than in any counter the overlay currently shows, so compare
frame time with `hud_radar_health` toggled, holding everything else constant.

**Built.** `RadarDraw_DrawRect` (a `kShapeSolid` quad) replaces the three `pfnFillRGBABlend` calls, and
the bars now share the marker `BeginPrims` region, so the pass split is gone. Verified in game.

### Outstanding from step 6

`radar_shot` golden images containing a label were invalidated by the nickname switch and will be
invalidated again by step 8. `radar_dump` is unaffected, as it captures no text pixels, so marker
regressions stay detectable throughout. The harness lives on the separate `radar-autotests-local`
branch and its images are machine- and resolution-bound, so regeneration must happen on the
originating machine.

---

## 6. Open questions

- **Pixel snapping is done in HUD space, not framebuffer space.** Fullscreen and `-stretchaspect`
  render into an FBO where HUD units are not framebuffer pixels; the radar already compensates for
  this in its blur capture path but the text origin does not. The hinted cells are therefore
  resampled in exactly those modes. Fixing it means threading the viewport ratio into `RadarText`
  and rasterizing at `round(px * sy)`, which is a change to the service's font selection as well.
- **The layout cache is keyed on the slot.** Two different callers reusing one slot index would
  thrash it. Today the only slotted caller is the player pass; bomb sites in step 8 must take their
  own slot range, not reuse player indices.
- **`kNameHeightScale` has not been tuned against real play**, only derived. It is one constant and
  the cvar covers 0.6 to 2.0 around it, so this is a matter of taste rather than a risk.
