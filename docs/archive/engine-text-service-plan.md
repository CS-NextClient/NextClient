# Engine Text Service Plan

Status: proposal. Supersedes an earlier plan (since removed) that proposed a shared package under
`nextclient/packages/` on the mistaken premise that `vgui2.dll` could not link glad. It can; that was
a case of reading "does not today" as "cannot". The package approach is also the wrong shape: it would
have left the facility owned by the consumers rather than by the engine.

Companion document to `docs/archive/radar-text-rendering-plan.md`, which covers the radar side: layout,
sizing, colour and the glyph quad. Glyph production (fonts, rasterization, atlas pages, the fallback
chain) is described here and not repeated there. Line anchors are against the working tree at the
time of writing.

Progress: COMPLETE. All six steps shipped and verified in game; vgui2_surfacelib is deleted.
See section 8.

---

## 1. The architecture being restored

Stock GoldSrc puts font rasterization and glyph atlasing in the **engine**, and consumers ask for text
through an interface:

- `engine/vgui2/FontTextureCache.cpp` owns the 256x256 atlas pages and the `(font, wch)` cell cache.
- `engine/vgui2/BaseUISurface.cpp` implements `ISurface` and draws text on behalf of callers.
- `vgui2.dll` and `client.dll` only call `DrawSetTextFont` / `DrawSetTextPos` / `DrawSetTextColor` /
  `DrawPrintText`. They own no glyph storage.

NextClient currently has the inverse, and has it twice:

| | `vgui2.dll` | `client_mini.dll` |
|---|---|---|
| File | `FontReplace.cpp` (SDK submodule) | `RadarFont.cpp` + `RadarText.cpp` |
| Owns | 256x256 RGBA pages, cell cache, packer, batch, clip | 512x512 LUMINANCE_ALPHA page, cell cache, packer |

(Written before the work started. The `client_mini` column is now historical: `RadarFont` was deleted
in `a943e5d` and the radar sources cells from the service. The `vgui2` column still describes the
tree, because step 5 was reverted.)

`FontReplace` exists for a real reason: per-glyph font fallback, which the stock engine cache does not
do. But it achieved that by moving the engine's job into a consumer, and the radar then repeated the
move independently. This plan puts both back where the original architecture had them.

**One structural fact makes this clean, and it was not obvious before investigating:**
`vgui2_surfacelib` (`CFontManager` / `CWin32Font` / `CFontAmalgam`) is already **rasterization-only and
renderer-agnostic**. `CWin32Font::GetCharRGBA` hands back a CPU pixel buffer and knows nothing about
textures. The atlas is not in it. So the font manager and the atlas are already separable, and only
the atlas half is in the wrong place.

---

## 2. Where it goes

`next_engine_mini.dll` is NextClient's engine layer and is the correct home:

- It already contains the engine graphics layer: `nextclient/engine_mini/src/graphics/` holds
  `gl_draw.cpp`, `gl_rmain.cpp`, `gl_vidnt.cpp`, `texture_loader.cpp` and `qgl.hpp` (the engine's GL
  entry-point table, including `qglGenTextures`, `qglBindTexture`, `qglTexSubImage2D`,
  `qglTexImage2D`, `qglPixelStorei`, `qglDeleteTextures`).
- It already owns the engine-side vgui2 text shims: `src/graphics/vgui2.cpp` currently holds
  `VGUI2_Draw_Init`, `VGUI2_ResetCurrentTexture`, `VGUI2_Draw_SetTextColor` — the same file the stock
  engine calls `engine/vgui2/text_draw.cpp`.
- It already exposes a versioned interface through the standard factory
  (`EXPOSE_SINGLE_INTERFACE(EngineMini, EngineMiniInterface, ENGINE_MINI_INTERFACE_VERSION)`,
  `src/engine.cpp:718`), which `gameui.dll` obtains with
  `Sys_GetFactory("next_engine_mini.dll")(ENGINE_MINI_INTERFACE_VERSION, NULL)`
  (`nextclient/gameui/src/GameUi/GameUi.cpp:179`).
