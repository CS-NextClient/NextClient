Language: EN | [RU](README.ru.md)

# Life Stats

This is the C++ reference for the [bundled Rust plugin](../rust-life-stats/README.md).
The two implementations have identical metadata, settings and output and share
one plugin ID. Install only one implementation. The C++ DLL is available as the
`plugin_life_stats_cpp` target; the normal client bundle includes the Rust DLL.

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

Standalone C++ builds produce `life_stats.dll`; client test builds name the
reference DLL `life_stats_cpp.dll` and do not install it. Standalone builds use the C++ SDK headers and `taocpp-json`, available through the
same vcpkg setup as the client. The example consumes SDK events and player snapshots
only; it does not use game headers or memory hooks.
