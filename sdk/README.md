Language: EN | [RU](README.ru.md)

# NextClient plugin SDK 1.0

Build native **Windows 10+ x86 DLLs** in Rust or C++. Both SDKs use the same
versioned C ABI. Plugin projects do not need the game source, HLSDK, VGUI headers,
or a private C++ bridge. Rust needs the standard MSVC linker/Windows SDK tools,
but plugin source is entirely Rust. API 1 provides permissions, declarative settings,
console commands, copied game state, frame callbacks, cvars, HUD visibility,
text/rectangle drawing, local sounds, game events, chat, connection control,
and per-plugin JSON stores. SDK, ABI, and API versions are 1.0.0 / 1 / 1.

## Install and trust

The runtime currently supports Windows only. Linux client builds omit plugins, their menus,
and their hooks; Windows DLLs cannot be loaded by the Linux client.

Put plugin DLLs in `plugins/` beside `cstrike.exe`. Open **Plugins** from the main
or pause screen. Review name, self-declared author, description, version, SDK
warnings, and compatibility rules; enable plugins and arrange their order.
**OK saves and restarts the game only when the selection or order changed**;
it is disabled when there are no pending changes. **Cancel** discards staged changes.
Invalid dependencies, conflicts, or duplicate IDs block
saving an enabled configuration. Recommended order is an explicit action;
advisory rules never prevent a manual order from being saved.

Discovery reads the `.nclmeta` PE section without executing DLL entry points.
New or changed DLLs are disabled until explicitly enabled. Approval records the
complete DLL's SHA-256, filename, and plugin ID. The host checks the hash again
and holds a file handle denying writes and replacement while the DLL is loaded.
Disabling/reordering also takes effect only after restarting.

First enable opens a localized popup with the plugin name, author, and every
required permission. **Allow and enable** accepts all requests; **Cancel** leaves
the plugin disabled. Approval is only committed by the manager's OK button.
The same approved DLL can be disabled and enabled again without another prompt.
Updating/replacing a DLL requires new consent. Grants are recorded alongside its
hash; approval requires explicit permission grants in the profile.

Native plugins have the privileges of the game process. This is consent, not a
sandbox or signature check. SDK callback errors retire the plugin and its required
dependents; native faults, memory corruption, and `panic=abort` can still crash the
game. Start with **`-noplugins`** to recover without loading plugins. Remove this
flag on a later launch to use plugins again. No live unload is offered.

Approvals/order are stored in `plugins/profile.json`; settings are stored in
`plugins/.host/settings/plugin-<id>.json`. Saves use atomic file replacement. The directory must be
writable. Metadata, authorship, and compatibility claims are self-declared. The
manager shows Active, Disabled, Pending Restart, or Blocked status. Pending edits
show a restart notice; undoing all edits clears it. Technical compatibility details
appear only when there is an issue or applicable ordering advice.

## Recovery and isolated settings

An unclean-session marker offers **Start without plugins** on the next normal
launch. In Options > Plugins, select a suspected plugin, click **Disable**, then
**OK** to save and restart. Disable any plugins that require it as well. Saving
the selection acknowledges the failed session. **OK** also acknowledges recovery
without changing the selection or restarting, including when the list is empty.
The marker is created only before the first plugin module is loaded. Plugins can
be enabled again through the same dialog; changed package hashes still require approval.

Settings migrate from the former aggregate `plugins/settings.json`. Migration
preserves `settings.json.migrated.bak`, resumes without replacing existing owner
files, and isolates invalid owners. Files up to 16 MiB can be considered for
migration, subject to parser memory admission. Each save validates the same
1 MiB size, nesting and integer-setting schema as the reader before replacement.
Valid predecessors use `.bak`; unreadable originals use `.damaged`. A corrupt
owner uses a readable backup or defaults without disabling other plugins. Settings
transactions are atomic per owner; a multi-owner Apply can partially succeed on
an I/O error. `plugins/.host` is reserved for host configuration.

Crash reports attach `session.json` (IDs, versions, hashes and initialization
order) and a bounded `session-trace.txt` callback history, plus the previous
session copies. Active initialization/callback/module-unload context identifies a **suspect**,
not a proven cause; native worker faults may have no attributable callback.

## Permissions

Omitting `permissions` means no additional permissions. Unknown or duplicate
permission names block loading. The host checks permission on each impactful API
call; declaring a `capabilities` entry does not grant permission.