- It *could* link `vgui2_surfacelib` with a one-line CMake change, since `surfacelib` is a plain
  `STATIC` target depending only on `tier0`/`tier1`/`vstdlib`, all of which `engine_mini` already
  links transitively via `nitro_api` -> `hl1_source_sdk`. **This is recorded only to show there is no
  technical obstacle. The plan does not do it** - see section 3.

New files: `nextclient/engine_mini/src/graphics/text/` containing the font manager, the glyph cache,
the atlas packer, the page allocator and the draw path. Public header:
`nextclient/engine_mini/include/next_engine_mini/TextRenderInterface.h`, following the existing
pattern of `HttpDownloadManagerInterface.h` and friends.

There is a second reason to choose `engine_mini`, independent of the architectural argument: **it is
the module that survives every option in Phase 4 of `docs/valve-code-removal-plan.md`.** That phase is
a decision point between reimplementing the vgui2 stack (A), migrating GameUI to CEF (B), and shipping
Valve's stock `vgui2.dll` while moving NextClient's features into runtime patches (C); the recommended
path is a B+C spike. Under C, NextClient stops building `vgui2.dll` at all and `FontReplace` becomes a
detour into the stock binary. A text service living in `vgui2.dll` - the status quo - would have to be
rebuilt or re-hosted under that outcome. One living in `engine_mini` would not: the detour would
consume the same interface.

---

## 3. surfacelib is replaced, not relinked

`vgui2_surfacelib` (`CFontManager` / `CWin32Font` / `CFontAmalgam`) must be **deleted**, not moved.
An earlier revision of this plan proposed linking it into `engine_mini`, which was wrong twice over:
it would pull SDK-derived code into a NextClient module and entrench it behind a versioned interface,
and it would survive precisely the Phase 4 outcomes meant to remove it.
`docs/valve-code-removal-plan.md` classifies it as fork code "without Valve notices", but with the
qualifier "**most of** `vgui2/surfacelib`"; the remainder is unaccounted for, and the library is
Valve-architecture code the product is trying to shed either way.

So the engine service implements the font layer first-party. What that costs, concretely:

| What surfacelib provides | Replacement | Cost |
|---|---|---|
| GDI rasterization to a coverage buffer | done, moved out of `RadarFont.cpp` into the service: `CreateFontIndirectW` + `GetGlyphOutlineW(GGO_GRAY8_BITMAP)`, metrics from `GLYPHMETRICS`/`TEXTMETRICW`, a face chain that accepts a substituted family instead of failing | done |
| Per-glyph fallback chain (`CFontAmalgam::GetFontForChar`) | ordered face list, a coverage probe via `GetGlyphIndicesW(GGI_MARK_NONEXISTING_GLYPHS)`, and a per-font char cache | new, small |
| `HFont` handle space (`CFontManager`) | a vector plus a memoizing `FindOrCreateFont` | new, trivial |
| Metrics: ABC widths, ascent, height | GDI directly | done |
| Effects: dropshadow, blur, scanlines, rotary, outline | only dropshadow is used by the shipped schemes (six entries); blur and scanlines appear in none of the four; outline is declared with no implementation in surfacelib either | dropshadow now, rest deferred |
| Foreign-language fallback face tables | a first-party data table | new, data only |

**The single-owner hazard disappears as a side effect**, which is worth stating because it was the
centre of the previous revision. `CFontManager` is a file-scope static in a static library
(`FontManager.cpp:22`) and `HFont` is a raw index into its `m_FontAmalgams`
(`FontManager.cpp:316-320`), so two linking modules would have produced two index spaces and silently
wrong fonts rather than a link error. With one first-party implementation in one module, that failure
mode cannot occur by construction.

One migration obligation survives regardless: `vgui2.dll` calls `FontManager()` **directly**, not only
through `ISurface`. `vgui_controls/Scheme.cpp:783` calls `FontManager().GetForeignFallbackFontName()`.
Every such call must be routed through the new interface before surfacelib can be deleted. This is the
single most likely thing to be discovered late and should be enumerated exhaustively early.

