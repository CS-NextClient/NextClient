# Shared text renderer

`text_render.cpp` implements the engine-owned `TextRenderInterface`; `glyph_atlas` stores its GL
cells. `text_surface.cpp` adapts the stock `VGUI_Surface026` object. The public interface lives in
`nextclient/engine_mini/include/next_engine_mini/TextRenderInterface.h`.

## Font and glyph contracts

- Consumers use the shared interface. Font handles are memoized and are not released individually;
  do not create an unbounded set of fonts by deriving a new size on every frame.
- A cached laid-out run must carry `Generation()` and be rebuilt when it changes. Repacking the
  atlas invalidates cached texture coordinates even if the font handle has not changed.
- `GetGlyph` returning null is transient, so retry later. A non-null glyph with `renderable == false`
  is the permanent result for an empty or unavailable glyph. Preserve this distinction in caches.
- Fallback faces are tried in insertion order. Metrics must come from the face that supplies the
  glyph; the primary face's ascent is not a substitute for `GetGlyphAscent`.

## Surface adapter

- The stock client and engine reach the shared surface through their own pointers. Keep the
  vtable adapter on that object and restore its original slots before teardown. Slots and calling
  conventions belong to `VGUI_Surface026`; do not transplant another surface version's layout.
- Adapter handles and native font handles are different. Route every native fallback operation
  through the mapping in `text_surface.cpp`, including measurements and font selection.
- The atlas supports antialiasing and drop shadows. Unsupported flags, blur, scanlines or restricted
  character ranges retain the native path for that font. Adding a glyph set must preserve the
  original call's parameters and result, and an unsupported set must not silently lose its effect.
- Measurement and drawing must use the same backend. Keep text position and color synchronized
  across native and custom calls, including mixed runs and additive character drawing.
- Preserve panel origin and clipping when converting surface coordinates to glyph quads. Buffering
  must not move text across intervening draw operations and change its order on screen.

## GL lifetime and verification

Keep GL cleanup in `TextRender_ReleaseGl`, before drawing shutdown. Restore touched state after
each draw, including texture unit zero's binding, blend state and current color. Atlas textures use
the coverage format documented on `TextGlyphFormat`; changes must preserve shadow modulation.

`engine_mini-tests` includes `TextSurfaceTests.cpp` for adapter routing and metrics. These tests do
not prove rendered appearance. Existing runtime diagnostics are `text_service_info`,
`text_surface_info` and `text_atlas_dump`; extend them when additional runtime evidence is needed.