| Permission | Allows |
| --- | --- |
| `ui.settings` | Add settings tabs and controls to plugin or built-in pages |
| `ui.draw` | Receive gameplay draw callbacks and draw rectangles/text |
| `ui.hide` | Hide the HUD, crosshair, health, radar, or death notices |
| `player.write` | Apply changes to outgoing buttons, view angles, and movement |
| `cvars.read` | Read existing console variables and subscribe to value changes |
| `cvars.write` | Change existing console variables |
| `audio.play` | Play local WAV sounds |
| `cvars.create` | Register console variables in the plugin's namespace |
| `chat.read` | Subscribe to received game chat |
| `chat.send` | Send public or team chat |
| `chat.print` | Display text in the local HUD chat without sending it to the server |
| `connection.connect` | Connect to a server, including replacing the current connection |
| `connection.disconnect` | Disconnect from the current server |
| `messages.read` | Subscribe to raw server messages |
| `messages.filter` | Filter supported chat/screen presentation messages, with read permissions |
| `ui.windows` | Create host-owned windows, widgets, textures, fonts, and menu entries |
| `ui.input` | Receive keyboard and mouse input in plugin windows |
| `services.call` | Call declared plugin dependencies and subscribe to their topics |

Console command registration/output, plugin-owned settings, debug logging, frame callbacks,
session/player/entity/weapon snapshots, extended game data, connection reads,
public game events, the plugin's own JSON store, player information, screen projection,
and text measurement are available without additional permissions.
All permission requests are independent: cvar write does not grant cvar read, and drawing
does not grant permission to hide the HUD. Denied calls return false/zero or a Rust
error/None. Input callbacks may observe input without `player.write`, but their
changes are discarded. The SDK does not expose local health, inventory, or position
setters. Native DLL permissions govern SDK access; they cannot sandbox arbitrary
native code or prevent a DLL from bypassing the SDK using operating-system APIs.

## Rust

Copy `examples/rust-events` into your project and point its path dependency at
`sdk/rust` (or a private vendored copy). From the example directory:

```powershell
rustup target add i686-pc-windows-msvc
cargo build --release
```

The included `.cargo/config.toml` selects x86 and statically links the CRT. Copy
`target/i686-pc-windows-msvc/release/nextclient_rust_events.dll` to `plugins/`.
Use `Plugin`, `Host`, `Control`, and `export_plugin!`; see the complete example.
No crates.io dependencies, bindgen, build script, or C++ source are required.
Keep `panic = "unwind"` so the SDK can contain panics at callback boundaries.

```rust
fn load(&mut self, host: &Host<'_>) -> Result {
    host.subscribe_event("player.health", true)?;
    host.register_command("status")
}
```

## C++

The SDK is header-only. Include `cpp/include/nextclient/plugin.hpp`, derive from
`nextclient::Plugin`, declare `NC_MANIFEST(...)`, and export with `NC_PLUGIN(Type)`.
Use C++17 or later with exceptions enabled. No host import library is needed.

```powershell
cmake -S sdk/examples/cpp-life-stats -B build/plugin-example -A Win32 -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x86-windows-static
cmake --build build/plugin-example --config Release
```

Copy the resulting `life_stats.dll` to `plugins/`. `BUILD_ALL` packages this C++
example, disabled until approved. It requests only `chat.print` and uses the
project's existing `taocpp-json` dependency to read event payloads. The SDK itself
is header-only and does not require a JSON library.

Life Stats reports received damage totals, kills, and the killer named by a death
message after the local player dies or the round ends. Known weapons and fatal
headshots are included. Console output is immediate; local chat lines are spaced
out. Nothing is sent to other players. See [the example](examples/cpp-life-stats/README.md).

## Settings and callbacks

Register tabs and controls during `load` only. Tab IDs are scoped to their owner.
Tabs/controls require `ui.settings`. Register non-UI integer settings with
`register_setting(id, initial, min, max)` without that permission. A control
registers its own setting automatically; reuse of a separately registered setting
must have matching defaults and bounds. `set_setting` saves immediately and does
not invoke `setting_changed` recursively. It only addresses the calling plugin's
registered settings. It is not undone by closing Options with Cancel.
A control may target the plugin's own tab or a built-in anchor: `multiplayer`,
`game`, `keyboard`, `mouse`, `audio`, `video`, `voice`, `miscellaneous`. Existing
tabs gain **Standard / Plugins** sections; custom tabs appear at the top level.
Scrollable controls and an overflow tab dropdown keep larger extensions reachable.
Built-in anchor names are reserved and cannot be used for custom tabs.

