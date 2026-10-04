Language: EN | [RU](EXTENSIONS.ru.md)

# SDK 1.0 extension interfaces

SDK 1.0 provides these interfaces through ABI 1/API 1 on Windows 10+ **x86**.
Enable, disable, order, and package updates require a restart.
They do not provide arbitrary engine detours, server-authoritative data the
client never receives, 3D rendering, hot unloading, or native-code isolation.

## Common ABI

`NcHost::query_interface(context, name, 1)` returns an `NcExtension` table or
null for an unknown name/version. Check `size` and `version` before using it.
Interface versions are negotiated independently of the `NcHost` layout.
Plugins can use these tables alongside the core host calls and event callbacks.

`call(context, operation, json)` copies an object argument and executes **once**.
It returns an owner-scoped result handle. `read_result` returns the required
UTF-8 buffer size, including NUL, and copies only when capacity is sufficient.
Reading again does not execute again. Always `release_result` after reading.
Zero means the call was rejected before dispatch (wrong thread, retired plugin,
or outstanding-result quota). Operations return `{"ok":true,...}` or
`{"ok":false,"error":"..."}`; SDK wrappers return this JSON unchanged.
Inputs are limited to 64 KiB and nesting depth 16. Release results promptly:
at most 64 outstanding results and a bounded result pool are allowed per plugin.
Handles are never reused during this process and cannot access another owner.

C++ (`nextclient::Plugin` subclass):

```cpp
auto json = extension("nextclient.events", "stats");
auto commit = extension("nextclient.storage", "commit",
                        R"({"set":{"total":42},"delete":["temporary"]})");
```

Rust (no C++ shim or external crate required):

```rust
let api = host.query_interface("nextclient.events", 1).ok_or(nc::Error)?;
let stats_json = api.call("stats", "{}")?;
host.console_print(&stats_json)?;
let store = host.query_interface("nextclient.storage", 1).ok_or(nc::Error)?;
let accepted = store.call("commit", r#"{"set":{"total":42}}"#)?;
// Parse accepted.ok and accepted.request using your preferred JSON parser.
```

All tables use the same C ABI and UTF-8 JSON schemas. Rust's `Extension` is
borrowed and not Send/Sync. Only the posting function described below is safe
on worker threads. Join all plugin workers before returning from unload.

## Events, tasks, and storage

`nextclient.events/stats` returns `queued`, cumulative `dropped`, `delivered`,
last event `callback_ms`, and `slow_callbacks` (event callbacks above 2 ms).
Each plugin has its own ordered 2,048-event / 1 MiB queue. Dispatch has a soft
2 ms / 128-event budget per plugin per frame. A callback cannot be preempted;
a slow native callback can still stall the game. No health/damage/death
coalescing is performed. New events created during dispatch wait for another
frame. Unsubscribed events are not retained or replayed.

Overflow drops new events and reports `sdk.overflow` out of band with cumulative
`dropped` and the `first`/`last` lost serials since the last report. Serials are
global enqueue serials; the range can include events destined for other plugins.
This is bounded delivery, not a lossless journal. On overflow, refresh snapshots
and invalidate calculations that require complete event history.

`nextclient.tasks/token` returns an opaque `token`. The table's `post(token,json)`
copies a payload from any thread and delivers `sdk.task` to that owner on the
game thread. It returns 0 when the token is stale or the queue is full/stopping.
Tokens expire on plugin failure/unload and are not reused. The global posting
queue is bounded to 1,024 posts / 1 MiB. Rust's `api.post_token(token)` returns a
Send/Sync `PostToken`. Each owner may queue at most 64 posts / 128 KiB. C++ can
retain the function pointer and token without
retaining `NcHost` on a worker.