---

## 4. Draw or query? Both, and the split is not negotiable

Stock `ISurface` only draws. That is sufficient for `vgui2`, and it is **not** sufficient for the
radar: radar glyphs must land inside the radar's own shader batch, where the flashbang wash
(`uFadeColor`/`uFadeAmt`) and the radar clip are applied per fragment. A draw-only interface would
force the radar back onto a separate draw call per label and lose both effects.

So the interface has two halves:

- **Glyph half (query).** Fonts in, atlas cells out: a texture name, texture coordinates and metrics.
  The radar consumes this and emits its own quads. This half is the general facility.
- **Text half (draw).** Built on the glyph half, reproducing what `FontReplace` does today: the quad
  batch, the GL state bracket, the panel clip and the flush points. `vgui2` consumes this.

The radar could in principle also use the draw half, and does not, for the reason above. That is a
deliberate asymmetry, not an oversight, and should be commented as such at the interface.

### 4.1 Atlas pages are partitioned by format, not shared as one texture

"A shared atlas" must not be read as one texture object. The radar page is `GL_LUMINANCE_ALPHA` with
`GL_LINEAR` (alpha = fill coverage, luminance = a dilated keyline); `vgui2` pages are RGBA with GDI
effects already baked in. Those cannot coexist in one texture, and the filter is per-texture state.

What is shared is the **packer, the cell cache and the allocator**, over page lists partitioned by
cell format. Format and filter become fields of the font descriptor, fixed at font creation and
stamped onto each page. The keyline pass stays gated at its single call site, so `vgui2` text pays
nothing for it.

Note also that pages must be created with `qgl*` directly rather than through `ISurface`:
`ISurface` exposes only `DrawSetTextureRGBA` and has no luminance-alpha upload path, which is why the
current in-`vgui2` design could not have hosted the radar format even if asked to.

---

## 5. Interface sketch

```cpp
// next_engine_mini/TextRenderInterface.h
enum class GlyphFormat { Rgba, CoverageKeyline };

struct FontDesc
{
    char face[32];
    int tall;
    int weight;
    int blur;
    int scanlines;
    int flags;          // FONTFLAG_*
    GlyphFormat format;
};

struct GlyphCell
{
    unsigned int texture; // GL texture name of the owning page
    float u0, v0, u1, v1;
    int wide, tall;
    int bearing_x, bearing_y;
    int advance;
    bool renderable;      // false = the face has no such glyph
};

class TextRenderInterface
{
public:
    // Glyph half.
    virtual HFont FindOrCreateFont(const FontDesc& desc) = 0;
    virtual bool AddGlyphSet(HFont font, const char* face, int tall, int weight, int blur, int scanlines, int flags) = 0;
    virtual void ResetGlyphSets(HFont font) = 0;
    virtual const GlyphCell* GetGlyph(HFont font, uint32_t codepoint) = 0; // nullptr = transient, retry
    virtual uint32_t Generation() = 0;

    // Metrics, as ISurface needs them.
    virtual int GetFontTall(HFont font) = 0;
    virtual int GetFontAscent(HFont font, wchar_t ch) = 0;
    virtual void GetCharABCwide(HFont font, int ch, int& a, int& b, int& c) = 0;
    virtual int GetCharacterWidth(HFont font, int ch) = 0;
    virtual void GetTextSize(HFont font, const wchar_t* text, int& wide, int& tall) = 0;

    // Text half.
    virtual void SetClipRect(int x0, int y0, int x1, int y1) = 0;
    virtual void ClearClipRect() = 0;
    virtual void DrawRun(int x, int y, HFont font, const wchar_t* text, int len, const int rgba[4], bool additive) = 0;
    virtual void Flush() = 0;
};

#define TEXT_RENDER_INTERFACE_VERSION (folded into EngineMini009)
```

Design notes that came out of verification and should stay attached to the interface:

- `GetGlyph` returning `nullptr` for a **transient** failure (atlas full) versus a `renderable == false`
  cell for a **permanently absent** codepoint is a distinction the radar implementation had to be
  patched for after review. Encoding it in the interface prevents the next consumer repeating it.
