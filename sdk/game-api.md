Language: EN | [RU](game-api.ru.md)

# SDK 1.0 game API

SDK 1.0.0 provides these interfaces for C++ and Rust through ABI 1, API 1.
The C ABI uses copied UTF-8 JSON and caller-owned buffers;
Rust returns `Option<String>`, and C++ fills a `std::string` and returns `bool`.
No engine pointers, STL objects, or Rust objects cross the ABI. Snapshot reads
return zero/None/false when unavailable. Unknown fields should be ignored.

## Events

Message-derived events and cached fields require a consumer registered through
the wrapped stock-client or client_mini engine table. Messages registered only
through other engine tables are not observed. In particular, `HealthInfo` and
`Account` have no fallback registrations, so updates from those messages require
an observed consumer too. Engine-backed snapshots and connection,
player join/leave, voice, and cvar notifications do not depend on this observer.

Call `subscribe_event("player.health", true)` during load or a later callback.
Disable with `false`. Implement `Plugin::event(name, json)` in C++ or
`fn event(&mut self, host: &Host<'_>, name: &str, json: &str) -> Result` in Rust.
Subscriptions are per plugin, exact-name, idempotent, and do not replay earlier
events. Unknown names fail. Callbacks run on the game thread in a rotating,
budgeted event pass before the frame-callback pass. Each owner's queued events
retain their order; delivery across owners is not in plugin load order. Queued
events may wait for later frames when the pass reaches a limit. Events emitted
by a callback wait for a subsequent frame; engine message parsing and cvar setters
never invoke plugin callbacks directly. See [dispatch limits](EXTENSIONS.md#events-tasks-and-storage).

All events below are permission-free except `chat.message` (`chat.read`) and
`cvar.changed` (`cvars.read`). Subscribe to the latter with `watch_cvar`, which
filters by variable name, rather than `subscribe_event`. Permission is checked
both when subscribing and when delivering. A failed callback retires the plugin
and its required dependents. Events observe state; they cannot cancel engine behavior.

| Event | JSON payload fields |
| --- | --- |
| `player.health` | `health` for local HUD health; or `player`, `reported_health` for a spectator target or server scoreboard update (null when hidden) |
| `player.armor` | `armor` or `armor_type`, depending on the received message |
| `player.money` | `money`, `flash` for local HUD money; or `player`, `reported_money` for a server scoreboard update (null when hidden) |
| `player.damage` | `armor`, `health` damage amounts, `bits`, `position` |
| `player.death` | `killer`, `victim`, `player` (victim), `headshot`, `weapon`; optional `death_flags`, `death_position`, `assister`, `kill_flags`, `kill_details` (see below) |
| `player.score` | `player`, `frags`, `deaths`, `class`, `team_id` |
| `player.team` | `player`, `team` |
| `player.attributes` | `player`, `dead`, `has_c4`, `vip`, raw `attribute_flags`, `has_defuser` (true when advertised, otherwise null) |
| `player.location` | `player`, and either `location` or `radar_position` |
| `player.status` | `id`, and either `value` or `text` from the status bar |
| `player.fov` | `fov` (raw HUD value; 0 means engine default) |
| `player.weapon` | `state`, `id`, `clip` (-1 when absent) |
| `player.ammo`, `ammo.pickup` | Ammo `id`, `amount`; a pickup is a notification, not the reserve total |
| `weapon.pickup` | Weapon `id` |
| `item.pickup` | Item `name` |
| `weapon.definition` | `id`, `name`, `ammo_type`, `ammo_max`, `ammo2_type`, `ammo2_max`, `slot`, `slot_position`, `flags` |
| `round.time` | `seconds`, `received_at` |
| `round.start` | Observed round transition: HLTV reset, or ResetHUD/RoundTime initially or after a round result/restart; may precede freeze-time end |
| `round.reset` | Local ResetHUD notification; also occurs on spawn, not exclusively at round start |
| `round.end`, `match.reset` | Recognized server message token in `reason` |
| `match.team_score` | `team`, `score` |
| `match.mode` | `mode` |
| `bomb.dropped` | `position`, `planted` |
| `bomb.picked_up` | No additional fields |
| `hostage.position` | `id`, `active`, `position` |
| `hostage.killed` | `id` |
| `hud.reset`, `hud.init` | No additional fields |
| `hud.status` | `icon`, `state`, and `r`, `g`, `b` when enabled |
| `hud.hide` | Engine HUD visibility `flags` |
| `hud.progress` | `seconds`, `percent`, `received_at` |
| `hud.flashlight` | `battery`, optionally `enabled` |
| `hud.nightvision` | `enabled` |
| `hud.fade` | `duration`, `hold` in seconds, `flags`, `r`, `g`, `b`, `a` |
| `hud.shake` | `amplitude`, `duration`, `frequency` |
| `player.joined` | Current scoreboard row; a slot became populated or changed user ID |
| `player.left` | `index`, `user_id`; a slot became empty or changed user ID |
| `connection.changed` | Connection snapshot described below; initial state is also reported |
| `map.changed` | `old`, `map` paths; empty path means leaving the map |
| `voice.state` | `player`, `talking`; engine-local voice indices can be -1 or 0 |
| `chat.message` | `sender`, `format`, `arguments`, `text`, `team` (null if unknown) |
| `cvar.changed` | Lowercase `name`, `old`, `value` strings |

Message-derived events include client `time`. Round-end events recognize the CS
win/draw/bomb/hostage/VIP system tokens; arbitrary server text never becomes a
permission-free event. Chat includes received SayText and HUD_PRINTTALK TextMsg,
with original format tokens and arguments retained. It does not intercept chat
input or voice audio. Custom message formats are not inferred.

Malformed/truncated messages are ignored by the observer without changing its
snapshots. Events are best-effort: each plugin has a queue of at most 2,048 events
and 1 MiB of JSON. Additional events are dropped and reported through `sdk.overflow`;
dispatch budgets and diagnostics are described in [extension interfaces](EXTENSIONS.md).
Public message payloads
are at most 64 KiB. Cvar changes may hold two values of up to 65,535 bytes each.
Events are notifications, not a reliable replay log; use snapshots for current state.

### Optional ReGameDLL-compatible messages

No ReGameDLL installation or version detection is required. Standard messages
are parsed independently of optional fields. Extended fields are parsed only when received;
another server addon may send the same documented format. Missing fields are
unknown, not zero or false. Do not assume all deaths include the same extensions.

After the standard `DeathMsg` weapon string, an optional unsigned 32-bit
`death_flags` selects these fields, in wire order:

| Bit | Field | Meaning |
| --- | --- | --- |
| `0x001` | `death_position` | Three coordinates of the victim's death, not a bullet impact or live player position |
| `0x002` | `assister` | Player index 1–32, or 0 for explicitly no assister |
| `0x004` | `kill_flags`, `kill_details` | Unsigned 32-bit rarity flags and decoded booleans |

`kill_details` contains `headshot` (0x001), `killer_blind` (0x002), `noscope`
(0x004), `penetrated` (0x008), `through_smoke` (0x010), `assisted_flash` (0x020),
`domination_began` (0x040), `domination` (0x080), `revenge` (0x100), and `in_air`
(0x200). The top-level `headshot` remains the standard message byte. Raw flag
words preserve unknown bits; unknown trailing data is not interpreted. A truncated
advertised extension invalidates the message without partially updating snapshots.
Stock messages have none of these extension fields. Explicit zero flags are kept.

`HealthInfo` and `Account` carry a player index followed by signed 32-bit health
or money. Values are retained at full width, including zero and values above 255.
The server's -1 visibility sentinel becomes null and replaces any cached value.
Other negative values remain signed server data. These updates never overwrite
local HUD health/money or imply a damage event.
`ScoreAttrib` preserves all attribute bits; bit 0x008 advertises a defuse kit.
An unset bit cannot distinguish no kit, hidden kit, and an unsupported server,
so `has_defuser` becomes null rather than claiming false. Round starts clear
cached reported health, money, defuse-kit information and raw attribute flags; player reuse, HUD
initialization and map/server changes also invalidate these values.

ReGameDLL controls these extensions through `mp_deathmsg_flags` and
`mp_scoreboard_showhealth`, `mp_scoreboard_showmoney`, `mp_scoreboard_showdefkit`.
The client does not query or change those settings. See the upstream
[message writer](https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/dlls/multiplay_gamerules.cpp),
[player updates](https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/dlls/player.cpp)
and [configuration](https://github.com/rehlds/ReGameDLL_CS#configuration-cvars).

Ordinary `Damage` contains byte-sized damage amounts, HUD-filtered damage
flags and the damage source position. It does not identify the attacker, weapon,
body part or outgoing damage; several hits can be combined into one update.

## Full client data

`game_data(section, index)` exposes copied client-visible state. It does **not**
claim access to server-only state, hidden enemies, private inventories, or reliable
remote health/velocity that the server has not transmitted. Network/prediction
objects preserve HLSDK field names and values; mod-specific user fields remain raw.

| Section | Index | Result |
| --- | --- | --- |
| `player` | Ignored | Local typed-snapshot fields, full `clientdata_t` in `client`, full predicted `entity_state_t` in `entity`, copied movement fields in `movement` (gravity, friction, hull, ground/water/duck state, timers, vectors, texture and mod user fields), `shots_fired`, and cached `hud` state |
| `entity` | Entity index | All `entity_state_t` fields, `render_origin`, `render_angles`, `model_name`, `is_player`, `known_health` |
| `entities` | Ignored | Array of currently available entity indices; query each with `entity` |
| `weapon` | Weapon ID 1–63 | `id`, `owned`, `definition`, full `weapon_data_t` in `prediction`, `reserve`, `reserve2` |
| `weapons` | Ignored | Array of all known weapon definitions/predictions |
| `ammo` | Ignored | Array of 32 ammo reserve values; unknown entries are null |
| `scoreboard` | 0 for all, or player index | Populated rows: `index`, `user_id`, `steam_id` (string), `name`, `model`, `ping`, `packet_loss`, `local`, `spectator`, `frags`, `deaths`, `class`, `team_id`, `team`, `dead`, `has_c4`, `vip`, `attribute_flags`, `has_defuser`, `location`, `reported_health`, `reported_money`, `radar_position` |
| `match` | Ignored | Client `time`, `map`, `paused`, `intermission`, `view_entity`, `game_type`, `round_seconds_remaining`, plus cached message state |
| `connection` | Ignored | `state`, `connected`, `in_game`, `address`, `map`, `max_clients`, `hostname`, `demo_playback`, `demo_recording`, `latency`, `packet_loss`, `connection_time`, and `remote_address` when connected |

Connection data is available whenever the client bridge is bound, even in menus.
State names are `disconnected`, `connecting`, `connected`, `active`,
`uninitialized`, `dedicated`, or `unknown`. This API excludes passwords and raw
userinfo/serverinfo dictionaries. It needs no connection or cvar permission.

Other sections require an active game and valid prediction. Entity enumeration
includes only entities in the current network frame. `known_health` is reliable
only for the local player; raw remote entity fields are not proof of known health.
Scoreboard fields and weapon definitions/reserves may be null until received.
Player slots are invalidated on departure/reuse; map/server changes reset cached
data. Weapon definitions survive HUD initialization because signon can send them
first. Reads return at most 1 MiB; a short C buffer is cleared and the required
size including NUL is returned. Read size and contents on the same game thread.

Cached HUD/match state uses the event name as its key, for example
`player.money.money`, `player.armor.armor`, `round.time.seconds`, or
`bomb.dropped.planted`. Separate dictionaries are `team_scores`, `status_icons`,
`hostages`, `StatusValue`, and `StatusText`. These are last-received values, not
invented defaults or a complete event history. Chat is never included. HUD reset,
new-round, bomb pickup/drop, and map reset invalidate relevant cached state.

Snapshot limits: `match.team_scores` retains at most 32 names and
`match.status_icons` at most 64 active icons. Names over 64 UTF-8 bytes and new
entries beyond those caps are omitted from snapshots; their events are still
delivered. Existing entries can be updated at capacity. Disabling an icon removes
it, freeing its slot. HUD reset clears icons; HUD initialization and map resets clear both dictionaries.

## Engine actions

`chat_print(text)` requires `chat.print` and displays one line in the local HUD
chat. It does not call `say`/`say_team`, send network traffic, notify chat subscribers,
or echo to the console. Call permission-free `console_print` separately when both
outputs are desired. Text is 1–190 UTF-8 bytes, including optional chat color bytes:
`\x01` (default), `\x03` (team), and `\x04` (green). Other control characters and
newlines are rejected. The client must be in an active game; being dead does not
prevent local output. `chat.send` and `chat.read` do not
grant `chat.print`, or vice versa.

`create_cvar("counter", "0", true)` requires `cvars.create`, runs during load,
and registers `nc.<plugin-id>.counter`. Local IDs use 1–48 lowercase ASCII letters,
digits, underscores, or hyphens; at most 32 cvars per plugin. Defaults are up to
4,096 bytes. Only the archive flag is exposed. Commands/existing cvars cannot be
replaced. Registration occurs after successful load, or when the client binds;
registration failure blocks/retires that plugin. Engine cvars last until engine
shutdown; they are not removed when a callback fails.

Reading, writing, and watching cvars all accept names up to 160 bytes, including
the full `nc.<plugin-id>.<local-name>` namespace (up to 148 bytes).

Creating a variable does not grant reading, watching, or writing it. `read_cvar`
and `watch_cvar(name, enabled)` require `cvars.read`; `write_cvar` requires
`cvars.write`. Watches are case-insensitive, exact-name, up to 64 per plugin, and
may precede cvar creation. No initial value event is generated. Events reflect
actual changes through the engine setter, including console/config writes; equal
values and rejected changes do not generate events. Direct memory writes by
native code cannot be observed reliably.

`send_chat(text, team)` requires `chat.send` and a live non-demo connection. Text
is up to 190 UTF-8 bytes; empty strings, control characters, quotes, backslashes,
and semicolons are rejected to prevent command injection. `team=true` uses team
chat. Receipt needs `chat.read` independently. Server chat restrictions still apply.

`connect(host, port)` requires `connection.connect`; `disconnect()` requires
`connection.disconnect`. Connect may replace the current connection. Host is an
IPv4 address or ASCII DNS name (up to 253 bytes), and port is 1–65535. No URLs,
inline port, passwords, raw commands, or IPv6 syntax are accepted. C++ defaults to
27015; Rust takes `u16`. Commands are submitted to the engine; success means
accepted for execution, not that a connection succeeded. Observe connection state
to determine the outcome.

## Per-plugin persistence

`store_set(key, json)` accepts any valid JSON value, including strings, arrays,
objects, booleans, numbers, and null. `store_get(key)` returns its serialized JSON;
a missing key returns None/false/zero, distinct from stored JSON `null`.
`store_delete(key)` is idempotent. `store_keys()` returns a sorted JSON array.
Keys use 1–96 lowercase ASCII letters, digits, dots, underscores, or hyphens.

Each stable plugin ID has its own `plugins/data/plugin-<id>.json`. No API accepts
another owner's ID or a filesystem path. Saves survive game restarts, disabling,
and DLL upgrades using the same ID. Settings-panel Cancel does not undo store
writes. This is SDK namespace isolation, not an OS sandbox for native DLLs.

Limits: 64 KiB input JSON per value, 1,024 keys, 1 MiB serialized store, and 16
JSON container levels including the store object. Writes flush a temporary file
and atomically replace the old file; failed writes leave the in-memory and saved
value unchanged. Corrupt stores fail calls without silently overwriting the file.
The game directory must be writable. These APIs are synchronous on the game thread;
batch changes and avoid saving every frame. For atomic multi-key transactions,
the [storage interface](EXTENSIONS.md#events-tasks-and-storage) provides background
commits to the same per-plugin store. There is no database, shared store, or
arbitrary file API.
