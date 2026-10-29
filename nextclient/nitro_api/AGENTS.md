# NitroApi

This subtree owns build-specific addresses, stock function signatures and hook registration. The engine and client modules consume those facts through `EngineData` and `ClientData`.

## Extending engine access

- A new exposed function normally touches the public data declaration, `StaticHookId`, module
  registration and the relevant address provider. For a variable, pair its declaration with
  `RegisterVar` and provider data. Check the complete path; a compiling declaration alone does
  not resolve an address at runtime.
- Match the calling convention, return type, pointer depth and layout to the engine ABI. Reuse
  `goldsrc` types instead of recreating a nearby SDK struct. Hook adapters must obey the original
  ABI even when a higher-level wrapper would accept a similar C++ type.
- Keep existing public data-member offsets and static hook IDs stable when extending the API.
  Append data members at the current end, not at a historical insertion point from an old session.
  Check enum values when adding a hook because many IDs are implicit increments.
- Windows and Linux have separate build-8684 address providers. Verify each supported path affected
  by a change; a Windows build cannot validate a Linux signature or address.
- A chained hook's `next` controls execution of the remaining chain. Call it according to the
  intended replacement/forwarding contract and preserve the return value; duplicate forwarding
  can repeat engine work, while an omitted call can suppress unrelated subscribers.

## Headers and subtree history

- Keep stock ABI declarations in [goldsrc_abi](../goldsrc_abi/AGENTS.md) and NitroApi's own API in
  this subtree. Public headers should forward-declare pointer-only dependencies where possible.
- `include/nitroapi/modules/client/GameHud.h` deliberately forward-declares `class Vector`. Including
  `<mathlib/vector.h>` there brings dynamic initialization and tier0 link requirements into Debug
  consumers; include it only in consumers that need the definition.
- The private `dep/easyloggingpp` copy differs from the repository's other copy. Deduplicating it
  changes behavior and requires a separate, explicit change.
- For pre-subtree history, inspect both path spellings:
  `git log -- nextclient/nitro_api/<path> <path>`. Before removing old submodule metadata or storage,
  check other worktrees and refs that still use it. See the
  [maintenance reference](../../docs/goldsrc-abi.md#frozen-extraction-paths) for the frozen extraction paths.

Private borrowed SDL discovery belongs to `nitroapi::sdl2`; hook profiling names belong to `nitroapi::profiling`. Keep helpers and their consumers inside those boundaries without repeating a subsystem prefix in function names.

## Frame lifecycle

`NITRO_FRAME_LIFECYCLE_001` exposes independent numeric subscriptions on the host thread. Subscribers own their IDs and must unsubscribe before destroying observers. Invocation snapshots contain IDs and per-recipient entry/acceptance state; validity is checked before every callback. Entry and acceptance follow registration order; exit follows reverse order after the ordinary hook chain. `FrameLifecycle_ClearObservers()` invalidates all IDs before NitroApi unloads modules, without resetting the ID sequence. Keep `FrameLifecycleInvocation` around the outer `Host_Frame`, with body-depth markers around `Host_FrameInternal`, so engine nonlocal aborts still reach the outer exit.

Each private `FrameLifecycleService` owns its registration IDs, current invocation chain and retained recipient frames. `FrameLifecycleInvocation` receives its service explicitly. Static hook installation binds the stable default service once; factory and shutdown adapters use that same owner. Frame leases survive all reverse-exit callbacks, including reentry after current-invocation restoration. `Clear()` preserves leased storage and the monotonic ID sequence. See [lifecycle ownership](../../docs/frame-lifecycle.md) for the storage and verification boundaries.

The lifecycle interfaces, subscription IDs, invocation owner and helpers belong to `nitroapi`. A subscriber that needs the current outer Host exit must keep its observer alive until that callback completes. The monitor implements this through [deferred detachment](../engine_mini/src/performance/AGENTS.md); the numeric subscription and invalidation rules remain owned here.

## Verification

Build `nitro_api` and the affected consumers after a public API change. `goldsrc_abi_golden` checks 32-bit layouts during the build; it does not verify address-provider matches or live hook order. For a change to public ABI or DLL boundaries, compare the relevant exports and layouts as well as compilation results, and state which runtime behavior remains unverified.