Controls support checkboxes, bounded integer sliders, indexed dropdowns, and
action buttons. Labels/dropdown entries support English and Russian, falling back
to English. Dropdown choices are newline-separated, indexed from zero. Settings
are namespaced by plugin ID. Register defaults, then call `setting` during load.
`setting_changed` follows a successful Options Apply/OK save. Cancel preserves
saved settings. Action buttons run immediately and are not undone by Cancel.

`command` receives a mutable copy of selected outgoing command fields and a
read-only player snapshot. Callbacks run after original input and mouse inversion,
in plugin load order. `CAN_JUMP` excludes ladders, deep water, water jumps,
frozen/dead players, and spectators; also check `VALID` and `ACTIVE`. No engine
pointers or server physics are exposed. A failed callback loses its command edits
and is retired for this run.

Register a console command during `load` with `register_command("inspect")`.
Its public name is `nc.<plugin-id>.inspect`; plugins cannot replace existing engine
commands or another plugin's commands. `console_command` receives the local ID and
arguments excluding the command name. Commands stop dispatching when their plugin
fails. Registration can be queued until client initialization completes.
Local command IDs contain 1–48 lowercase ASCII letters, digits, underscores, or
hyphens; dots are excluded to keep plugin namespaces unambiguous.

`player()` returns local health, armor, weapon ID/inventory bits, position, velocity,
view angles/offset, FOV, movement type, water level, and movement flags.
`entity(index)` returns the current client snapshot of an entity, including its
model, transform, velocity, bounds, owner, team, animation sequence, and effects.
Only current network entities are returned. Entity health is valid only when
`NC_ENTITY_HEALTH` / `ENTITY_HEALTH` is set (currently the local player).
`weapon(id)` returns local predicted clip, ownership, reload/state flags, and attack
timers for weapon IDs 1–63. Remote health and inventories not transmitted by the
server are unavailable; team/velocity fields reflect what the server actually sends.
No engine pointers are exposed. Snapshots are unavailable before prediction, after
a map reset, or while disconnected. The SDK does not provide a world/entity write API.

`read_cvar` distinguishes a missing/denied cvar from an empty string. The C ABI uses
a caller-owned buffer and returns the required byte count including NUL; short
buffers are cleared, never truncated. `write_cvar` sets an existing cvar through
the engine's cvar setter, preserving its behavior. It never executes console text
and cannot create cvars. Cvar changes follow engine persistence rules and are not
automatically undone when a plugin stops.

`hide_ui(element, true)` requests hiding one element: `NC_UI_HUD`,
`NC_UI_CROSSHAIR`, `NC_UI_HEALTH`, `NC_UI_RADAR`, or `NC_UI_DEATH_NOTICES`
(Rust uses the same names without `NC_UI_`). Health means the health display;
armor, ammo, and damage indicators remain separate. Death notices covers both
classic and custom rendering.
Pass false to release that plugin's request. Requests from active plugins combine;
one plugin cannot undo another's request. Failure/shutdown releases its requests.
The Plugins menu remains reachable. Arbitrary VGUI panels are not exposed.