- `Generation()` is a pull-only invalidation counter. It is not strictly needed today, because a video
  mode change is a whole-process relaunch (`OptionsSubVideo::OnApplyChanges` issues `_restart`, and
  the launcher spawns a fresh process), but it is cheap insurance if an in-process `vid_restart` ever
  returns.
- `FindOrCreateFont` **must memoize**. `CFontManager::CreateFont` never dedups and nothing is ever
  freed, so the radar picking a pixel height per frame through a naive `CreateFont` would grow the
  handle table without bound. The radar's existing 9..28 px clamp bounds the memoized set at about
  twenty handles.
- Returning a raw GL texture name across the DLL boundary is safe here: both modules share the one GL
  context, and the radar already binds engine-owned texture names in its blur path.

---

## 6. Migration

Sequencing note: the radar text files (`RadarFont.*`, `RadarText.*`) are **untracked and uncommitted**,
so migrating the radar first costs no history churn and touches no shipped behaviour. `vgui2` is the
riskier consumer because it renders every menu, the console and the scoreboard.

1. **Fix the reload growth first.** `CScheme::ReloadFontGlyphs` appends a duplicate face set on every
   call (`FontManager::AddGlyphSetToFont` ends in an unconditional `AddFont`), and the in-process
   reload paths (`CSchemeManager::ReloadFonts`, repeat `LoadSchemeFromFileEx`, the Ctrl+Shift+Alt+R
   hotkey) are independent of the mode-change relaunch. `CFontAmalgam::RemoveAll` already exists, so
   this is a few lines. Do it before the interface is frozen, or the growth behaviour gets baked into
   a versioned contract.
2. **Stand up the service.** Add `src/graphics/text/` to `engine_mini`: the first-party font manager
   of section 3 (rasterizer moved out of `RadarFont.cpp`, plus the fallback chain, the handle space
   and dropshadow), the packer and cache moved out of `RadarFont.cpp`, and `TextRenderInterface`.
   `surfacelib` is not linked. No consumer yet. Verified by a console command that bakes a tier and
   dumps the atlas, and by a side-by-side comparison of the fallback chain against surfacelib's on a
   Cyrillic and a CJK string.
3. **Migrate the radar.** `RadarFont.{h,cpp}` is deleted; `RadarText.cpp` keeps its layout cache, its
   size-tier policy and its centred draw, and sources cells from the service. `RadarDraw`, the shader
   and `PlayersLayer` are untouched. Verified in game against the pre-migration build.
4. **Enumerate and route vgui2's direct surfacelib calls.** *Enumeration done; the result is narrower
   than this plan assumed.* Exactly two files in `vgui2/` reach surfacelib at all:

   - `FontReplace.cpp`, 9 sites, all of them inside the ISurface vtable hooks that step 5 replaces
     anyway. They map onto the interface directly: `CreateFont` -> `FindOrCreateFont`;
     `AddGlyphSetToFont` -> font creation plus `AddFallbackFace`; `GetFontTall` -> `GetLineHeight`;
     `GetFontForChar` and `GetPrimaryFont` become internal to the service's own chain resolution;
     `GetCharABCwide`, `GetCharacterWidth`, `GetTextSize`, `GetFontAscent` are metrics the interface
     must grow (they are already in the section 5 sketch).
   - `Scheme.cpp`, 2 sites: `ResetGlyphSets` (added by step 1) and `GetForeignFallbackFontName`.
     Only the second is genuinely outside the surface hooks, and it needs one accessor.

   So the gate is one new accessor plus the four metric methods. Nothing was found that cannot be
   expressed through the interface, which was the risk this step existed to test.
5. **Migrate vgui2.** `FontReplace.cpp` keeps its vtable patching and its clip reconstruction, drops
   its packer, cache and batch, and forwards to the text half. Its own atlas goes with them.
