# Client module

`src/main.cpp` connects the stock client callbacks to `GameHud`, the view code and weapon
behavior. The radar wrapper lives in `src/hud/HudRadar.cpp`.

## Ownership and callback order

- `GameHud` owns the HUD components. Keep component state with that owner and reset it through
  the existing HUD lifecycle instead of adding another process-wide cache.
- Trace `HUD_Redraw`, `HUD_VidInit`, `HUD_Reset`, `HUD_InitHUDData` and shutdown separately when
  changing state lifetime. A round reset, video reset and map transition invalidate different data.
- Keep the explicit `Sys_Error` cleanup in `src/main.cpp`: this exit path skips `Host_Shutdown`
  and therefore `HUD_Shutdown`. GL resources and hook subscriptions must be released while the
  engine and its context are still available, including on this path.
- Restore patched callbacks and remove subscriptions before destroying the state they call.
  C callback sinks must be null outside their owner's lifetime.

## Stock client boundaries

- The stock `client.dll` (`client.so` on Linux) has its own engine function table, reached through
  NitroApi's `ClientData::gEngfuncs`. It is distinct from this module's `gEngfuncs` copy. A hook intended to
  intercept stock HUD drawing must patch the former; changing the latter misses those calls.
- When wrapping a draw callback, preserve its return value and state changes as well as its
  pixels. String width, final cursor position, glyph advance and ambient text color have different
  contracts across the engine's text entry points.
- Determine observer mode and observed player from the live client state. A local player's
  state, an observed entity and a server broadcast can describe different players or moments;
  do not substitute one for another without checking the recipient and update rules.
- Use existing SDK constants and NitroApi declarations for engine facts. Use `ncl_math` types inside
  owned calculations; convert raw engine vectors at the boundary. Keep `main.h` ahead of legacy
  SDK headers that depend on its declarations.

## Verification

Build `client_mini`. Shared gameplay logic has `view-tests` and `crosshair-tests`; run the affected
cases when changing those packages. This module has no dedicated test target. Keep geometry,
policy and buffering decisions independent of the engine so they can be tested. Engine callback
fakes verify arguments, return values and ordering; they do not verify the actual GL output.

For public header or callback changes, also build the affected consumers. Changes to shared
gameplay definitions can affect both `client_mini` and `gameui`.