`nextclient.storage/commit` accepts `{"set":{...},"delete":[...]}`. The
per-plugin JSON store is updated as one atomic transaction by a host worker.
The response supplies a `request`. `sdk.storage` reports `{"request":N,"ok":true}`
or `ok:false` after completion. Only one transaction per plugin may be pending;
concurrent sync/async mutations are rejected. Reads show the last committed
snapshot until the main thread processes completion. Accepted writes drain
before shutdown; a failed write preserves the prior store. Synchronous and
asynchronous persistence share limits of 1,024 keys, 64 KiB per value, and 1 MiB
total. Completion notifications share the event queue;
after overflow, re-read the store before assuming a transaction's outcome.

`sdk.*` notifications are targeted by the host and need no normal event
subscription. Plugins cannot publish a host event using the service broker.

## Server messages and presentation filters

Request `messages.read` to use `nextclient.messages/subscribe` or `unsubscribe`
with `{"name":"CustomMsg"}`. Names are 1–15 ASCII letters/digits/underscores;
up to 128 names per plugin. Registration can precede client initialization.
Custom server messages do not require an update to the bridge's event enum.

`sdk.message` contains `name`, `bytes` (unsigned byte array), client `time`, and
server `epoch`. Payloads above 4,096 bytes are ignored. Bytes are copied before
filters and before the original HUD handler. Stock and optional ReGameDLL
typed events/snapshots continue to parse the original data. `SayText` and
`TextMsg` additionally require `chat.read`. `messages.read` can expose any data
carried in a custom message, so request only subscriptions the plugin needs.

Use the messages table's `set_filter(context,name,callback,user)` to register a
synchronous presentation filter; null removes it. Requires `messages.filter`
**and** the corresponding read permissions. Supported names are `SayText`,
`TextMsg`, `ScreenFade`, and `ScreenShake`. Other engine operations cannot be
cancelled through this interface.

The callback receives immutable input and a separate 4,096-byte output buffer;
`*output_size` initially contains capacity. Return 0 to keep, 1 to replace
(set the output length), 2 to hide, or -1 to report failure. Filters run in load
order; replacements feed the next filter, and any hide decision remains sticky.
Nested message dispatch skips synchronous filters to prevent recursive chains.
Original handlers run at most once. Invalid rewrites retire the offending plugin
and retain the last valid input. ScreenFade must be 10 bytes and ScreenShake 6;
text messages must be bounded and NUL terminated with a valid leading channel or
player byte. Never retain input pointers. Catch C++ exceptions/Rust panics inside
callbacks; the Rust raw filter registration is unsafe because buffer and userdata
lifetimes are the plugin's responsibility.

## Host-owned UI

`nextclient.ui` requires `ui.windows`. Interactive windows and menu entries also
require `ui.input`. Settings controls and HUD drawing have separate permissions.
Passive windows work in menus and gameplay without valid prediction.
The host owns panels, input release, scrolling, textures, and handle cleanup.

`create` accepts this object and returns `handle`:

```json
{
  "title":{"en":"Statistics","ru":"Статистика"},
  "menu":{"en":"Statistics","ru":"Статистика"},
  "surface":"all", "interactive":true, "visible":false,
  "width":480, "height":360,
  "items":[
    {"id":"title","kind":"label","text":{"en":"Display","ru":"Отображение"}},
    {"id":"enabled","kind":"checkbox","text":{"en":"Enabled","ru":"Включено"},"value":true},
    {"id":"reset","kind":"button","text":{"en":"Reset","ru":"Сброс"}}
  ]
}
```

`surface` is `menu`, `game`, or `all`; game windows show while the game menu is
closed. `menu` is optional. Text fields use explicit `en` and `ru` strings.
Other item kinds are `slider` (`min`, `max`, integer `value`), `text` (string
`value`, at most 1,024 bytes), `list` (`options` array of localized captions,
integer selected `value`, -1 for none), and `image` (`texture` ID).
Items form scrollable rows; optional integer `row` groups items horizontally
into equal-width cells. Supply a label item in the same row to label an input.
Up to eight windows and 64 items per window are allowed. Escape/close hides a
window and releases its input. Failure/unload removes windows and menu entries.