`draw` receives screen dimensions, time, and intermission state after normal HUD
rendering. Only within that callback may `draw_rect(x, y, width, height, rgba)` be
used, with color `0xRRGGBBAA`. `draw_text(x, y, text, rgb)` uses `0xRRGGBB`
and the engine console font, without alpha. Text and rectangles preserve call order,
are queued until the callback succeeds, and are discarded if it fails.
`measure_text(text)` returns the same font's width/height without drawing or needing
permission. Text is a single line, up to 1024 bytes; tabs/newlines are rejected.
Glyph coverage and encoding follow the engine font. Host-owned windows support
installed fonts and textures through [the UI interface](EXTENSIONS.md#host-owned-ui).
3D rendering is not exposed.

`frame(session)` is eligible once per GameUI frame, including disconnected menu
frames. A rotating scheduler may skip owners when the callback budget is spent;
plugins must tolerate missed frames (see [dispatch limits](EXTENSIONS.md#events-tasks-and-storage)).
It needs no permission. When the client bridge is bound, `session()` obtains the same
kind of copied state: `CONNECTED`/`IN_GAME` flags, max clients, screen dimensions,
client time, frame interval, and map path (for example `maps/de_dust2.bsp`).
The frame interval uses the latest HUD frame delta, is zero until the first HUD
frame, and is clamped to 0-1 seconds. Disconnected snapshots clear map/client time/max clients.
Snapshots can be stored by value. Subscribe to `map.changed` and
`connection.changed` for transition notifications.

`player_info(index)` returns copied name/model, ping, packet loss, local-player
and spectator flags for a populated player slot. Iterate `1..=max_clients`;
empty/disconnected slots return false/None. Names/model/map are bounded,
NUL-terminated arrays; Rust provides `name()`, `model()`, and `map_name()` helpers.
`world_to_screen([x, y, z])` projects world coordinates to screen pixels using the
current camera. Points behind the camera return false/None; successful points may
be off-screen. Projection is most useful inside `draw`, after the camera update.

`play_sound("buttons/blip1.wav", 0.5)` requires `audio.play`. Paths are relative to
the game's `sound/` search path, use forward slashes and ASCII letters/digits,
underscores, hyphens or dots, and must end in `.wav` (up to 240 bytes).
Absolute paths, empty/dot/traversal segments, non-finite volumes, and volumes
outside 0-1 are rejected. Successful submission does not guarantee the asset
exists. This plays locally; it does not send a server command or install assets.

`console_print("Ready")` needs no permission and adds no plugin ID prefix.
Up to 4096 bytes are accepted. Plugins supply their own user-facing label.
Color bytes `\x01` (default), `\x03` (blue), and `\x04` (green) are supported;
for example, `"\x04[Life Stats]\x01 Ready"`. Percent format markers remain literal
text, never commands. Output ends with a newline.
Like other client services, output is unavailable until the client bridge is bound;
a plugin loaded earlier can retry from `frame`. Messages are not queued.
`log()` is a permission-free debugger diagnostic, not in-game console output.
Arbitrary console execution is deliberately absent because it could bypass the
separate permissions for changing cvars and player input.

Callbacks and ordinary host functions run on the game thread. The
[extension interfaces](EXTENSIONS.md) provide asynchronous storage and a worker-safe
posting function. Only that posting function may be called from workers. Do not
retain borrowed callback pointers/references or use the host after shutdown.
Copied snapshot values may be retained.
Shutdown runs in reverse load order. A failed load rolls back registrations and
defers unload of partial initialization until shutdown, keeping its DLL and context alive. Join workers during unload. C++ exceptions
and Rust panics must not cross the ABI; the high-level SDKs contain them.
Rust callbacks receive a borrowed `Host` argument, available only for that call.
Host operations reject calls from worker threads and retired plugins. Join workers
during unload; host services are unavailable during unload.

## Events, full client data, engine actions, and storage

See [Game API reference](game-api.md) for event names and payloads, game-data
sections, availability, limits, and permission rules.

- `subscribe_event(name, enabled)` delivers named JSON events to `event(name, json)`.
- `game_data(section, index)` returns copied JSON covering client prediction,
  network entity state, weapon definitions/prediction, ammo, scoreboard, match/HUD,
  and connection details. Only data actually received by the client is available.
- `create_cvar(id, initial, archive)` registers a namespaced cvar during load.
  `watch_cvar(name, enabled)` delivers `cvar.changed` after actual changes.
- `send_chat(text, team)`, `connect(host, port)`, and `disconnect()` use separate
  permissions. Connection state reads need no permission.
- `store_get`, `store_set`, `store_delete`, and `store_keys` persist structured JSON
  under `plugins/data/plugin-<plugin-id>.json`. Calls always address the caller's
  store, with no permission needed. Settings and this store are independent.

`examples/rust-events` is a Rust-only example with no requested permissions or
external crates. It saves a startup counter and the last health event, and registers
`nc.org.nextclient.events.status` to print connection data. Build it from that
directory with `cargo build --release --locked`.
JSON is transported as UTF-8 strings, so plugin authors can choose their own parser.

## Manifest and compatibility

Embed one NUL-terminated UTF-8 JSON manifest, at most 64 KiB, with the SDK macro.
The exported static preserves the section in optimized Release DLLs.

```json
{
  "schema": 1,
  "id": "com.example.movement",
  "name": "Movement tools",
  "author": "Example author",
  "description": "Adds movement preferences.",
  "translations": {
    "ru": {
      "name": "Инструменты движения",
      "description": "Добавляет настройки движения."
    }
  },
  "version": "1.2.0",
  "sdk": "1.0.0",
  "abi": 1,
  "api": 1,
  "capabilities": ["settings", "command"],
  "permissions": ["ui.settings", "player.write"],
  "compatibility_revision": 2,
  "requires": [{"id":"com.example.core","version":">=1.0.0 <2.0.0","reason":"Needs the core behavior"}],
  "conflicts": [{"id":"com.example.other_movement","version":"*","reason":"Changes the same input"}],
  "after": [{"id":"com.example.camera","version":"<2.0.0","reason":"Camera adjusts angles first"}],
  "before": []
}
```

IDs use lowercase ASCII letters, digits, dots, dashes, and underscores (1–96
characters). Versions use `major.minor.patch`, without prerelease/build suffixes.
Ranges support `*`, an exact version, or space-separated `=`, `<`, `<=`, `>`, `>=`
comparisons combined with AND. Cargo/npm shorthand such as `^`, `~`, or `||` is
not accepted. Relation/capability arrays are optional. Increment
`compatibility_revision` when changing compatibility recommendations.

`name` and `description` are required fallback strings. Optional `translations`
maps language tags to translated `name` and/or `description` fields. The manager
uses the current UI language, then its base language (`ru-ru` → `ru`), and falls
back to the original field if its translation is missing or empty. Currently the
host ships English and Russian UI resources. Tags use lowercase letters, digits,
and hyphens (1–16 characters); at most 32 languages may be included. Names are
limited to 128 UTF-8 bytes and descriptions to 4096 bytes in every language.
Both Rust and C++ embed this manifest format. Manifests without translations use
the fallback strings in every language.

Required plugins must be enabled, match the version range, and load earlier.
Dependencies are never enabled silently. Hard cycles block affected plugins and
their dependents; unrelated plugins can still load. Conflicts block both plugins.
Before/after advice applies only to an enabled plugin in the declared version
range. Recommended order uses a stable topological sort. A recommendation cycle
is reported and leaves order unchanged. Versioned services and topics are available
through `nextclient.services`; callers must declare their providers in `requires`.

`sdk` identifies the author's build SDK. A different SDK version warns when ABI/API
remain compatible. Unsupported schema, ABI, required API, capability, or
architecture blocks loading. This host supports ABI 1/API 1 and the `settings`
and `command` capabilities. Structs use fixed-width values, default C layout, and
`cdecl` (`extern "C"` in Rust, **not** `extern "system"`). No language ABI, STL
container, exception, or allocator-owned object crosses the boundary. New layouts
require a new ABI version. Strings are copied during registration.

Plugins can use self-contained DLLs or packages. A package uses
`plugins/<package>/plugin.dll` as its entry point, with uniquely named companion
DLLs in the same directory and assets in subdirectories. Approval covers every package file.
The loader locks the approved files, checks x86 imports and name conflicts,
and loads companions in dependency order without enabling directory searches.
See [package rules and extension APIs](EXTENSIONS.md).

Limits: 256 discovered DLLs, 64 MiB per DLL, 64 KiB manifest, 16 tabs, 128 controls
and settings, 32 console commands per plugin, and 4096 drawing operations plus
64 KiB of text per draw callback.
Names and strings have the additional bounds documented in the headers/parser.

## Validation

`plugin-tests` tests compatibility rules, metadata, binary approval, locked files,
safe mode, persisted settings, consent and permission validation, denied and granted APIs,
worker-thread rejection, event filtering/reentrancy, protocol parsing, store isolation/durability,
chat/connect validation, sound/path validation, frame failure, ordered text/rectangle
drawing and rollback, and real DLL callbacks. Set
`NEXTCLIENT_RUST_EVENTS_DLL` to the built events example's absolute path to
exercise Rust event callbacks and storage across restarts. Run SDK unit tests with:

```powershell
cargo test --manifest-path sdk/rust/Cargo.toml --target i686-pc-windows-msvc
```

The CI workflow builds the C++ and Rust examples and runs their integration tests.