6. **Delete surfacelib.** With no caller left, remove `vgui2_surfacelib` from `vgui2`'s link line and
   delete the library. This is the step the whole plan is aimed at, and it is only reachable once
   step 4 has routed every direct `FontManager()` call.

---

## 7. Risks and open questions

- **The radar text path has never been run in game.** Steps 2 and 3 refactor code whose correctness is
  unestablished, so a refactor bug and an original bug will be indistinguishable. Running the current
  build first is the cheapest way to remove that ambiguity, and is recommended before step 2.
- **An undiscovered direct `FontManager()` call in `vgui2` blocks step 6.** Step 4 exists to find them
  all; if any cannot be expressed through the interface, surfacelib cannot be deleted and the design
  needs revisiting.
- **Fallback-chain parity is the real risk of replacing surfacelib**, not the rasterization.
  `CFontAmalgam::GetFontForChar` walks faces in insertion order, takes the first whose `HasChar` is
  true, and falls back to the first in range so the missing-glyph box still renders. Reproducing the
  ordering and that last-resort rule exactly is what keeps foreign-language text looking the same;
  step 2's verification should compare the two implementations directly rather than assume.
- **Phase 4 of the removal plan is still an open decision**, and this plan is written to be robust to
  it rather than to predict it. If option C is chosen, step 5 changes shape: `FontReplace` becomes a
  detour into Valve's stock `vgui2.dll` instead of source we compile. The interface and steps 1 to 4
  are unaffected, which is the point of hosting the service in `engine_mini`.
- **A dropped ordering barrier.** `FontReplace::FlushBatch` binds through
  `ISurface::DrawSetTexture`, and the engine's `drawSetTexture` also drains the engine's own pending
  text queue on a texture change. An engine-side flush binding with raw `qglBindTexture` +
  `VGUI2_ResetCurrentTexture` loses that barrier, because `VGUI2_ResetCurrentTexture` only zeroes the
  cached texture id and flushes nothing. This is benign **only** because vtable slots 18 to 21 are all
  patched, leaving the engine text queue permanently empty. That invariant is currently written down
  nowhere; it must be commented on the engine-side flush, because unpatching any text slot later
  re-opens an ordering inversion. This is INFERRED from the reverse-engineered engine reference and
  should be confirmed in game.
- **`staticTextureCurrent` staleness.** The engine's `drawSetTexture` also updates a texture record
  that `VGUI2_ResetCurrentTexture` does not touch. A raw-bind engine flush leaves it stale. Also
  INFERRED; verify in game rather than trusting the reference.
- **Interface shape is driven by exactly two consumers**, and the second (step 5) has not been
  attempted. If it needs changes to `TextRenderInterface`, that is expected; the service should
  absorb them rather than the consumers working around it. The version string exists for that.

---

## 8. Progress

| Step | State |
|---|---|
| 1. Fix the reload growth | Shipped, SDK `385a94a` |
| 2. Stand up the service | Shipped, `b18a9fd`; GL routed through the engine table in `16538cd`; interface grown for vgui2 in `0ed3b79` |
| 3. Migrate the radar | Shipped, `a943e5d`, verified in game |
| 4. Enumerate vgui2's direct surfacelib calls | Done; result folded into step 4 above |
| 5. Migrate vgui2 | Shipped after a revert+re-apply, verified in game (SDK `741b920`..`069f80ed`) |
| 6. Delete surfacelib | Shipped, SDK `054c226` |

Diagnostics for the service landed separately in `d60a6c7`: the console commands `text_atlas_dump`
(writes each page to a TGA, coverage on the left half and luminance on the right) and
`text_service_info` (fonts, faces, cells, pages, generation, and a per-font breakdown). These were
step 2's prescribed verification, added late; their absence is why the step 5 failures were chased
by guesswork rather than measurement.

### Post-completion changes

Follow-on work after this plan was marked complete, kept here so the interface description above stays
honest:

- **`GetFontDesc` added to `TextRenderInterface`; version bumped `EngineMini008` -> `EngineMini009`.**
  It reads back a font's face/tall/weight/flags so the radar can mirror a vgui scheme font (its HFont is
  the same handle as a `TextFont_t` in the shared service) at its own sizes. Radar nicknames and
  bomb-site letters now render in the scheme's standard "EngineFont" (with its baked drop shadow)
  instead of a hardcoded Tahoma face.
