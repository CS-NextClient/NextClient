Language: EN | [RU](README.ru.md)

# Life Stats

The bundled Rust implementation of Life Stats. Its metadata, settings and output
match the [C++ reference](../cpp-life-stats/README.md). Both use the same plugin ID;
install only one implementation. Replacing the DLL requires approval again,
while the existing Output setting is preserved.

Enable **Life Stats** in the Plugins menu and accept its **Display messages in
local chat** permission. Its plugin-specific Output control needs no UI permission.
The plugin reports damage and kills at death or round end, with names, weapons,
and fatal headshots from death messages.

In **Options → Plugins → Life Stats**, set **Output** to **Console** (the default)
or **Console + Chat**. Console output includes a timeline of received damage
updates and kills, with millisecond offsets before death or round end. HP lost
and overkill are counted separately. Optional chat output shows a compact
summary with a green `[Life Stats]` label. Apply/OK saves the choice for future
sessions. Switching to Console clears any queued chat lines.

When the server sends ReGameDLL-compatible death extensions, Life Stats also
counts reported assists and adds kill details (through wall/smoke, no-scope,
blindness, airborne kills, flash assists, domination and revenge) and assister
names to the console timeline. Chat remains compact. Ordinary servers work
without these fields; no missing-data notices or invented assist totals are shown.

Console example:

```text
[Life Stats] Life ended. Damage taken: 100 HP, 0 armor. Overkill: 91 HP. Kills: 0.
[Life Stats] Damage taken -96 HP (-00:05.405)
[Life Stats] Self-inflicted damage (grenade) -4 HP (91 HP overkill) (Death)
```

Each life is reported once. A death report is not repeated at round end.
New lives and map changes clear previous totals. Ordinary HUD refreshes, such as
`fullupdate`, preserve the active life's totals. Spectating does not start a life.
The plugin waits 0.25 seconds for the final damage update, prints the report to
the console, and, when enabled, displays chat lines 0.6 seconds apart. All output
is local; the plugin does not request permission to send chat or change gameplay.

Reports and player snapshots wait until queued events have been delivered. If
the host reports lost events, the current totals are discarded with a local
warning. Tracking resumes after the backlog drains and a new spawn, round, or
map begins.

The fatal damage update is matched to the local death message for its attacker
and weapon. Earlier updates use the received damage flags, when provided;
standard servers filter most damage types out of these flags. A later suicide
does not identify the source of earlier damage.
Stock servers can combine several hits into one Damage message, so the timeline
records received updates rather than claiming individual bullet accuracy.
Known health and healing updates are used to cap HP loss and calculate overkill.
At most 4,096 timeline entries and 512 named kills/assists are retained per life; totals
continue accumulating. A missing round-end signal does not generate a late report
at the next round's start.

## Build

The normal Windows NextClient `BUILD_ALL` build compiles this Rust plugin and
includes it as `plugins/life_stats.dll`, disabled until approved. Install Rust
1.85 or later and its `i686-pc-windows-msvc` target before configuring NextClient.
CMake finds Cargo and rustc on PATH or beside each other; portable installations
can set `NEXTCLIENT_CARGO_EXECUTABLE` and `NEXTCLIENT_RUSTC_EXECUTABLE`.
The client build also links its existing Windows 7 compatibility object.
Linux client builds omit the native plugin runtime and this DLL.

For a standalone Windows x86 build, with the MSVC linker and Windows SDK installed:

```powershell
rustup target add i686-pc-windows-msvc
cd sdk/examples/rust-life-stats
cargo build --release --locked
```

Copy `target/i686-pc-windows-msvc/release/life_stats.dll` to `plugins/`.
The included Cargo configuration selects x86 and a static CRT. Keep
`panic = "unwind"` to contain callback panics. The plugin uses the public Rust SDK
and `serde_json`, with no C++ source, game headers or memory hooks.

`plugin-tests` runs the same behavior suite against the Rust and C++ DLLs,
compares their manifests, replays events to compare output bytes and delivery
timing, and checks saved settings when replacing either implementation.