`show` takes `{"handle":N,"visible":true}`. `destroy` takes `{"handle":N}`.
`update` takes `{"handle":N,"window":{...complete creation object...}}`.
An update replaces the window specification; batch updates rather than rebuilding
it every frame. `sdk.ui` carries `handle`, item `id`, and typed `value` for changes
or null for a button. `$open`/`$close` are host-generated visibility actions.
Handles and widget actions cannot address another plugin's windows.

Optional `fonts` contains up to four `{id,name,height,weight}` entries using an
installed Windows font (height 8–72, weight 100–900). Labels/buttons/checkboxes
select them by `font` ID. Fonts are shared in a bounded process cache (128
descriptors; VGUI has no font-destroy API). Optional `textures` contains up to
four `{id,width,height,rgba}` entries, each at most 64×64, with RGBA byte arrays.
Texture uploads count toward the 64 KiB creation/update JSON limit; textures are
released when their window is destroyed. No pointers to VGUI or renderer objects
are exposed. Complex editors, custom font files, and 3D rendering are not covered.

## Services and custom topics

`nextclient.services/register` (during load) takes local `name`, positive integer
`version`, and optional caller `permissions` array. A service's full name becomes
`<plugin-id>/<name>`. `topic` registers a publication topic with the same fields.
Publishing/registering one's own service is permission-free; consuming another
service requires `services.call`, every permission in the provider's contract,
and a compatible enabled provider explicitly listed in the caller's `requires`.
No permissions are inherited from a provider. Providers are responsible for all SDK
actions they perform and should validate methods and payloads as part of their contract.

`lookup`: `{name,version}` returns whether a compatible service/topic exists.
`request`: `{name,version,method,data}` returns a `request` ID, then delivers
`sdk.service` to the provider with that ID, `caller`, `name`, `version`, `method`,
and `data`. `reply`: `{request,data}` may be used once, only by that provider.
The caller receives `sdk.reply` with `request` and `result:{ok:true,data}`.
Pending calls time out after 10 seconds and are cancelled on provider failure or
unload with `result:{ok:false,error}`. There is no synchronous cross-plugin call
or borrowed function pointer, preventing call-stack recursion and dangling DLL
function pointers. At most 1,024 pending requests and 1,024 registrations exist.
Per plugin, limits are 64 pending requests and 32 service/topic registrations.

`subscribe`/`unsubscribe`: `{name,version}` for topics. `publish`: `{name,data}`
is restricted to the topic's owner. Subscribers receive `sdk.topic` with `name`,
`version`, and `data`. Provider-required permissions are checked for both service
requests and topic subscriptions/delivery. Requests, replies, and topics share
the bounded event queues and overflow reporting.

## Approved packages

Use `plugins/<directory>/plugin.dll` as the entry point, companion DLLs directly
beside it, and assets in subdirectories. The embedded manifest is the single
source of metadata. No companion is executed during discovery. Approval hashes
the paths and bytes of **all** files; adding, deleting, or changing an asset or DLL
requires approval again. Approved files remain locked against writes/deletes
while loaded. Packages are limited to 128 files, 64 MiB each, and 256 MiB total.
Reparse points/symlinks and DLLs in subdirectories are rejected.

Imports must be x86 and have simple DLL basenames. Use unique companion basenames
(for example `mycompany_imagecodec_v1.dll`): Windows resolves already-loaded DLLs
by name, so private directories do not give namespace isolation. A conflicting
loaded basename blocks the package. Delay imports, cycles, and imports back into
the entry-point DLL are rejected. The host preloads verified companions in import
order, and permits external imports only from System32 or modules already loaded
by the game. It never changes the process DLL search path. Native plugin code can
still call OS APIs itself; SDK permissions are not a sandbox.

`nextclient.package/read` takes `{"path":"assets/config.json"}` and returns
`bytes` for an already approved file of at most 64 KiB. Paths use `/`, are relative
to the entry DLL, and must exactly match an approved file. There is no arbitrary
filesystem access through this interface. Use the per-plugin store for mutable
data; modifying package files would invalidate approval.