- **The keyline glyph path was removed.** `TextGlyphFormat::CoverageKeyline`, `BuildKeyline`/`MarkOutside`,
  the keyline branch in `Bake`, the thin-keyline constants and the `FontInfo.keyline` field are gone; the
  only remaining format is `Coverage` (plain, or with a baked drop shadow). It was the radar's own
  contrast device and no consumer produces it anymore now that radar labels use the scheme font's shadow.
  The atlas filter is therefore always `GL_NEAREST`, and `text_atlas_dump`'s right half is plain luminance.

### Step 5: reverted once, re-applied with two fixes

The first attempt (`c9109c6`, reverted) had two defects.

**Drop shadow was not implemented.** `kTextFontDropShadow` was stored on the font and never applied,
while surfacelib baked it into the glyph. Six entries in the shipped schemes use it, including the
chat font, so chat text lost its shadow. **Fixed** in `61065ed`: `Bake` now offsets a black copy of
the glyph into the two-channel cell, and `Hook_AddGlyphSetToFont` translates
`FONTFLAG_DROPSHADOW` (0x080) into the service's own flag.

**HUD sprite corruption.** Health, armour, timer and ammo digits rendered as garbled glyph shapes.
The first hypothesis, a stale `currenttexture` cache, was wrong: restoring the binding did not help.
The second, grounded in the radar's own `FlushBatch`, is the **active texture unit**: the radar
saves/sets/restores the unit around its atlas bind and works; the reverted `FontReplace` did not, so
if the engine's HUD pass left a non-zero unit active, the atlas bound to the wrong unit and the next
sprite sampled it. **Attempted** in `6921845`: the flush now forces unit 0 (via a
`wglGetProcAddress`-loaded `glActiveTexture`, since vgui2.dll links only GL 1.1) and restores it.

This hypothesis is **not confirmed**. The reverse-engineered reference shows the engine ending its
world pass on unit 0 (`GL_SelectTexture(TEXTURE0_SGIS)`), which would mean the unit is already 0 and
the fix is a no-op against the real cause. So the re-applied build is best-effort, not a proven fix.
If the corruption survives, reproduce it and run `text_atlas_dump`: correct-looking pages point at
metrics or cell coordinates (`GetCharABCwide`, UVs), wrong pages at `Bake` or the packer. That split
is what the diagnostics exist to make, and it is the next step if verification fails.

### Step 5 resolution (confirmed in game)

Two distinct HUD defects were seen after migrating vgui2, with two distinct causes:

- **Garbled glyph shapes on the number sprites** were the **active texture unit**, as suspected. The
  unit save/set-0/restore around the atlas bind in `FontReplace::FlushBatch` (and the matching one in
  the radar batch) fixed it and holds in game. Kept.
- **A single sprite digit flashing white** for a second or two (the bottom-centre timer `0` in the
  first game) was a **fixed-function colour and blend leak**, not a texture bind. HUD number sprites
  are greyscale and take their colour only from `glColor` set by `SPR_Set`; the glyph batch left
  `glColor` white and did not restore the blend state, so a text flush landing between `SPR_Set` and
  the digit's `Draw_Frame` repainted the digit white. Fixed by snapshotting and restoring
  `GL_CURRENT_COLOR` and the blend enable/func around the batch draw.

Three earlier attempts to cure the white digit by invalidating the engine and vgui texture-bind
caches (`*p_currenttexture = -1`, `VGUI2_ResetCurrentTexture`, an `InvalidateBindCache` call after
every flush) were on the wrong axis and did nothing. They were removed once the colour/blend cause
was found (SDK `2819cba`, NclNitroApi `6710617`, `700c501`). The atlas bind sites are
texture-self-consistent on their own: each snapshots and restores `GL_TEXTURE_BINDING_2D`, so the
binding is never left dangling and no cache invalidation is required.
